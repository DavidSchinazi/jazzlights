#!/usr/bin/env python3
"""Client side of the secure channel, see extras/docs/OTA_PROPOSAL.md.

Use SecureChannelClient to talk to a server, and FakeSecureChannelServer to test clients. Both give access to the
lower-level pieces, so that tests can send malformed messages. Each use of the channel has its own script on top of
this module.

Running this script prints the public key from a private key file, formatted for secure_channel_keys.h.
"""

import argparse
import os
import select
import socket
import time

from cryptography.exceptions import InvalidSignature, InvalidTag
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature, encode_dss_signature
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.hkdf import HKDF

DEFAULT_PORT = 6699
MAX_TLV_LENGTH = 16384
PUBLIC_KEY_SIZE = 65
GCM_TAG_SIZE = 16
# How much plaintext we put in a record by default.
DEFAULT_MAX_RECORD_PLAINTEXT = 4096

TOP_CLIENT_HELLO = 0
TOP_SERVER_HELLO = 1
TOP_RECORD = 2

RECORD_CLIENT_READY = 0
RECORD_SERVER_READY = 1
RECORD_CLOSE = 2
FIRST_USE_TYPE = 3

KEYS_LABEL = b"JazzLightsSecureChannel v1 keys\0"
CLIENT_READY_LABEL = b"JazzLightsSecureChannel v1 ClientReady\0"


class ProtocolError(Exception):
    """The peer sent something invalid."""


class ConnectionEnded(Exception):
    """The connection ended without a Close, which means the session failed."""


class SessionClosed(Exception):
    """The peer sent a Close."""

    def __init__(self, status):
        super().__init__("session closed with status %d" % status)
        self.status = status


def encode_varint(value, size=None):
    """Encodes a QUIC varint, in the shortest form unless `size` is given."""
    if size is None:
        size = next(s for s in (1, 2, 4, 8) if value < 1 << (8 * s - 2))
    if value >= 1 << (8 * size - 2):
        raise ValueError("%d doesn't fit in a varint of size %d" % (value, size))
    encoded = bytearray(value.to_bytes(size, "big"))
    encoded[0] |= {1: 0, 2: 1, 4: 2, 8: 3}[size] << 6
    return bytes(encoded)


def decode_varint(data, offset=0):
    """Returns (value, new offset), or None if the varint is cut off."""
    if offset >= len(data):
        return None
    size = 1 << (data[offset] >> 6)
    if offset + size > len(data):
        return None
    value = int.from_bytes(data[offset : offset + size], "big") & ((1 << (8 * size - 2)) - 1)
    return value, offset + size


def encode_tlv(tlv_type, value=b"", type_size=None, length_size=None):
    return encode_varint(tlv_type, type_size) + encode_varint(len(value), length_size) + value


def encode_integer_field(tlv_type, value):
    return encode_tlv(tlv_type, encode_varint(value))


def parse_tlv_header(data, offset=0):
    """Returns (type, length, value offset), or None if the header is cut off."""
    parsed_type = decode_varint(data, offset)
    if parsed_type is None:
        return None
    parsed_length = decode_varint(data, parsed_type[1])
    if parsed_length is None:
        return None
    if parsed_length[0] > MAX_TLV_LENGTH:
        raise ProtocolError("Length %d is over the maximum" % parsed_length[0])
    return parsed_type[0], parsed_length[0], parsed_length[1]


def parse_fields(data):
    """Parses a container into a dictionary from type to Value. Callers ignore the types they don't know about."""
    fields = {}
    offset = 0
    while offset < len(data):
        header = parse_tlv_header(data, offset)
        if header is None or header[2] + header[1] > len(data):
            raise ProtocolError("TLV goes past the end of its container")
        tlv_type, length, value_offset = header
        if tlv_type in fields:
            raise ProtocolError("field %d is repeated" % tlv_type)
        fields[tlv_type] = data[value_offset : value_offset + length]
        offset = value_offset + length
    return fields


def parse_integer(value):
    parsed = decode_varint(value)
    if parsed is None or parsed[1] != len(value):
        raise ProtocolError("integer doesn't fill its Value")
    return parsed[0]


def required_field(fields, tlv_type, size=None):
    if tlv_type not in fields:
        raise ProtocolError("field %d is missing" % tlv_type)
    if size is not None and len(fields[tlv_type]) != size:
        raise ProtocolError("field %d is the wrong size" % tlv_type)
    return fields[tlv_type]


def public_key_bytes(public_key):
    return public_key.public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)


def load_private_key(path):
    with open(os.path.expanduser(path), "rb") as f:
        key = serialization.load_pem_private_key(f.read(), password=None)
    if not isinstance(key, ec.EllipticCurvePrivateKey) or not isinstance(key.curve, ec.SECP256R1):
        raise ValueError("%s isn't a P-256 private key" % path)
    return key


def find_private_key_path(key_argument):
    """Finds the private key from a --key argument or the JL_CLIENT_KEY environment variable."""
    path = key_argument or os.environ.get("JL_CLIENT_KEY")
    if not path:
        raise ValueError("specify the private key with --key or the JL_CLIENT_KEY environment variable")
    return path


def derive_key_material(own_private, peer_public_bytes, client_hello, server_hello):
    """Returns (client key, client IV, server key, server IV)."""
    peer_public = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), peer_public_bytes)
    shared_secret = own_private.exchange(ec.ECDH(), peer_public)
    material = HKDF(
        algorithm=hashes.SHA256(),
        length=56,
        salt=None,  # This is the same as an empty salt.
        info=KEYS_LABEL + client_hello + server_hello,
    ).derive(shared_secret)
    return material[:16], material[16:28], material[28:44], material[44:56]


def sign_client_ready(long_term_private, client_hello, server_hello, public_key_tlv):
    """Returns the raw signature, r || s."""
    signed_data = CLIENT_READY_LABEL + client_hello + server_hello + public_key_tlv
    r, s = decode_dss_signature(long_term_private.sign(signed_data, ec.ECDSA(hashes.SHA256())))
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")


def encode_client_ready_value(long_term_private, client_hello, server_hello):
    public_key_tlv = encode_tlv(0, public_key_bytes(long_term_private.public_key()))
    signature = sign_client_ready(long_term_private, client_hello, server_hello, public_key_tlv)
    return public_key_tlv + encode_tlv(1, signature)


class RecordProtection:
    """The keys and counter for one direction of a session."""

    def __init__(self, key, iv):
        self.aead = AESGCM(key)
        self.iv = iv
        self.counter = 0

    def next_nonce(self):
        nonce = (int.from_bytes(self.iv, "big") ^ self.counter).to_bytes(12, "big")
        self.counter += 1
        return nonce


class _Peer:
    """What both sides of a session have in common: reading top-level TLVs, and the streams of TLVs inside records."""

    def __init__(self, sock):
        self.sock = sock
        self.received = bytearray()
        self.plaintext = bytearray()
        self.send_protection = None
        self.receive_protection = None
        self.top_level_to_skip = 0
        self.max_record_plaintext = DEFAULT_MAX_RECORD_PLAINTEXT

    def send_raw(self, data):
        self.sock.sendall(data)

    def seal_record(self, plaintext, type_size=None, length_size=None):
        """Returns a complete Record, which protects `plaintext` whether or not it's a valid stream of TLVs."""
        value_length = len(plaintext) + GCM_TAG_SIZE
        header = encode_varint(TOP_RECORD, type_size) + encode_varint(value_length, length_size)
        nonce = self.send_protection.next_nonce()
        return header + self.send_protection.aead.encrypt(nonce, bytes(plaintext), header)

    def send_record(self, plaintext):
        self.send_raw(self.seal_record(plaintext))

    def send_stream(self, data):
        """Sends bytes of the stream of TLVs, using as many records as needed."""
        for offset in range(0, len(data), self.max_record_plaintext):
            self.send_record(data[offset : offset + self.max_record_plaintext])

    def send_tlv(self, tlv_type, value=b""):
        self.send_stream(encode_tlv(tlv_type, value))

    def _receive_more(self, deadline):
        timeout = max(0.0, deadline - time.monotonic())
        readable, _, _ = select.select([self.sock], [], [], timeout)
        if not readable:
            raise TimeoutError("timed out waiting for data")
        try:
            data = self.sock.recv(65536)
        except ConnectionError as err:
            raise ConnectionEnded(str(err)) from err
        if not data:
            raise ConnectionEnded("connection ended")
        self.received += data

    def read_top_level(self, deadline):
        """Returns (type, complete TLV, value) for the next known top-level TLV, skipping unknown ones."""
        while True:
            if self.top_level_to_skip > 0:
                skipped = min(self.top_level_to_skip, len(self.received))
                del self.received[:skipped]
                self.top_level_to_skip -= skipped
            header = parse_tlv_header(self.received)
            if header is not None:
                tlv_type, length, value_offset = header
                if tlv_type > TOP_RECORD:
                    self.top_level_to_skip = value_offset + length
                    continue
                if len(self.received) >= value_offset + length:
                    tlv = bytes(self.received[: value_offset + length])
                    del self.received[: value_offset + length]
                    return tlv_type, tlv, tlv[value_offset:]
            self._receive_more(deadline)

    def read_tlv(self, timeout):
        """Returns (type, value) for the next TLV in the stream."""
        deadline = time.monotonic() + timeout
        while True:
            header = parse_tlv_header(self.plaintext)
            if header is not None and len(self.plaintext) >= header[2] + header[1]:
                tlv_type, length, value_offset = header
                value = bytes(self.plaintext[value_offset : value_offset + length])
                del self.plaintext[: value_offset + length]
                return tlv_type, value
            tlv_type, tlv, value = self.read_top_level(deadline)
            if tlv_type != TOP_RECORD:
                raise ProtocolError("top-level message %d out of order" % tlv_type)
            if len(value) <= GCM_TAG_SIZE:
                raise ProtocolError("record has no plaintext")
            header_bytes = tlv[: len(tlv) - len(value)]
            nonce = self.receive_protection.next_nonce()
            try:
                self.plaintext += self.receive_protection.aead.decrypt(nonce, value, header_bytes)
            except InvalidTag as err:
                raise ProtocolError("record failed authentication") from err

    def close_socket(self):
        self.sock.close()


class SecureChannelClient(_Peer):
    """A client session. Call connect() or the individual handshake steps, then send_tlv() and receive()."""

    def __init__(self, host, long_term_private, port=DEFAULT_PORT, timeout=10.0):
        super().__init__(socket.create_connection((host, port), timeout=timeout))
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.timeout = timeout
        self.long_term_private = long_term_private
        self.ephemeral_private = ec.generate_private_key(ec.SECP256R1())
        self.client_hello = None
        self.server_hello = None
        self.server_ready_received = False

    def encode_client_hello(self):
        fields = encode_tlv(0, public_key_bytes(self.ephemeral_private.public_key()))
        return encode_tlv(TOP_CLIENT_HELLO, fields)

    def send_client_hello(self, client_hello=None, used_for_keys=None):
        """Sends a ClientHello, and optionally uses another one for the keys, to simulate tampering."""
        client_hello = client_hello if client_hello is not None else self.encode_client_hello()
        self.client_hello = used_for_keys if used_for_keys is not None else client_hello
        self.send_raw(client_hello)

    def receive_server_hello(self):
        """Reads the ServerHello and derives the session keys."""
        tlv_type, tlv, value = self.read_top_level(time.monotonic() + self.timeout)
        if tlv_type != TOP_SERVER_HELLO:
            raise ProtocolError("expected a ServerHello but got %d" % tlv_type)
        self.server_hello = tlv
        peer_public = required_field(parse_fields(value), 0, PUBLIC_KEY_SIZE)
        client_key, client_iv, server_key, server_iv = derive_key_material(
            self.ephemeral_private, peer_public, self.client_hello, self.server_hello
        )
        self.send_protection = RecordProtection(client_key, client_iv)
        self.receive_protection = RecordProtection(server_key, server_iv)

    def client_ready_tlv(self):
        return encode_tlv(
            RECORD_CLIENT_READY,
            encode_client_ready_value(self.long_term_private, self.client_hello, self.server_hello),
        )

    def wait_for_server_ready(self):
        deadline = time.monotonic() + self.timeout
        while not self.server_ready_received:
            self._receive_one(deadline)

    def connect(self, wait_for_server_ready=True):
        """Performs the whole handshake."""
        self.send_client_hello()
        self.receive_server_hello()
        self.send_stream(self.client_ready_tlv())
        if wait_for_server_ready:
            self.wait_for_server_ready()

    def _receive_one(self, deadline):
        """Reads one TLV. Returns (type, value) if it's for a use of the channel, otherwise handles it."""
        tlv_type, value = self.read_tlv(max(0.0, deadline - time.monotonic()))
        if tlv_type == RECORD_CLIENT_READY:
            raise ProtocolError("ClientReady in the wrong direction")
        if tlv_type == RECORD_SERVER_READY:
            if self.server_ready_received:
                raise ProtocolError("second ServerReady")
            parse_fields(value)
            self.server_ready_received = True
            return None
        if tlv_type == RECORD_CLOSE:
            raise SessionClosed(parse_integer(required_field(parse_fields(value), 0)))
        # Before ServerReady, these get skipped like unknown types.
        return (tlv_type, value) if self.server_ready_received else None

    def receive(self, timeout=None):
        """Returns (type, value) for the next TLV of type 3 and up, handling the channel's own TLVs along the way."""
        deadline = time.monotonic() + (timeout if timeout is not None else self.timeout)
        while True:
            received = self._receive_one(deadline)
            if received is not None:
                return received

    def close(self, status=0):
        """Sends a Close, and closes the connection."""
        try:
            self.send_tlv(RECORD_CLOSE, encode_integer_field(0, status))
        except OSError:
            pass
        self.close_socket()


class FakeSecureChannelServer(_Peer):
    """The server's side of one session, on a connection that was accepted. For testing clients."""

    def __init__(self, sock, timeout=10.0):
        super().__init__(sock)
        self.timeout = timeout
        self.client_public = None

    def handshake(self):
        """Performs the handshake, checking the signature but accepting any client key."""
        tlv_type, client_hello, value = self.read_top_level(time.monotonic() + self.timeout)
        if tlv_type != TOP_CLIENT_HELLO:
            raise ProtocolError("expected a ClientHello but got %d" % tlv_type)
        peer_public = required_field(parse_fields(value), 0, PUBLIC_KEY_SIZE)
        ephemeral = ec.generate_private_key(ec.SECP256R1())
        server_hello = encode_tlv(TOP_SERVER_HELLO, encode_tlv(0, public_key_bytes(ephemeral.public_key())))
        self.send_raw(server_hello)
        client_key, client_iv, server_key, server_iv = derive_key_material(
            ephemeral, peer_public, client_hello, server_hello
        )
        self.send_protection = RecordProtection(server_key, server_iv)
        self.receive_protection = RecordProtection(client_key, client_iv)
        tlv_type, value = self.read_tlv(self.timeout)
        if tlv_type != RECORD_CLIENT_READY:
            raise ProtocolError("expected a ClientReady but got %d" % tlv_type)
        fields = parse_fields(value)
        client_public_bytes = required_field(fields, 0, PUBLIC_KEY_SIZE)
        signature = required_field(fields, 1, 64)
        self.client_public = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), client_public_bytes)
        signed_data = CLIENT_READY_LABEL + client_hello + server_hello + encode_tlv(0, client_public_bytes)
        der_signature = encode_dss_signature(
            int.from_bytes(signature[:32], "big"), int.from_bytes(signature[32:], "big")
        )
        try:
            self.client_public.verify(der_signature, signed_data, ec.ECDSA(hashes.SHA256()))
        except InvalidSignature as err:
            raise ProtocolError("invalid signature") from err
        self.send_tlv(RECORD_SERVER_READY)


def main():
    parser = argparse.ArgumentParser(description="Prints a public key for secure_channel_keys.h.")
    parser.add_argument("--key", help="path to the PEM private key, defaults to the JL_CLIENT_KEY environment variable")
    args = parser.parse_args()
    encoded = public_key_bytes(load_private_key(find_private_key_path(args.key)).public_key())
    print("    {")
    for offset in range(0, len(encoded), 13):
        print("        " + " ".join("0x%02x," % b for b in encoded[offset : offset + 13]))
    print("    },")


if __name__ == "__main__":
    main()
