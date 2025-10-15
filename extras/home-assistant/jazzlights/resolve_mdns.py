#!/usr/bin/env python3
"""Implementation of mDNS resolution using asyncio."""

# This file was originally based on slimDNS by Nicko van Someren.
# https://github.com/nickovs/slimDNS
# slimDNS is licensed under Apache-2.0.

import asyncio
import contextlib
import logging
import socket
import sys
import time
from struct import pack_into, unpack_from

LOGGER = logging.getLogger(__name__)


def check_name(n):
    """Ensure that a name is a list of encoded blocks of bytes.

    Typically starting as a qualified domain name.
    """
    if isinstance(n, str):
        n = n.split(".")
        if n[-1] == "":
            n = n[:-1]
    return [i.encode("UTF8") if isinstance(i, str) else i for i in n]


def skip_name_at(buf, o):
    """Move the offset past the name to which it currently points."""
    while True:
        length = buf[o]
        if length == 0:
            o += 1
            break
        if (length & 0xC0) == 0xC0:
            o += 2
            break
        o += length + 1
    return o


def compare_packed_names(buf, o, packed_name, po=0):
    """Test if two possibly compressed names are equal."""
    while packed_name[po] != 0:
        while buf[o] & 0xC0:
            (o,) = unpack_from("!H", buf, o)
            o &= 0x3FFF
        while packed_name[po] & 0xC0:
            (po,) = unpack_from("!H", packed_name, po)
            po &= 0x3FFF
        l1 = buf[o] + 1
        l2 = packed_name[po] + 1
        if l1 != l2 or buf[o : o + l1] != packed_name[po : po + l2]:
            return False
        o += l1
        po += l2
    return buf[o] == 0


def name_packed_len(name):
    """Find the memory size needed to pack a name without compression."""
    return sum(len(i) + 1 for i in name) + 1


def pack_name(buf, name):
    """Pack a name into the start of the buffer.

    We don't support writing with name compression.
    """
    o = 0
    for part in name:
        pl = len(part)
        buf[o] = pl
        buf[o + 1 : o + pl + 1] = part
        o += pl + 1
    buf[o] = 0


def pack_question(name, qtype, qclass):
    """Pack a question into a new array and return it as a memoryview.

    Return a pre-packed question as a memoryview.
    """
    name = check_name(name)
    name_len = name_packed_len(name)
    buf = bytearray(name_len + 4)
    pack_name(buf, name)
    pack_into("!HH", buf, name_len, qtype, qclass)
    return memoryview(buf)


def skip_question(buf, o):
    """Advance the offset past the question to which it points."""
    o = skip_name_at(buf, o)
    return o + 4


def skip_answer(buf, o):
    """Advance the offset past the answer to which it points."""
    o = skip_name_at(buf, o)
    (rdlen,) = unpack_from("!H", buf, o + 8)
    return o + 10 + rdlen


def compare_q_and_a(q_buf, q_offset, a_buf, a_offset=0):
    """Test if a question matches an answer.

    Note that this also works for comparing a "known answer" in a packet to a
    local answer. The code is asymmetric to the extent that the questions may
    have a type or class of ANY.
    """
    if not compare_packed_names(q_buf, q_offset, a_buf, a_offset):
        return False
    (q_type, q_class) = unpack_from("!HH", q_buf, skip_name_at(q_buf, q_offset))
    (r_type, r_class) = unpack_from("!HH", a_buf, skip_name_at(a_buf, a_offset))
    _TYPE_ANY = 255
    if q_type not in (r_type, _TYPE_ANY):
        return False
    _CLASS_MASK = 0x7FFF
    q_class &= _CLASS_MASK
    r_class &= _CLASS_MASK
    return q_class in (r_class, _TYPE_ANY)


class MulticastDNSProtocol:
    """Implementation of asyncio protocol."""

    def __init__(self, hostname, loop) -> None:
        """Construct object."""
        _CLASS_IN = 1
        _TYPE_A = 1
        self.question = pack_question(hostname, _TYPE_A, _CLASS_IN)
        self.packet_to_send = bytearray(len(self.question) + 12)
        pack_into("!HHHHHH", self.packet_to_send, 0, 1, 0, 1, 0, 0, 0)
        self.packet_to_send[12:] = self.question
        self.answer = []
        self.future_done = loop.create_future()
        self.transport = None

    async def send_query(self, timeout: float | None = None):
        """Start sending mDNS packets."""
        if timeout is not None:
            start_time = time.monotonic()
        while True:
            read_timeout = 1
            if timeout is not None:
                time_left = start_time + timeout - time.monotonic()
                if time_left <= 0:
                    return
                read_timeout = min(read_timeout, time_left)
            LOGGER.debug("Sending %u bytes to multicast", len(self.packet_to_send))
            self.transport.sendto(self.packet_to_send, ("224.0.0.251", 5353))
            with contextlib.suppress(TimeoutError, asyncio.exceptions.CancelledError):
                await asyncio.wait_for(asyncio.shield(self.future_done), read_timeout)
            if len(self.answer) > 0:
                return
        # Timed out waiting for answer, closing the socket.
        self.transport.close()

    def connection_made(self, transport):
        """Underlying socket is ready."""
        self.transport = transport

    def datagram_received(self, buf, addr):
        """DNS packet received."""
        LOGGER.debug("Received %u bytes from %s", len(buf), addr)
        if not self.question or not buf:
            return
        try:
            (_, _, qst_count, ans_count, _, _) = unpack_from("!HHHHHH", buf, 0)
            o = 12
            for _ in range(qst_count):
                o = skip_question(buf, o)
            for _ in range(ans_count):
                if compare_q_and_a(self.question, 0, buf, o):
                    a = buf[o : skip_answer(buf, o)]
                    addr_offset = skip_name_at(a, 0) + 10
                    self.answer.append(a[addr_offset : addr_offset + 4])
                o = skip_answer(buf, o)
        except IndexError:
            LOGGER.error("Received malformed DNS packet")
        if len(self.answer) > 0:
            # Got an answer, closing the socket.
            self.transport.close()
        else:
            LOGGER.error("Ignoring DNS packet without positive answer")

    def error_received(self, exc):
        """Error received on socket."""
        LOGGER.error("Socket received error: %s", exc)
        self.transport.close()

    def connection_lost(self, exc):
        """Transport closed."""
        if not self.future_done.done():
            self.future_done.set_result(True)


async def resolve_mdns_async(hostname: str, timeout: float | None = None) -> str | None:
    """Resolve mDNS asynchronously."""
    loop = asyncio.get_running_loop()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: MulticastDNSProtocol(hostname, loop),
        sock=sock,
    )
    try:
        await protocol.send_query(timeout=timeout)
    finally:
        transport.close()
    if len(protocol.answer) > 0:
        return ".".join(str(i) for i in protocol.answer[0])
    return None


def resolve_mdns_sync(hostname: str, timeout: float | None = 5) -> str | None:
    """Resolve mDNS synchronously."""
    return asyncio.run(resolve_mdns_async(hostname, timeout=timeout))


if __name__ == "__main__":
    hostnames = sys.argv[1:]
    if len(hostnames) == 0:
        hostnames = ["jazzlights-clouds.local", "jazzlights-nothing.local"]
    for host in hostnames:
        address = resolve_mdns_sync(host)
        if address is None:
            LOGGER.error("%s resolution failed", host)
        else:
            LOGGER.error("%s resolved to %s", host, address)
