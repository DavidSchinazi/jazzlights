# This file was adapted from slimDNS by Nicko van Someren
# https://github.com/nickovs/slimDNS
# slimDNS is licensed under Apache-2.0

from select import select
from struct import pack_into, unpack_from
import socket
import time

# The biggest packet we will process
MAX_PACKET_SIZE = 2048

MAX_NAME_SEARCH = 20

# DNS constants

_MDNS_ADDR = "224.0.0.251"
_MDNS_PORT = 5353
_DNS_TTL = 2 * 60  # two minute default TTL

_FLAGS_QR_RESPONSE = 0x8000  # response

_FLAGS_AA = 0x0400  # Authorative answer

_CLASS_IN = 1
_CLASS_ANY = 255
_CLASS_MASK = 0x7FFF
_CLASS_UNIQUE = 0x8000

_TYPE_A = 1
_TYPE_PTR = 12
_TYPE_TXT = 16
_TYPE_AAAA = 28
_TYPE_SRV = 33
_TYPE_ANY = 255


# Convert a dotted IPv4 address string into four bytes, with some
# sanity checks
def dotted_ip_to_bytes(ip):
    l = [int(i) for i in ip.split(".")]
    if len(l) != 4 or any(i < 0 or i > 255 for i in l):
        raise ValueError
    return bytes(l)


# Convert four bytes into a dotted IPv4 address string, without any
# sanity checks
def bytes_to_dotted_ip(a):
    return ".".join(str(i) for i in a)


# Ensure that a name is in the form of a list of encoded blocks of
# bytes, typically starting as a qualified domain name
def check_name(n):
    if isinstance(n, str):
        n = n.split(".")
        if n[-1] == "":
            n = n[:-1]
    n = [i.encode("UTF8") if isinstance(i, str) else i for i in n]
    return n


# Move the offset past the name to which it currently points
def skip_name_at(buf, o):
    while True:
        l = buf[o]
        if l == 0:
            o += 1
            break
        elif (l & 0xC0) == 0xC0:
            o += 2
            break
        else:
            o += l + 1
    return o


# Test if two possibly compressed names are equal
def compare_packed_names(buf, o, packed_name, po=0):
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


# Find the memory size needed to pack a name without compression
def name_packed_len(name):
    return sum(len(i) + 1 for i in name) + 1


# Pack a name into the start of the buffer
def pack_name(buf, name):
    # We don't support writing with name compression, BIWIOMS
    o = 0
    for part in name:
        pl = len(part)
        buf[o] = pl
        buf[o + 1 : o + pl + 1] = part
        o += pl + 1
    buf[o] = 0


# Pack a question into a new array and return it as a memoryview
def pack_question(name, qtype, qclass):
    # Return a pre-packed question as a memoryview
    name = check_name(name)
    name_len = name_packed_len(name)
    buf = bytearray(name_len + 4)
    pack_name(buf, name)
    pack_into("!HH", buf, name_len, qtype, qclass)
    return memoryview(buf)


# Pack an answer into a new array and return it as a memoryview
def pack_answer(name, rtype, rclass, ttl, rdata):
    # Return a pre-packed answer as a memoryview
    name = check_name(name)
    name_len = name_packed_len(name)
    buf = bytearray(name_len + 10 + len(rdata))
    pack_name(buf, name)
    pack_into("!HHIH", buf, name_len, rtype, rclass, ttl, len(rdata))
    buf[name_len + 10 :] = rdata
    return memoryview(buf)


# Advance the offset past the question to which it points
def skip_question(buf, o):
    o = skip_name_at(buf, o)
    return o + 4


# Advance the offset past the answer to which it points
def skip_answer(buf, o):
    o = skip_name_at(buf, o)
    (rdlen,) = unpack_from("!H", buf, o + 8)
    return o + 10 + rdlen


# Test if a questing an answer. Note that this also works for
# comparing a "known answer" in a packet to a local answer. The code
# is asymetric to the extent that the questions my have a type or
# class of ANY
def compare_q_and_a(q_buf, q_offset, a_buf, a_offset=0):
    if not compare_packed_names(q_buf, q_offset, a_buf, a_offset):
        return False
    (q_type, q_class) = unpack_from("!HH", q_buf, skip_name_at(q_buf, q_offset))
    (r_type, r_class) = unpack_from("!HH", a_buf, skip_name_at(a_buf, a_offset))
    if not (q_type == r_type or q_type == _TYPE_ANY):
        return False
    q_class &= _CLASS_MASK
    r_class &= _CLASS_MASK
    return q_class == r_class or q_class == _TYPE_ANY


# The main SlimDNSServer class
class SlimDNSServer:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._reply_buffer = None
        self._pending_question = None
        self.answered = False

    def process_packet(self, buf, addr):
        # Process a single multicast DNS packet

        (pkt_id, flags, qst_count, ans_count, _, _) = unpack_from("!HHHHHH", buf, 0)
        o = 12
        for _ in range(qst_count):
            o = skip_question(buf, o)

        # In theory we could do known answer suppression here
        # We don't, BIWIOMS

        if self._pending_question:
            for i in range(ans_count):
                if compare_q_and_a(self._pending_question, 0, buf, o):
                    if self._answer_callback(buf[o : skip_answer(buf, o)]):
                        self.answered = True
                o = skip_answer(buf, o)

    def process_waiting_packets(self):
        # Handle all the packets that can be read immediately and
        # return as soon as none are waiting
        while True:
            readers, _, _ = select([self.sock], [], [], 0)
            if not readers:
                break
            buf, addr = self.sock.recvfrom(MAX_PACKET_SIZE)
            print("Received {} bytes from {}".format(len(buf), addr))
            if buf:
                try:
                    self.process_packet(memoryview(buf), addr)
                except IndexError:
                    print("Index error processing packet; probably malformed data")
                except Exception as e:
                    print("Error processing packet: {}".format(e))
                    # raise e

    def handle_question(self, q, answer_callback, retry_count=3):
        # Send our a (packed) question, and send matching replies to
        # the answer_callback function.  This will stop after sending
        # the given number of retries and waiting for the a timeout on
        # each, or sooner if the answer_callback function returns True
        p = bytearray(len(q) + 12)
        pack_into("!HHHHHH", p, 0, 1, 0, 1, 0, 0, 0)
        p[12:] = q

        self._pending_question = q
        self._answer_callback = answer_callback
        self.answered = False

        try:
            for _ in range(retry_count):
                if self.answered:
                    break
                self.sock.sendto(p, (_MDNS_ADDR, _MDNS_PORT))
                timeout = time.monotonic() + 1.0
                while not self.answered:
                    select_time = timeout - time.monotonic()
                    if select_time <= 0:
                        break
                    (rr, _, _) = select([self.sock], [], [], select_time / 1000.0)
                    if rr:
                        self.process_waiting_packets()
        finally:
            self._pending_question = None
            self._answer_callback = None

    def resolve_mdns_address(self, hostname):
        # Look up an IPv4 address for a hostname using mDNS.
        q = pack_question(hostname, _TYPE_A, _CLASS_IN)
        answer = []

        def _answer_handler(a):
            addr_offset = skip_name_at(a, 0) + 10
            answer.append(a[addr_offset : addr_offset + 4])
            return True

        self.handle_question(q, _answer_handler)
        return bytes(answer[0]) if answer else None


def print_resolution_for(host):
    server = SlimDNSServer()
    address = server.resolve_mdns_address(host)
    if address is None:
        print("{} resolution failed".format(host))
    else:
        print("{} resolved to {}".format(host, bytes_to_dotted_ip(address)))


if __name__ == "__main__":
    print_resolution_for("jazzlights-clouds.local")
    print_resolution_for("jazzlights-nothing.local")
