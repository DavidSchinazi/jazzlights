#!/usr/bin/env python3
"""Tests the secure channel and OTA, see "Testing" in extras/docs/OTA_PROPOSAL.md.

By default this runs against the native server from extras/secure_channel_test, which it expects to already be built.
That fakes the flash but otherwise runs the same code as the firmware, under ASan and UBSan.

To test a real device instead, flash it with the vest_secure_channel_test environment, with the public keys of two
private keys added to secure_channel_keys.h, and run this with JL_TEST_HOST set to its address and JL_TEST_KEY and
JL_TEST_KEY2 set to the paths of the private keys. Tests that need the fake flash get skipped, so the remaining OTA
tests on hardware still need to be done by hand.
"""

import builtins
import hashlib
import os
import re
import shutil
import socket
import subprocess
import tempfile
import threading
import time
import unittest
from unittest import mock

from cryptography.hazmat.primitives.asymmetric import ec

import ota_upload
import secure_channel as sc

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SERVER_BINARY = os.path.join(REPO_ROOT, "extras/secure_channel_test/build/secure-channel-test-server")
TEST_HOST = os.environ.get("JL_TEST_HOST")
NATIVE = TEST_HOST is None
HOST = TEST_HOST or "127.0.0.1"
ECHO = 1000
UNKNOWN_TYPE = 2000
RESTART_EXIT_CODE = 42
ROLLBACK_EXIT_CODE = 43
# Faster than the server's timeouts, slow enough for anything to arrive.
CLOSE_TIMEOUT_SECONDS = 5 if NATIVE else 15
NATIVE_SOFTWARE_VERSION = bytes(range(20))

if NATIVE:
    KEY = ec.generate_private_key(ec.SECP256R1())
    KEY2 = ec.generate_private_key(ec.SECP256R1())
else:
    KEY = sc.load_private_key(os.environ["JL_TEST_KEY"])
    KEY2 = sc.load_private_key(os.environ["JL_TEST_KEY2"])
UNKNOWN_KEY = ec.generate_private_key(ec.SECP256R1())


def make_image(size=10000, chip_id=0, project=b"jazzlights"):
    image = bytearray(os.urandom(size))
    image[0] = 0xe9
    image[12:14] = chip_id.to_bytes(2, "little")
    image[80:112] = project.ljust(32, b"\0")
    return bytes(image)


class ServerTestCase(unittest.TestCase):
    server_args = ()

    def setUp(self):
        self.clients = []
        self.image_out = None
        self.expect_restart = False
        self.restart_exit_code = RESTART_EXIT_CODE
        self.server = None
        if not NATIVE:
            return
        self.temp_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.temp_dir)
        self.image_out = os.path.join(self.temp_dir, "image.bin")
        keys = [sc.public_key_bytes(k.public_key()).hex() for k in (KEY, KEY2)]
        self.log_path = os.path.join(self.temp_dir, "server.log")
        self.log_file = open(self.log_path, "w", encoding="utf-8")  # pylint: disable=consider-using-with
        self.addCleanup(self.log_file.close)
        self.server = subprocess.Popen(  # pylint: disable=consider-using-with
            [SERVER_BINARY, "--image-out", self.image_out, *self.server_args, *keys],
            stdout=self.log_file,
            stderr=subprocess.STDOUT,
        )
        self.addCleanup(self.stop_server)
        self.wait_for_server()

    def stop_server(self):
        if self.server.poll() is None:
            self.server.kill()
            self.server.wait()

    def wait_for_server(self):
        for _ in range(100):
            try:
                socket.create_connection((HOST, sc.DEFAULT_PORT), timeout=1).close()
                return
            except OSError:
                time.sleep(0.05)
        self.fail("server didn't start")

    def tearDown(self):
        for client in self.clients:
            client.close_socket()
        if self.server is None:
            return
        if self.expect_restart:
            self.server.wait(timeout=5)
        code = self.server.poll()
        if code is not None and not (self.expect_restart and code == self.restart_exit_code):
            with open(self.log_path, encoding="utf-8") as f:
                self.fail("server exited with code %s:\n%s" % (code, f.read()))

    def new_client(self, key=KEY):
        client = sc.SecureChannelClient(HOST, key)
        self.clients.append(client)
        return client

    def connected_client(self, key=KEY):
        client = self.new_client(key)
        client.connect()
        return client

    def raw_socket(self):
        sock = socket.create_connection((HOST, sc.DEFAULT_PORT), timeout=5)
        self.addCleanup(sock.close)
        return sock

    def assert_echo_works(self, client, value=b"hello"):
        client.send_tlv(ECHO, value)
        self.assertEqual(client.receive(), (ECHO, value))

    def assert_closed_without_close(self, client):
        """The connection must end, with nothing sent before that, and in particular no ServerReady."""
        with self.assertRaises(sc.ConnectionEnded):
            client.receive(timeout=CLOSE_TIMEOUT_SECONDS)
        self.assertFalse(client.server_ready_received)

    def assert_session_failed(self, client):
        """The connection must end, without anything else being delivered."""
        with self.assertRaises(sc.ConnectionEnded):
            client.receive(timeout=CLOSE_TIMEOUT_SECONDS)

    def assert_socket_closed(self, sock):
        sock.settimeout(CLOSE_TIMEOUT_SECONDS)
        try:
            while sock.recv(4096):
                pass
        except ConnectionError:
            pass


class HandshakeTest(ServerTestCase):
    def test_happy_path(self):
        self.assert_echo_works(self.connected_client())

    def test_second_key(self):
        self.assert_echo_works(self.connected_client(KEY2))

    def test_sizes_match_documentation(self):
        client = self.new_client()
        self.assertEqual(len(client.encode_client_hello()), 71)
        client.connect()
        self.assertEqual(len(client.server_hello), 71)
        self.assertEqual(len(client.seal_record(client.client_ready_tlv())), 157)
        self.assertEqual(len(client.seal_record(sc.encode_tlv(sc.RECORD_SERVER_READY))), 20)

    def test_public_key_not_in_firmware(self):
        client = self.new_client(UNKNOWN_KEY)
        client.connect(wait_for_server_ready=False)
        self.assert_closed_without_close(client)

    def test_wrong_private_key(self):
        client = self.new_client()
        client.send_client_hello()
        client.receive_server_hello()
        public_key_tlv = sc.encode_tlv(0, sc.public_key_bytes(KEY.public_key()))
        signature = sc.sign_client_ready(UNKNOWN_KEY, client.client_hello, client.server_hello, public_key_tlv)
        client.send_tlv(sc.RECORD_CLIENT_READY, public_key_tlv + sc.encode_tlv(1, signature))
        self.assert_closed_without_close(client)

    def test_client_public_key_changed_after_signing(self):
        client = self.new_client()
        client.send_client_hello()
        client.receive_server_hello()
        signed_tlv = sc.encode_tlv(0, sc.public_key_bytes(KEY.public_key()))
        signature = sc.sign_client_ready(KEY, client.client_hello, client.server_hello, signed_tlv)
        changed_tlv = sc.encode_tlv(0, sc.public_key_bytes(KEY2.public_key()))
        client.send_tlv(sc.RECORD_CLIENT_READY, changed_tlv + sc.encode_tlv(1, signature))
        self.assert_closed_without_close(client)

    def test_replay(self):
        first = self.new_client()
        hello = first.encode_client_hello()
        first.send_client_hello(hello)
        first.receive_server_hello()
        client_ready = first.seal_record(first.client_ready_tlv())
        first.send_raw(client_ready)
        first.wait_for_server_ready()
        replayed = self.new_client()
        replayed.send_raw(hello + client_ready)
        with self.assertRaises(sc.ConnectionEnded):
            # This reads the ServerHello that the replay can't do anything with.
            replayed.sock.settimeout(CLOSE_TIMEOUT_SECONDS)
            while True:
                if not replayed.sock.recv(4096):
                    raise sc.ConnectionEnded()

    def test_client_hello_not_on_curve(self):
        client = self.new_client()
        point = sc.public_key_bytes(client.ephemeral_private.public_key())
        client.send_client_hello(sc.encode_tlv(sc.TOP_CLIENT_HELLO, sc.encode_tlv(0, point[:-1] + bytes([point[-1] ^ 1]))))
        self.assert_socket_closed(client.sock)

    def test_client_hello_field_errors(self):
        point = sc.public_key_bytes(ec.generate_private_key(ec.SECP256R1()).public_key())
        field = sc.encode_tlv(0, point)
        for value in (b"", field + field, sc.encode_tlv(0, point[:-1]), sc.encode_tlv(1, point), field + b"\x05"):
            with self.subTest(value=value.hex()):
                client = self.new_client()
                client.send_raw(sc.encode_tlv(sc.TOP_CLIENT_HELLO, value))
                self.assert_socket_closed(client.sock)

    def test_client_ready_field_errors(self):
        for name in ("missing_key", "missing_signature", "repeated_key", "short_key", "short_signature", "cut_off"):
            with self.subTest(name=name):
                client = self.new_client()
                client.send_client_hello()
                client.receive_server_hello()
                public_key = sc.public_key_bytes(KEY.public_key())
                key_tlv = sc.encode_tlv(0, public_key)
                signature_tlv = sc.encode_tlv(
                    1, sc.sign_client_ready(KEY, client.client_hello, client.server_hello, key_tlv)
                )
                value = {
                    "missing_key": signature_tlv,
                    "missing_signature": key_tlv,
                    "repeated_key": key_tlv + key_tlv + signature_tlv,
                    "short_key": sc.encode_tlv(0, public_key[:-1]) + signature_tlv,
                    "short_signature": key_tlv + signature_tlv[:-1],
                    "cut_off": key_tlv + signature_tlv + b"\x05",
                }[name]
                client.send_tlv(sc.RECORD_CLIENT_READY, value)
                self.assert_closed_without_close(client)

    def test_tampered_skipped_hello_field(self):
        client = self.new_client()
        point = sc.encode_tlv(0, sc.public_key_bytes(client.ephemeral_private.public_key()))
        sent = sc.encode_tlv(sc.TOP_CLIENT_HELLO, point + sc.encode_tlv(50, b"tampered"))
        original = sc.encode_tlv(sc.TOP_CLIENT_HELLO, point + sc.encode_tlv(50, b"original"))
        client.send_client_hello(sent, used_for_keys=original)
        client.receive_server_hello()
        client.send_stream(client.client_ready_tlv())
        self.assert_closed_without_close(client)

    def test_tampered_reencoded_hello_varint(self):
        client = self.new_client()
        fields = sc.encode_tlv(0, sc.public_key_bytes(client.ephemeral_private.public_key()))
        sent = sc.encode_tlv(sc.TOP_CLIENT_HELLO, fields, type_size=4, length_size=8)
        client.send_client_hello(sent, used_for_keys=sc.encode_tlv(sc.TOP_CLIENT_HELLO, fields))
        client.receive_server_hello()
        client.send_stream(client.client_ready_tlv())
        self.assert_closed_without_close(client)

    def test_hello_with_longer_varints_and_skipped_fields(self):
        client = self.new_client()
        point = sc.encode_tlv(0, sc.public_key_bytes(client.ephemeral_private.public_key()), type_size=8, length_size=8)
        fields = sc.encode_tlv(50, b"skipped") + point + sc.encode_tlv(51)
        client.send_client_hello(sc.encode_tlv(sc.TOP_CLIENT_HELLO, fields, type_size=2, length_size=4))
        client.receive_server_hello()
        client.send_stream(client.client_ready_tlv())
        client.wait_for_server_ready()
        self.assert_echo_works(client)

    def test_client_ready_with_longer_varints(self):
        client = self.new_client()
        client.send_client_hello()
        client.receive_server_hello()
        key_tlv = sc.encode_tlv(0, sc.public_key_bytes(KEY.public_key()), type_size=4, length_size=8)
        signature = sc.sign_client_ready(KEY, client.client_hello, client.server_hello, key_tlv)
        value = sc.encode_tlv(1, signature) + sc.encode_tlv(99, b"skipped") + key_tlv
        client.send_stream(sc.encode_tlv(sc.RECORD_CLIENT_READY, value, type_size=8, length_size=2))
        client.wait_for_server_ready()
        self.assert_echo_works(client)


class TopLevelTest(ServerTestCase):
    def test_server_hello_from_client(self):
        client = self.new_client()
        client.send_raw(sc.encode_tlv(sc.TOP_SERVER_HELLO, b""))
        self.assert_socket_closed(client.sock)

    def test_record_before_client_hello(self):
        client = self.new_client()
        client.send_raw(sc.encode_tlv(sc.TOP_RECORD, bytes(32)))
        self.assert_socket_closed(client.sock)

    def test_second_client_hello(self):
        client = self.new_client()
        hello = client.encode_client_hello()
        client.send_client_hello(hello)
        client.receive_server_hello()
        client.send_raw(hello)
        self.assert_session_failed(client)

    def test_client_hello_during_session(self):
        client = self.connected_client()
        client.send_raw(client.encode_client_hello())
        self.assert_session_failed(client)

    def test_unknown_skipped_at_every_stage(self):
        injected = sc.encode_tlv(77, b"x" * 100) + sc.encode_tlv(78)
        client = self.new_client()
        client.send_raw(injected)
        client.send_client_hello()
        client.send_raw(injected)
        client.receive_server_hello()
        client.send_raw(injected)
        client.send_stream(client.client_ready_tlv())
        client.send_raw(injected)
        client.wait_for_server_ready()
        client.send_raw(injected)
        self.assert_echo_works(client)

    def test_length_limits(self):
        client = self.new_client()
        client.send_raw(sc.encode_tlv(77, bytes(sc.MAX_LENGTH)))
        client.send_client_hello()
        client.receive_server_hello()
        client.send_raw(sc.encode_varint(77) + sc.encode_varint(sc.MAX_LENGTH + 1))
        self.assert_session_failed(client)

    def test_unknown_top_level_tlv_cant_be_as_large_as_a_client_hello(self):
        client = self.new_client()
        client.send_raw(sc.encode_varint(77) + sc.encode_varint(sc.MAX_CLIENT_HELLO_LENGTH))
        self.assert_socket_closed(client.sock)

    def test_huge_varint_length(self):
        client = self.new_client()
        client.send_raw(sc.encode_varint(77) + sc.encode_varint(2**62 - 1))
        self.assert_socket_closed(client.sock)

    def test_client_hello_of_maximum_size(self):
        client = self.new_client()
        point = sc.encode_tlv(0, sc.public_key_bytes(client.ephemeral_private.public_key()))
        padding = sc.encode_tlv(5, bytes(sc.MAX_CLIENT_HELLO_LENGTH - len(point) - 3))
        hello = sc.encode_tlv(sc.TOP_CLIENT_HELLO, point + padding)
        self.assertEqual(sc.parse_tlv_header(hello)[1], sc.MAX_CLIENT_HELLO_LENGTH)
        client.send_client_hello(hello)
        client.receive_server_hello()
        client.send_stream(client.client_ready_tlv())
        client.wait_for_server_ready()
        self.assert_echo_works(client)

    def test_client_hello_over_maximum_size(self):
        client = self.new_client()
        client.send_raw(sc.encode_varint(sc.TOP_CLIENT_HELLO) + sc.encode_varint(sc.MAX_CLIENT_HELLO_LENGTH + 1))
        self.assert_socket_closed(client.sock)


class RecordTest(ServerTestCase):
    def test_record_of_maximum_length(self):
        client = self.connected_client()
        # The echo's Type and this Length both take two bytes.
        value = bytes(sc.MAX_LENGTH - sc.GCM_TAG_SIZE - 4)
        record = client.seal_record(sc.encode_tlv(ECHO, value))
        self.assertEqual(sc.parse_tlv_header(record)[1], sc.MAX_LENGTH)
        client.send_raw(record)
        self.assertEqual(client.receive(), (ECHO, value))

    def test_record_over_maximum_length(self):
        client = self.connected_client()
        client.send_raw(sc.encode_varint(sc.TOP_RECORD) + sc.encode_varint(sc.MAX_LENGTH + 1))
        self.assert_session_failed(client)

    def test_flipped_bit(self):
        client = self.connected_client()
        record = bytearray(client.seal_record(sc.encode_tlv(ECHO, b"hello")))
        record[-1] ^= 1
        client.send_raw(record)
        self.assert_session_failed(client)

    def test_flipped_bit_in_last_record_of_spanning_tlv(self):
        client = self.connected_client()
        tlv = sc.encode_tlv(ECHO, b"hello world")
        client.send_record(tlv[:5])
        record = bytearray(client.seal_record(tlv[5:]))
        record[-1] ^= 1
        client.send_raw(record)
        self.assert_session_failed(client)

    def test_record_header_reencoded(self):
        for sizes in ((2, None), (None, 4)):
            with self.subTest(sizes=sizes):
                client = self.connected_client()
                sealed = client.seal_record(sc.encode_tlv(ECHO, b"hello"))
                value = sc.parse_tlv_header(sealed)
                client.send_raw(
                    sc.encode_tlv(sc.TOP_RECORD, sealed[value[2] :], type_size=sizes[0], length_size=sizes[1])
                )
                self.assert_session_failed(client)

    def test_record_with_longer_header_when_authenticated_that_way(self):
        client = self.connected_client()
        client.send_raw(client.seal_record(sc.encode_tlv(ECHO, b"hello"), type_size=8, length_size=8))
        self.assertEqual(client.receive(), (ECHO, b"hello"))

    def test_no_plaintext(self):
        client = self.connected_client()
        client.send_raw(client.seal_record(b""))
        self.assert_session_failed(client)

    def test_shorter_than_tag(self):
        client = self.connected_client()
        client.send_raw(sc.encode_tlv(sc.TOP_RECORD, bytes(5)))
        self.assert_session_failed(client)

    def test_replayed_record(self):
        client = self.connected_client()
        record = client.seal_record(sc.encode_tlv(ECHO, b"hello"))
        client.send_raw(record)
        self.assertEqual(client.receive(), (ECHO, b"hello"))
        client.send_raw(record)
        self.assert_session_failed(client)

    def test_several_tlvs_in_one_record(self):
        client = self.connected_client()
        client.send_record(sc.encode_tlv(ECHO, b"one") + sc.encode_tlv(UNKNOWN_TYPE, b"skip") + sc.encode_tlv(ECHO, b"two"))
        self.assertEqual(client.receive(), (ECHO, b"one"))
        self.assertEqual(client.receive(), (ECHO, b"two"))

    def test_tlv_split_at_every_byte(self):
        client = self.connected_client()
        tlv = sc.encode_tlv(ECHO, b"hello world", type_size=4, length_size=4)
        for split in range(1, len(tlv)):
            client.send_record(tlv[:split])
            client.send_record(tlv[split:])
            self.assertEqual(client.receive(), (ECHO, b"hello world"))
        for size in (1, 2, 3):
            for offset in range(0, len(tlv), size):
                client.send_record(tlv[offset : offset + size])
            self.assertEqual(client.receive(), (ECHO, b"hello world"))

    def test_top_level_arrives_one_byte_at_a_time(self):
        client = self.connected_client()
        record = client.seal_record(sc.encode_tlv(ECHO, b"hello"))
        for byte in record:
            client.send_raw(bytes([byte]))
            time.sleep(0.002)
        self.assertEqual(client.receive(), (ECHO, b"hello"))

    def test_largest_tlvs(self):
        client = self.connected_client()
        value = os.urandom(sc.MAX_LENGTH)
        # In the largest possible records, so the Value of each TLV is split over two of them. Each echo is read before
        # sending the next, as a device's TCP buffers are too small to hold a whole echo while we're still sending.
        self.assertEqual(client.max_record_plaintext, sc.MAX_LENGTH - sc.GCM_TAG_SIZE)
        for _ in range(3):
            client.send_tlv(ECHO, value)
            self.assertEqual(client.receive(), (ECHO, value))

    def test_nonminimal_tlv_of_maximum_size(self):
        client = self.connected_client()
        value = bytes(sc.MAX_LENGTH)
        client.send_stream(sc.encode_tlv(ECHO, value, type_size=8, length_size=8))
        self.assertEqual(client.receive(), (ECHO, value))

    def test_tlv_over_maximum_length(self):
        client = self.connected_client()
        client.send_stream(sc.encode_varint(ECHO) + sc.encode_varint(sc.MAX_LENGTH + 1))
        self.assert_session_failed(client)

    def test_unknown_tlv_of_maximum_length_spanning_records(self):
        client = self.connected_client()
        client.send_stream(sc.encode_tlv(UNKNOWN_TYPE, bytes(sc.MAX_LENGTH)))
        self.assert_echo_works(client)

    def test_unknown_tlv_over_maximum_length(self):
        client = self.connected_client()
        client.send_stream(sc.encode_varint(UNKNOWN_TYPE) + sc.encode_varint(sc.MAX_LENGTH + 1))
        self.assert_session_failed(client)

    def test_stream_cut_short(self):
        client = self.connected_client()
        client.send_record(sc.encode_tlv(ECHO, b"hello")[:4])
        client.close_socket()
        self.assert_echo_works(self.connected_client())

    def test_close(self):
        client = self.connected_client()
        client.close()
        self.assert_echo_works(self.connected_client())

    def test_anything_after_close(self):
        client = self.connected_client()
        client.send_record(sc.encode_tlv(sc.RECORD_CLOSE, sc.encode_integer_field(0, 0)) + sc.encode_tlv(ECHO, b"no"))
        self.assert_session_failed(client)

    def test_close_missing_status(self):
        client = self.connected_client()
        client.send_tlv(sc.RECORD_CLOSE)
        self.assert_session_failed(client)


class ReadyMessageTest(ServerTestCase):
    def handshake_until_client_ready(self):
        client = self.new_client()
        client.send_client_hello()
        client.receive_server_hello()
        return client

    def test_split_across_records(self):
        for split in (1, 2, 3, 50, 137):
            with self.subTest(split=split):
                client = self.handshake_until_client_ready()
                tlv = client.client_ready_tlv()
                client.send_record(tlv[:split])
                client.send_record(tlv[split:])
                client.wait_for_server_ready()
                self.assert_echo_works(client)
                # Make room, since there are more of these than the server allows connections.
                client.close()

    def test_sharing_a_record_with_other_tlvs(self):
        client = self.handshake_until_client_ready()
        client.send_record(client.client_ready_tlv() + sc.encode_tlv(ECHO, b"right behind"))
        self.assertEqual(client.receive(), (ECHO, b"right behind"))
        self.assertTrue(client.server_ready_received)

    def test_tlvs_behind_client_ready_with_bad_signature(self):
        client = self.handshake_until_client_ready()
        key_tlv = sc.encode_tlv(0, sc.public_key_bytes(KEY.public_key()))
        value = key_tlv + sc.encode_tlv(1, bytes(64))
        client.send_record(sc.encode_tlv(sc.RECORD_CLIENT_READY, value) + sc.encode_tlv(ECHO, b"never"))
        self.assert_closed_without_close(client)

    def test_tlv_in_next_record_behind_client_ready(self):
        client = self.handshake_until_client_ready()
        client.send_stream(client.client_ready_tlv())
        client.send_tlv(ECHO, b"right behind")
        self.assertEqual(client.receive(), (ECHO, b"right behind"))

    def test_repeated(self):
        client = self.handshake_until_client_ready()
        tlv = client.client_ready_tlv()
        client.send_stream(tlv)
        client.wait_for_server_ready()
        client.send_stream(tlv)
        self.assert_session_failed(client)

    def test_server_ready_from_client(self):
        for after_handshake in (False, True):
            with self.subTest(after_handshake=after_handshake):
                client = self.handshake_until_client_ready()
                if after_handshake:
                    client.send_stream(client.client_ready_tlv())
                    client.wait_for_server_ready()
                client.send_tlv(sc.RECORD_SERVER_READY)
                self.assert_session_failed(client)

    def test_after_unknown_tlv(self):
        client = self.handshake_until_client_ready()
        client.send_tlv(UNKNOWN_TYPE, b"skipped")
        client.send_stream(client.client_ready_tlv())
        client.wait_for_server_ready()
        self.assert_echo_works(client)

    def test_tlv_of_type_3_or_up_before_client_ready_is_skipped(self):
        client = self.handshake_until_client_ready()
        client.send_tlv(ECHO, bytes(sc.MAX_LENGTH))
        client.send_tlv(ECHO, b"must not be handled")
        client.send_tlv(ota_upload.OTA_QUERY)
        client.send_stream(client.client_ready_tlv())
        client.send_tlv(ECHO, b"after")
        self.assertEqual(client.receive(), (ECHO, b"after"))

    def test_ota_begin_before_client_ready_is_skipped(self):
        client = self.handshake_until_client_ready()
        client.send_tlv(ota_upload.OTA_BEGIN, ota_upload.encode_ota_begin(make_image()))
        client.send_stream(client.client_ready_tlv())
        # If it had been handled, the first of these would be an acknowledgment, and this session would be updating.
        client.send_tlv(ota_upload.OTA_END)
        tlv_type, value = client.receive()
        self.assertEqual(tlv_type, ota_upload.OTA_RESULT)
        self.assertEqual(ota_upload.result_status(value), 6)

    def test_client_ready_of_maximum_size(self):
        client = self.handshake_until_client_ready()
        value = sc.encode_client_ready_value(KEY, client.client_hello, client.server_hello)
        value += sc.encode_tlv(5, bytes(sc.MAX_LENGTH - len(value) - 3))
        self.assertEqual(len(value), sc.MAX_LENGTH)
        client.send_tlv(sc.RECORD_CLIENT_READY, value)
        client.wait_for_server_ready()
        self.assert_echo_works(client)

    def test_close_before_client_ready(self):
        client = self.handshake_until_client_ready()
        client.send_tlv(sc.RECORD_CLOSE, sc.encode_integer_field(0, 0))
        self.assert_session_failed(client)


# How many connections the server allows at once.
MAX_CONNECTIONS = 4


class SessionTest(ServerTestCase):
    def test_maximum_clients_at_once(self):
        clients = [self.connected_client() for _ in range(MAX_CONNECTIONS)]
        for round_number in range(3):
            for index, client in enumerate(clients):
                client.send_tlv(ECHO, b"%d-%d" % (round_number, index))
            for index, client in enumerate(clients):
                self.assertEqual(client.receive(), (ECHO, b"%d-%d" % (round_number, index)))

    def test_one_failing_while_others_carry_on(self):
        clients = [self.connected_client() for _ in range(3)]
        record = bytearray(clients[1].seal_record(sc.encode_tlv(ECHO, b"hello")))
        record[-1] ^= 1
        clients[1].send_raw(record)
        self.assert_session_failed(clients[1])
        self.assert_echo_works(clients[0])
        self.assert_echo_works(clients[2])

    def test_full_of_sessions_closes_new_connections(self):
        clients = [self.connected_client() for _ in range(MAX_CONNECTIONS)]
        extra = self.raw_socket()
        self.assert_socket_closed(extra)
        for client in clients:
            self.assert_echo_works(client)
        clients[0].close()
        time.sleep(0.2)
        self.assert_echo_works(self.connected_client())

    def test_full_of_handshakes_replaces_oldest(self):
        handshakes = []
        for _ in range(MAX_CONNECTIONS):
            handshakes.append(self.raw_socket())
            time.sleep(0.05)
        client = self.connected_client()
        self.assert_echo_works(client)
        self.assert_socket_closed(handshakes[0])
        # The others are still there.
        handshakes[1].setblocking(False)
        with self.assertRaises(BlockingIOError):
            handshakes[1].recv(1)

    def test_handshakes_never_replace_sessions(self):
        clients = [self.connected_client() for _ in range(MAX_CONNECTIONS - 1)]
        for _ in range(10):
            self.raw_socket()
            time.sleep(0.02)
        for client in clients:
            self.assert_echo_works(client)

    def test_flood_of_new_connections(self):
        client = self.connected_client()
        for _ in range(40):
            sock = socket.create_connection((HOST, sc.DEFAULT_PORT), timeout=5)
            sock.close()
        self.assert_echo_works(client)

    def watch_timeouts(self, started, watched, injected_into, active, expected):
        """Waits for each watched socket to be closed, while sending injected TLVs into some of them and keeping
        `active` alive, and checks that each closed `expected` seconds after `started`."""
        injected = sc.encode_tlv(77, b"x")
        start = time.monotonic()
        last_active = start
        ended = {}

        def is_closed(sock):
            sock.setblocking(False)
            try:
                return sock.recv(4096) == b""
            except BlockingIOError:
                return False
            except ConnectionError:
                return True

        while len(ended) < len(watched) and time.monotonic() - start < expected + 15:
            time.sleep(0.5)
            for sock in injected_into:
                try:
                    sock.sendall(injected)
                except OSError:
                    pass
            if time.monotonic() - last_active >= 20:
                self.assert_echo_works(active)
                last_active = time.monotonic()
            for name, sock in watched.items():
                if name not in ended and is_closed(sock):
                    ended[name] = time.monotonic() - started[name]
        self.assertEqual(set(ended), set(watched))
        for name, duration in ended.items():
            self.assertAlmostEqual(duration, expected, delta=2.5, msg=name)
        self.assert_echo_works(active)

    def test_handshake_timeouts(self):
        # Each timeout runs from when that connection was accepted. That's when each of these is set up, which on
        # hardware is about a second apart because of the handshakes. These and `active` take all 4 connections.
        started = {"silent": time.monotonic()}
        silent = self.raw_socket()
        started["flooding_before_hello"] = time.monotonic()
        flooding_before_hello = self.raw_socket()
        started["before_ready"] = time.monotonic()
        before_ready = self.new_client()
        before_ready.send_client_hello()
        before_ready.receive_server_hello()
        active = self.connected_client()
        watched = {
            "silent": silent,
            "flooding_before_hello": flooding_before_hello,
            "before_ready": before_ready.sock,
        }
        self.watch_timeouts(started, watched, (flooding_before_hello, before_ready.sock), active, expected=10)

    @unittest.skipIf(os.environ.get("JL_TEST_SKIP_SLOW"), "this takes over a minute")
    def test_idle_timeouts(self):
        # Each timeout runs from when that session last sent a record.
        idle = self.connected_client()
        started = {"idle": time.monotonic()}
        idle_with_injected = self.connected_client()
        started["idle_with_injected"] = time.monotonic()
        active = self.connected_client()
        watched = {"idle": idle.sock, "idle_with_injected": idle_with_injected.sock}
        self.watch_timeouts(started, watched, (idle_with_injected.sock,), active, expected=60)


def begin_update(client, image, reboot_when_done=False):
    client.send_tlv(ota_upload.OTA_BEGIN, ota_upload.encode_ota_begin(image, reboot_when_done))


def ota_ack(value):
    return sc.parse_integer(sc.required_field(sc.parse_fields(value), 0))


def send_image(client, image):
    """Sends the image, one piece at a time, and expects an acknowledgment after each."""
    written = 0
    for offset in range(0, len(image), ota_upload.DATA_CHUNK_SIZE):
        chunk = image[offset : offset + ota_upload.DATA_CHUNK_SIZE]
        client.send_tlv(ota_upload.OTA_DATA, chunk)
        written += len(chunk)
        tlv_type, value = client.receive()
        assert tlv_type == ota_upload.OTA_ACK and ota_ack(value) == written


# The server keeps 8KB of heap free, and wants 2KB more for a connection and 12KB more for each half of a handshake.
MIN_FREE_HEAP = 8 * 1024


def heap_server_args(free_heap):
    return ("--free-heap", str(free_heap))


class HeapTestBase(ServerTestCase):
    def assert_logged(self, message):
        with open(self.log_path, encoding="utf-8") as f:
            self.assertIn(message, f.read())

    def largest_client_hello(self, client):
        point = sc.encode_tlv(0, sc.public_key_bytes(client.ephemeral_private.public_key()))
        return sc.encode_tlv(
            sc.TOP_CLIENT_HELLO, point + sc.encode_tlv(5, bytes(sc.MAX_CLIENT_HELLO_LENGTH - len(point) - 3))
        )


@unittest.skipUnless(NATIVE, "needs a fake heap")
class EnoughHeapTest(HeapTestBase):
    server_args = heap_server_args(MIN_FREE_HEAP + 12 * 1024)

    def test_largest_client_hello_and_echo(self):
        client = self.new_client()
        client.send_client_hello(self.largest_client_hello(client))
        client.receive_server_hello()
        client.send_stream(client.client_ready_tlv())
        client.wait_for_server_ready()
        self.assert_echo_works(client, os.urandom(sc.MAX_LENGTH))


@unittest.skipUnless(NATIVE, "needs a fake heap")
class LowHeapTest(HeapTestBase):
    server_args = heap_server_args(MIN_FREE_HEAP + 12 * 1024 - 1)

    def test_handshake_refused(self):
        client = self.new_client()
        client.send_client_hello()
        self.assert_closed_without_close(client)
        self.assert_logged("couldn't handle its ClientHello without leaving too little heap")


@unittest.skipUnless(NATIVE, "needs a fake heap")
class VeryLowHeapTest(HeapTestBase):
    server_args = heap_server_args(MIN_FREE_HEAP + 4096)

    def test_largest_client_hello_refused(self):
        """Its receive buffer can't grow enough."""
        client = self.new_client()
        client.send_client_hello(self.largest_client_hello(client))
        self.assert_closed_without_close(client)
        self.assert_logged("couldn't grow its receive buffer without leaving too little heap")


@unittest.skipUnless(NATIVE, "needs a fake heap")
class NoHeapForConnectionsTest(HeapTestBase):
    server_args = heap_server_args(MIN_FREE_HEAP + 2 * 1024 - 1)

    def test_connection_refused(self):
        self.assert_socket_closed(self.raw_socket())
        self.assert_logged("won't use 2048 bytes of heap for a new connection")


class OtaTest(ServerTestCase):
    def expect_result(self, client, status):
        tlv_type, value = client.receive()
        self.assertEqual(tlv_type, ota_upload.OTA_RESULT)
        self.assertEqual(ota_upload.result_status(value), status)

    def expect_ack(self, client, written):
        tlv_type, value = client.receive()
        self.assertEqual(tlv_type, ota_upload.OTA_ACK)
        self.assertEqual(ota_ack(value), written)

    def start_update(self, client, image, reboot_when_done=False):
        begin_update(client, image, reboot_when_done)
        self.expect_ack(client, 0)

    def query(self, client):
        client.send_tlv(ota_upload.OTA_QUERY)
        tlv_type, value = client.receive()
        self.assertEqual(tlv_type, ota_upload.OTA_INFO)
        return ota_upload.OtaInfo(value)

    def read_image_out(self):
        with open(self.image_out, "rb") as f:
            return f.read()

    @unittest.skipUnless(NATIVE, "needs the fake flash")
    def test_query(self):
        info = self.query(self.connected_client())
        self.assertEqual(info.available_size, 1024 * 1024)
        self.assertEqual(info.software_version, NATIVE_SOFTWARE_VERSION)
        self.assertEqual(info.chip_id, 1)
        self.assertEqual(info.build_environment, b"secure_channel_test")
        self.assertEqual(info.mac_address, bytes.fromhex("021122334455"))
        self.assertEqual(info.build_time, 1793836800)
        self.assertEqual(info.description(), "MAC address 02:11:22:33:44:55, built at 2026-11-05T00:00:00+00:00")

    @unittest.skipUnless(NATIVE, "needs the fake flash")
    def test_update_succeeds_without_reboot(self):
        client = self.connected_client()
        image = make_image()
        self.start_update(client, image)
        send_image(client, image)
        client.send_tlv(ota_upload.OTA_END)
        self.expect_result(client, 0)
        self.assertEqual(self.read_image_out(), image)
        self.assert_echo_works(client)
        # Another update can follow.
        self.start_update(client, image)

    @unittest.skipUnless(NATIVE, "needs the fake flash")
    def test_update_with_reboot_closes_every_session(self):
        other = self.connected_client()
        client = self.connected_client()
        image = make_image()
        self.start_update(client, image, reboot_when_done=True)
        send_image(client, image)
        client.send_tlv(ota_upload.OTA_END)
        self.expect_result(client, 0)
        self.expect_restart = True
        for session in (client, other):
            with self.assertRaises(sc.SessionClosed) as raised:
                session.receive()
            self.assertEqual(raised.exception.status, 0)
            session.close_socket()
        self.server.wait(timeout=5)
        self.assertEqual(self.server.returncode, RESTART_EXIT_CODE)

    def test_echo_interleaved_with_update(self):
        client = self.connected_client()
        other = self.connected_client()
        image = make_image(50000)
        self.start_update(client, image)
        written = 0
        # Leaves room in each record for the echo.
        chunk_size = 4000
        for offset in range(0, len(image), chunk_size):
            chunk = image[offset : offset + chunk_size]
            client.send_record(sc.encode_tlv(ota_upload.OTA_DATA, chunk) + sc.encode_tlv(ECHO, b"%d" % offset))
            written += len(chunk)
            self.expect_ack(client, written)
            self.assertEqual(client.receive(), (ECHO, b"%d" % offset))
            self.assert_echo_works(other)

    def test_data_or_end_without_begin(self):
        client = self.connected_client()
        client.send_tlv(ota_upload.OTA_DATA, b"discarded")
        client.send_tlv(ota_upload.OTA_END)
        self.expect_result(client, 6)
        self.assert_echo_works(client)

    def test_two_clients_at_once(self):
        first = self.connected_client()
        second = self.connected_client()
        image = make_image()
        self.start_update(first, image)
        begin_update(second, image)
        self.expect_result(second, 4)
        # The other session's data is discarded rather than ending up in the image.
        second.send_tlv(ota_upload.OTA_DATA, b"discarded")
        second.send_tlv(ota_upload.OTA_END)
        self.expect_result(second, 6)
        # A second OtaBegin from the updating client doesn't disturb its update either.
        begin_update(first, image)
        self.expect_result(first, 4)
        if NATIVE:
            send_image(first, image)
            first.send_tlv(ota_upload.OTA_END)
            self.expect_result(first, 0)
            self.assertEqual(self.read_image_out(), image)

    @unittest.skipUnless(NATIVE, "needs the fake flash")
    def test_updating_callback(self):
        client = self.connected_client()
        self.start_update(client, make_image())
        client.close()
        time.sleep(0.3)
        with open(self.log_path, encoding="utf-8") as f:
            log = f.read()
        self.assertIn("OTA updating: yes", log)
        self.assertIn("OTA updating: no", log[log.index("OTA updating: yes") :])

    def test_failing_mid_update_lets_another_client_update(self):
        for ending in ("no_close", "clean_close", "flipped_bit"):
            with self.subTest(ending=ending):
                client = self.connected_client()
                image = make_image()
                self.start_update(client, image)
                client.send_tlv(ota_upload.OTA_DATA, image[:4096])
                self.expect_ack(client, 4096)
                if ending == "no_close":
                    client.close_socket()
                elif ending == "clean_close":
                    client.close()
                else:
                    record = bytearray(client.seal_record(sc.encode_tlv(ECHO, b"hello")))
                    record[-1] ^= 1
                    client.send_raw(record)
                    self.assert_session_failed(client)
                time.sleep(0.3)
                other = self.connected_client()
                self.start_update(other, image)
                other.close()
                time.sleep(0.3)

    def test_begin_errors(self):
        client = self.connected_client()
        image = make_image()
        sha256 = sc.encode_tlv(1, hashlib.sha256(image).digest())
        chip = sc.encode_integer_field(2, 1)
        too_large = sc.encode_integer_field(0, 2**40)
        begin = sc.encode_integer_field(0, len(image))
        for fields, status in (
            (too_large + sha256 + chip, 2),
            (sc.encode_integer_field(0, 0) + sha256 + chip, 2),
            (begin + sha256 + sc.encode_integer_field(2, 9), 3),
        ):
            client.send_tlv(ota_upload.OTA_BEGIN, fields)
            self.expect_result(client, status)
        self.assert_echo_works(client)

    def test_malformed_begin_closes_session(self):
        image = make_image()
        sha256 = sc.encode_tlv(1, hashlib.sha256(image).digest())
        size = sc.encode_integer_field(0, len(image))
        chip = sc.encode_integer_field(2, 1)
        for name, fields in (
            ("missing_size", sha256 + chip),
            ("missing_sha256", size + chip),
            ("missing_chip", size + sha256),
            ("repeated_size", size + size + sha256 + chip),
            ("short_sha256", size + sc.encode_tlv(1, bytes(31)) + chip),
            ("integer_too_long", sc.encode_tlv(0, bytes(3)) + sha256 + chip),
            ("flag_with_value", size + sha256 + chip + sc.encode_tlv(3, b"x")),
            ("cut_off", size + sha256 + chip + b"\x05"),
        ):
            with self.subTest(name=name):
                client = self.connected_client()
                client.send_tlv(ota_upload.OTA_BEGIN, fields)
                with self.assertRaises(sc.SessionClosed) as raised:
                    client.receive()
                self.assertEqual(raised.exception.status, 1)

    def test_malformed_query_or_end_closes_session(self):
        for tlv_type in (ota_upload.OTA_QUERY, ota_upload.OTA_END):
            with self.subTest(tlv_type=tlv_type):
                client = self.connected_client()
                client.send_tlv(tlv_type, b"\x05")
                with self.assertRaises(sc.SessionClosed):
                    client.receive()

    def test_query_with_unknown_field_is_answered(self):
        client = self.connected_client()
        client.send_tlv(ota_upload.OTA_QUERY, sc.encode_tlv(50, b"future"))
        self.assertEqual(client.receive()[0], ota_upload.OTA_INFO)

    def test_too_much_data(self):
        client = self.connected_client()
        image = make_image(100)
        self.start_update(client, image)
        client.send_tlv(ota_upload.OTA_DATA, image + b"extra")
        self.expect_result(client, 2)
        # We're no longer updating.
        client.send_tlv(ota_upload.OTA_END)
        self.expect_result(client, 6)

    def test_too_little_data(self):
        client = self.connected_client()
        image = make_image()
        self.start_update(client, image)
        send_image(client, image[:-1])
        client.send_tlv(ota_upload.OTA_END)
        self.expect_result(client, 2)

    def test_sha256_mismatch(self):
        client = self.connected_client()
        image = make_image()
        self.start_update(client, image)
        send_image(client, image[:-1] + bytes([image[-1] ^ 1]))
        client.send_tlv(ota_upload.OTA_END)
        self.expect_result(client, 8)

    @unittest.skipUnless(NATIVE, "needs the fake flash")
    def test_wrong_chip_in_image(self):
        client = self.connected_client()
        image = make_image(chip_id=9)
        client.send_tlv(
            ota_upload.OTA_BEGIN,
            sc.encode_integer_field(0, len(image))
            + sc.encode_tlv(1, hashlib.sha256(image).digest())
            + sc.encode_integer_field(2, 1),
        )
        self.expect_ack(client, 0)
        send_image(client, image)
        client.send_tlv(ota_upload.OTA_END)
        self.expect_result(client, 9)
        self.assertFalse(os.path.exists(self.image_out))

    @unittest.skipUnless(NATIVE, "needs the fake flash")
    def test_wrong_project(self):
        client = self.connected_client()
        image = make_image(project=b"arduino-lib-builder")
        self.start_update(client, image)
        send_image(client, image)
        client.send_tlv(ota_upload.OTA_END)
        self.expect_result(client, 10)
        self.assertFalse(os.path.exists(self.image_out))


@unittest.skipUnless(NATIVE, "needs the fake flash")
class OtaSlowFlashTest(OtaTest):
    """All of OtaTest again, with writes slow enough to still be going on when the next OtaData arrives."""

    server_args = ("--write-delay-ms", "20")

    def test_large_image(self):
        client = self.connected_client()
        image = make_image(size=200000)
        self.start_update(client, image)
        send_image(client, image)
        client.send_tlv(ota_upload.OTA_END)
        self.expect_result(client, 0)
        self.assertEqual(self.read_image_out(), image)


@unittest.skipUnless(NATIVE, "needs the fake flash")
class OtaWriteFailureTest(ServerTestCase):
    server_args = ("--fail-write-at", "10000")

    def test_failed_write_is_reported_on_the_next_tlv(self):
        """Writes happen after the OtaAck, so a failure gets reported on the OtaData or OtaEnd after it."""
        image = make_image(size=20000)
        size = ota_upload.DATA_CHUNK_SIZE
        for following in ("data", "end"):
            with self.subTest(following=following):
                client = self.connected_client()
                begin_update(client, image)
                tlv_type, value = client.receive()
                self.assertEqual((tlv_type, ota_ack(value)), (ota_upload.OTA_ACK, 0))
                for offset in range(0, 3 * size, size):
                    client.send_tlv(ota_upload.OTA_DATA, image[offset : offset + size])
                    tlv_type, value = client.receive()
                    self.assertEqual((tlv_type, ota_ack(value)), (ota_upload.OTA_ACK, offset + size))
                if following == "data":
                    client.send_tlv(ota_upload.OTA_DATA, image[3 * size : 4 * size])
                else:
                    client.send_tlv(ota_upload.OTA_END)
                tlv_type, value = client.receive()
                self.assertEqual(tlv_type, ota_upload.OTA_RESULT)
                self.assertEqual(ota_upload.result_status(value), 7)
                # The update has ended, so another can start.
                begin_update(client, image)
                tlv_type, value = client.receive()
                self.assertEqual((tlv_type, ota_ack(value)), (ota_upload.OTA_ACK, 0))
                client.close()
                time.sleep(0.3)


@unittest.skipUnless(NATIVE, "needs the fake flash")
class OtaNoPartitionTest(ServerTestCase):
    server_args = ("--no-partition",)

    def test_query_and_begin(self):
        client = self.connected_client()
        client.send_tlv(ota_upload.OTA_QUERY)
        tlv_type, value = client.receive()
        self.assertEqual(tlv_type, ota_upload.OTA_INFO)
        self.assertEqual(ota_upload.OtaInfo(value).available_size, 0)
        begin_update(client, make_image())
        tlv_type, value = client.receive()
        self.assertEqual(tlv_type, ota_upload.OTA_RESULT)
        self.assertEqual(ota_upload.result_status(value), 1)


@unittest.skipUnless(NATIVE, "needs the fake flash")
class OtaPendingVerifyTest(ServerTestCase):
    server_args = ("--pending-verify",)

    def test_begin_before_marked_valid(self):
        client = self.connected_client()
        begin_update(client, make_image())
        tlv_type, value = client.receive()
        self.assertEqual(tlv_type, ota_upload.OTA_RESULT)
        self.assertEqual(ota_upload.result_status(value), 5)


def rollback_guard_test(*args):
    """Makes a base class whose server reports this to OtaRollbackGuard, which confirms after 1s and times out at 3s."""

    @unittest.skipUnless(NATIVE, "needs the fake flash")
    class RollbackGuardTestBase(ServerTestCase):
        server_args = ("--pending-verify", "--rollback-guard", *args)

        def update_status_after_seconds(self, seconds):
            """Waits and returns the status from an OtaBegin, or None if it was allowed to start."""
            time.sleep(seconds)
            client = self.connected_client()
            begin_update(client, make_image())
            tlv_type, value = client.receive()
            return ota_upload.result_status(value) if tlv_type == ota_upload.OTA_RESULT else None

        def assert_rolls_back(self):
            self.assertEqual(self.update_status_after_seconds(0), 5)
            self.expect_restart = True
            self.restart_exit_code = ROLLBACK_EXIT_CODE
            start = time.monotonic()
            self.server.wait(timeout=6)
            # We've already used a little of the 3s. The timeout task checks once per second.
            self.assertLess(time.monotonic() - start, 5)
            self.assertEqual(self.server.returncode, ROLLBACK_EXIT_CODE)

    return RollbackGuardTestBase


class RollbackGuardWorkingTest(rollback_guard_test("working")):
    def test_confirms_and_never_rolls_back(self):
        self.assertEqual(self.update_status_after_seconds(0), 5)
        self.assertIsNone(self.update_status_after_seconds(1.5))
        # Still running long after the timeout would have fired, and updates can carry on.
        time.sleep(3)
        self.assertIsNone(self.server.poll())
        self.assert_echo_works(self.connected_client())


class RollbackGuardNoNetworkTest(rollback_guard_test("no-network")):
    def test_rolls_back(self):
        self.assert_rolls_back()


class RollbackGuardNoRenderTest(rollback_guard_test("no-render")):
    def test_rolls_back(self):
        self.assert_rolls_back()


class RollbackGuardHungTest(rollback_guard_test("hung")):
    def test_rolls_back_without_the_primary_runloop(self):
        self.assert_rolls_back()


class RollbackGuardImpossibleTest(rollback_guard_test("no-network", "--rollback-impossible")):
    def test_keeps_running_unconfirmed(self):
        time.sleep(5)
        self.assertIsNone(self.server.poll())
        self.assertEqual(self.update_status_after_seconds(0), 5)


class RollbackGuardImpossibleThenConfirmedTest(rollback_guard_test("late-network", "--rollback-impossible")):
    def test_can_still_confirm_after_failing_to_roll_back(self):
        self.assertEqual(self.update_status_after_seconds(0), 5)
        self.assertIsNone(self.update_status_after_seconds(6))


class OtaClientTestBase(unittest.TestCase):
    """Runs Uploader against a FakeSecureChannelServer."""

    def setUp(self):
        self.project_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.project_dir)
        with open(os.path.join(self.project_dir, "platformio.ini"), "w", encoding="utf-8") as f:
            f.write("[env]\nx = 1\n\n[env:vest]\nextends = env:_atom\n\n[env:vest_s3]\n")
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.addCleanup(self.listener.close)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(8)
        self.port = self.listener.getsockname()[1]
        self.builds = []
        self.build_exit_code = 0
        self.build_seconds = 0

    def write_build(self, environment, image, software_version):
        build_dir = os.path.join(self.project_dir, ".pio", "build", environment)
        os.makedirs(build_dir)
        with open(os.path.join(build_dir, "firmware.bin"), "wb") as f:
            f.write(image)
        with open(os.path.join(build_dir, "software_version.bin"), "wb") as f:
            f.write(software_version)

    def start_build(self, _project_dir, environment):
        self.builds.append(environment)
        test = self

        class FakeProcess:
            def __init__(self):
                self.start = time.monotonic()
                self.returncode = None

            def poll(self):
                if time.monotonic() - self.start >= test.build_seconds:
                    self.returncode = test.build_exit_code
                return self.returncode

            def kill(self):
                pass

        return FakeProcess()

    def uploader(self):
        return ota_upload.Uploader(KEY, project_dir=self.project_dir, port=self.port, start_build=self.start_build)

    def accept(self):
        self.listener.settimeout(5)
        sock, _ = self.listener.accept()
        self.addCleanup(sock.close)
        server = sc.FakeSecureChannelServer(sock)
        server.handshake()
        return server

    def send_info(self, server, build_environment=b"vest", software_version=bytes(20), available_size=1 << 20, chip=1):
        tlv_type, _ = server.read_tlv(5)
        self.assertEqual(tlv_type, ota_upload.OTA_QUERY)
        server.send_tlv(
            ota_upload.OTA_INFO,
            sc.encode_integer_field(0, available_size)
            + sc.encode_tlv(1, software_version)
            + sc.encode_integer_field(2, chip)
            + sc.encode_tlv(3, build_environment),
        )

    def expect_client_closed(self, server):
        tlv_type, _ = self.read_past_keepalives(server)
        self.assertEqual(tlv_type, sc.RECORD_CLOSE)

    def read_past_keepalives(self, server):
        while True:
            tlv_type, value = server.read_tlv(5)
            if tlv_type != ota_upload.OTA_QUERY:
                return tlv_type, value


class OtaClientTest(OtaClientTestBase):
    def test_environment_not_in_platformio_ini(self):
        for name in (b"../..", b"-t upload", b"vest\n", b"env:vest", b"", b"vest_s4", b"\xff"):
            with self.subTest(name=name):
                thread = self.uploader().on_server_heard("127.0.0.1")
                server = self.accept()
                self.send_info(server, build_environment=name)
                self.expect_client_closed(server)
                thread.join(5)
                self.assertEqual(self.builds, [])

    def test_build_failure(self):
        self.build_exit_code = 1
        uploader = self.uploader()
        thread = uploader.on_server_heard("127.0.0.1")
        server = self.accept()
        self.send_info(server)
        self.expect_client_closed(server)
        thread.join(5)
        self.assertEqual(self.builds, ["vest"])

    def test_skips_matching_version(self):
        self.write_build("vest", make_image(), b"v" * 20)
        thread = self.uploader().on_server_heard("127.0.0.1")
        server = self.accept()
        self.send_info(server, software_version=b"v" * 20)
        self.expect_client_closed(server)
        thread.join(5)

    def test_skips_image_too_large(self):
        self.write_build("vest", make_image(1000), b"v" * 20)
        thread = self.uploader().on_server_heard("127.0.0.1")
        server = self.accept()
        self.send_info(server, available_size=999)
        self.expect_client_closed(server)
        thread.join(5)

    def test_skips_wrong_chip(self):
        for image_chip_id, reported_chip in ((9, 1), (0, 9), (0, 0)):
            with self.subTest(image_chip_id=image_chip_id, reported_chip=reported_chip):
                shutil.rmtree(os.path.join(self.project_dir, ".pio"), ignore_errors=True)
                self.write_build("vest", make_image(1000, chip_id=image_chip_id), b"v" * 20)
                thread = self.uploader().on_server_heard("127.0.0.1")
                server = self.accept()
                self.send_info(server, chip=reported_chip)
                self.expect_client_closed(server)
                thread.join(5)

    def test_chip_conversion(self):
        self.assertEqual(ota_upload.image_chip_model(make_image(chip_id=0)), 1)
        self.assertEqual(ota_upload.image_chip_model(make_image(chip_id=9)), 9)
        self.assertEqual(ota_upload.image_chip_model(make_image(chip_id=13)), 13)

    def test_keeps_session_open_during_slow_build(self):
        original = ota_upload.KEEPALIVE_INTERVAL_SECONDS
        ota_upload.KEEPALIVE_INTERVAL_SECONDS = 1
        self.addCleanup(setattr, ota_upload, "KEEPALIVE_INTERVAL_SECONDS", original)
        self.build_seconds = 3.5
        self.write_build("vest", make_image(), b"v" * 20)
        thread = self.uploader().on_server_heard("127.0.0.1")
        server = self.accept()
        self.send_info(server, software_version=b"v" * 20)
        keepalives = 0
        while True:
            tlv_type, _ = server.read_tlv(5)
            if tlv_type != ota_upload.OTA_QUERY:
                break
            keepalives += 1
            self.send_info_answer(server)
        self.assertEqual(tlv_type, sc.RECORD_CLOSE)
        self.assertGreaterEqual(keepalives, 2)
        thread.join(5)

    @staticmethod
    def send_info_answer(server):
        server.send_tlv(
            ota_upload.OTA_INFO,
            sc.encode_integer_field(0, 1 << 20)
            + sc.encode_tlv(1, bytes(20))
            + sc.encode_integer_field(2, 1)
            + sc.encode_tlv(3, b"vest"),
        )

    def test_builds_one_at_a_time(self):
        self.build_seconds = 1.2
        self.write_build("vest", make_image(), b"v" * 20)
        uploader = self.uploader()
        running = []
        maximum = [0]
        original_start_build = uploader.start_build

        def counting_start_build(project_dir, environment):
            process = original_start_build(project_dir, environment)
            running.append(process)
            maximum[0] = max(maximum[0], sum(1 for p in running if p.returncode is None))
            return process

        uploader.start_build = counting_start_build
        threads = [threading.Thread(target=uploader.update_server, args=("127.0.0.1",)) for _ in range(2)]
        for thread in threads:
            thread.start()
        servers = [self.accept(), self.accept()]
        for server in servers:
            self.send_info(server, software_version=b"v" * 20)
        for server in servers:
            self.expect_client_closed(server)
        for thread in threads:
            thread.join(10)
        self.assertEqual(len(self.builds), 2)
        self.assertEqual(maximum[0], 1)

    def test_discovery_connects_once_per_address(self):
        uploader = self.uploader()
        connections = []
        uploader.update_server = connections.append
        thread = uploader.on_server_heard("10.0.0.1")
        thread.join(5)
        self.assertIsNone(uploader.on_server_heard("10.0.0.1"))
        uploader.on_server_heard("10.0.0.2").join(5)
        # After a reboot, the same address is heard again.
        self.assertIsNone(uploader.on_server_heard("10.0.0.1"))
        self.assertEqual(connections, ["10.0.0.1", "10.0.0.2"])

    def test_is_our_payload(self):
        self.assertTrue(ota_upload.is_our_payload(b"\x10abc"))
        self.assertTrue(ota_upload.is_our_payload(b"\x1fabc"))
        self.assertFalse(ota_upload.is_our_payload(b"\x20abc"))
        self.assertFalse(ota_upload.is_our_payload(b""))


@unittest.skipUnless(NATIVE, "needs the fake flash")
class OtaClientEndToEndTest(ServerTestCase):
    def test_upload(self):
        project_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, project_dir)
        with open(os.path.join(project_dir, "platformio.ini"), "w", encoding="utf-8") as f:
            f.write("[env:secure_channel_test]\n")
        build_dir = os.path.join(project_dir, ".pio", "build", "secure_channel_test")
        os.makedirs(build_dir)
        image = make_image(300000)
        with open(os.path.join(build_dir, "firmware.bin"), "wb") as f:
            f.write(image)
        with open(os.path.join(build_dir, "software_version.bin"), "wb") as f:
            f.write(b"v" * 52)

        class FinishedProcess:
            returncode = 0

            def poll(self):
                return 0

        self.expect_restart = True
        uploader = ota_upload.Uploader(KEY, project_dir=project_dir, start_build=lambda _d, _e: FinishedProcess())
        uploader.update_server(HOST)
        self.server.wait(timeout=5)
        self.assertEqual(self.server.returncode, RESTART_EXIT_CODE)
        with open(self.image_out, "rb") as f:
            self.assertEqual(f.read(), image)

    def test_up_to_date(self):
        project_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, project_dir)
        with open(os.path.join(project_dir, "platformio.ini"), "w", encoding="utf-8") as f:
            f.write("[env:secure_channel_test]\n")
        build_dir = os.path.join(project_dir, ".pio", "build", "secure_channel_test")
        os.makedirs(build_dir)
        with open(os.path.join(build_dir, "firmware.bin"), "wb") as f:
            f.write(make_image())
        with open(os.path.join(build_dir, "software_version.bin"), "wb") as f:
            f.write(NATIVE_SOFTWARE_VERSION)

        class FinishedProcess:
            returncode = 0

            def poll(self):
                return 0

        uploader = ota_upload.Uploader(KEY, project_dir=project_dir, start_build=lambda _d, _e: FinishedProcess())
        uploader.update_server(HOST)
        self.assertEqual(self.server.poll(), None)
        self.assertFalse(os.path.exists(self.image_out))


class FinishedBuild:
    returncode = 0

    def poll(self):
        return 0


class OtaClientUploadTestBase(ServerTestCase):
    def upload(self, image):
        """Runs the client's whole update against the server, and returns what it logged."""
        project_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, project_dir)
        with open(os.path.join(project_dir, "platformio.ini"), "w", encoding="utf-8") as f:
            f.write("[env:secure_channel_test]\n")
        build_dir = os.path.join(project_dir, ".pio", "build", "secure_channel_test")
        os.makedirs(build_dir)
        with open(os.path.join(build_dir, "firmware.bin"), "wb") as f:
            f.write(image)
        with open(os.path.join(build_dir, "software_version.bin"), "wb") as f:
            f.write(b"v" * 52)
        uploader = ota_upload.Uploader(KEY, project_dir=project_dir, start_build=lambda _d, _e: FinishedBuild())
        logged = []
        uploader.log = lambda _address, message: logged.append(message)
        uploader.update_server(HOST)
        return logged


@unittest.skipUnless(NATIVE, "needs the fake flash")
class OtaClientSlowFlashTest(OtaClientUploadTestBase):
    # 900KB at 5ms per 4077 bytes takes over a second to write, and is more than the socket buffers on both sides hold,
    # so the client has to wait for the socket to take more while it reads acknowledgments.
    server_args = ("--write-delay-ms", "5")

    def test_upload_fills_the_send_buffer(self):
        image = make_image(900000)
        self.expect_restart = True
        logged = self.upload(image)
        self.server.wait(timeout=5)
        self.assertEqual(self.server.returncode, RESTART_EXIT_CODE)
        with open(self.image_out, "rb") as f:
            self.assertEqual(f.read(), image)
        self.assertIn("100%, 900000 of 900000 bytes", "\n".join(logged))


@unittest.skipUnless(NATIVE, "needs the fake flash")
class OtaClientWriteFailureTest(OtaClientUploadTestBase):
    server_args = ("--fail-write-at", "100000")

    def test_failure_while_still_sending(self):
        """The client hears about the failure partway through sending, and stops."""
        logged = self.upload(make_image(900000))
        self.assertIn("failed: server reported status 7: flash write failed", logged)
        self.assertFalse(any(message.startswith("100%") for message in logged))


class SoftwareVersionTest(unittest.TestCase):
    """Runs software_version.py with a stand-in for PlatformIO's env."""

    def setUp(self):
        self.project_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.project_dir)
        self.build_dir = os.path.join(self.project_dir, ".pio", "build", "vest")
        self.git("init", "-q")
        self.git("config", "user.email", "test@example.com")
        self.git("config", "user.name", "Test")
        self.write("tracked.txt", "tracked")
        self.write(".gitignore", ".pio\nignored.txt\n")
        self.git("add", ".")
        self.git("commit", "-qm", "initial")

    def git(self, *args):
        return subprocess.run(["git", *args], cwd=self.project_dir, check=True, capture_output=True, text=True).stdout

    def write(self, path, contents):
        with open(os.path.join(self.project_dir, path), "w", encoding="utf-8") as f:
            f.write(contents)

    def read_build_time(self):
        with open(os.path.join(self.build_dir, "generated", "generated_software_version.h"), encoding="utf-8") as f:
            return int(re.search(r"^#define JL_BUILD_TIME ([0-9]+)$", f.read(), re.MULTILINE).group(1))

    def test_build_time_only_changes_with_version(self):
        with mock.patch("time.time", return_value=1000.5):
            self.run_script()
        self.assertEqual(self.read_build_time(), 1000)
        with mock.patch("time.time", return_value=2000):
            self.run_script()
        self.assertEqual(self.read_build_time(), 1000)
        self.write("tracked.txt", "changed")
        with mock.patch("time.time", return_value=3000):
            self.run_script()
        self.assertEqual(self.read_build_time(), 3000)

    def run_script(self):
        project_dir = self.project_dir
        build_dir = self.build_dir

        class FakeEnv:
            def __init__(self):
                self.appended = {}

            def subst(self, name):
                return {"$PROJECT_DIR": project_dir, "$BUILD_DIR": build_dir, "$PIOENV": "vest"}[name]

            def Append(self, **kwargs):  # pylint: disable=invalid-name
                self.appended.update(kwargs)

        fake_env = FakeEnv()
        script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "software_version.py")
        with open(script, encoding="utf-8") as f:
            code = compile(f.read(), script, "exec")
        scope = {"Import": lambda _name: None, "env": fake_env, "__builtins__": builtins}
        exec(code, scope)  # pylint: disable=exec-used
        self.assertEqual(fake_env.appended["SRC_BUILD_FLAGS"], ["-I" + os.path.join(build_dir, "generated")])
        with open(os.path.join(build_dir, "software_version.bin"), "rb") as f:
            return f.read()

    def test_clean_checkout(self):
        version = self.run_script()
        self.assertEqual(version.hex(), self.git("rev-parse", "HEAD").strip())
        with open(os.path.join(self.build_dir, "generated", "generated_software_version.h"), encoding="utf-8") as f:
            header = f.read()
        self.assertIn("0x%02x,0x%02x" % (version[0], version[1]), header)
        self.assertIn('#define JL_BUILD_ENVIRONMENT "vest"', header)

    def test_changed_tracked_file(self):
        clean = self.run_script()
        self.write("tracked.txt", "changed")
        changed = self.run_script()
        self.assertEqual(len(changed), 52)
        self.assertEqual(changed[:20], clean)
        self.write("tracked.txt", "changed again")
        self.assertNotEqual(self.run_script(), changed)
        self.write("tracked.txt", "tracked")
        self.assertEqual(self.run_script(), clean)

    def test_new_untracked_file(self):
        clean = self.run_script()
        self.write("new.txt", "new")
        changed = self.run_script()
        self.assertEqual(len(changed), 52)
        self.assertEqual(changed[:20], clean)
        self.write("new.txt", "newer")
        self.assertNotEqual(self.run_script(), changed)

    def test_ignored_file(self):
        clean = self.run_script()
        self.write("ignored.txt", "ignored")
        self.assertEqual(self.run_script(), clean)


if __name__ == "__main__":
    unittest.main()
