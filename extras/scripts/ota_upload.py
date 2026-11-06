#!/usr/bin/env python3
"""Keeps JazzLights devices on the local network up to date, see "Part 2: OTA" in extras/docs/OTA_PROPOSAL.md.

For every device it discovers, this builds the firmware from this checkout for that device's PlatformIO environment,
and uploads it if it differs from what's running. This runs until interrupted.
"""

import argparse
import datetime
import hashlib
import os
import re
import socket
import struct
import subprocess
import threading
import time

import secure_channel

MULTICAST_ADDRESS = "224.0.0.169"
MULTICAST_PORT = 6699
# The high nibble of the first byte of our UDP payloads.
UDP_VERSION = 0x10
UDP_VERSION_MASK = 0xf0

OTA_QUERY = 3
OTA_INFO = 4
OTA_BEGIN = 5
OTA_DATA = 6
OTA_ACK = 7
OTA_END = 8
OTA_RESULT = 9

# As much as fits in one record, after the OtaData's one-byte Type and two-byte Length.
DATA_CHUNK_SIZE = secure_channel.DEFAULT_MAX_RECORD_PLAINTEXT - 3
MAX_UNACKNOWLEDGED = 4
KEEPALIVE_INTERVAL_SECONDS = 30
# Flash writes and image verification can take a while, particularly with several devices at once.
ACK_TIMEOUT_SECONDS = 30
RESULT_TIMEOUT_SECONDS = 60
IMAGE_CHIP_ID_OFFSET = 12
CHIP_ID_ESP32_IN_IMAGE = 0
CHIP_MODEL_ESP32 = 1

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

STATUS_NAMES = {
    0: "success",
    1: "no OTA partition",
    2: "bad image size",
    3: "wrong chip",
    4: "another client is updating",
    5: "esp_ota_begin failed, the running firmware might not be marked valid yet",
    6: "not updating",
    7: "flash write failed",
    8: "SHA-256 mismatch",
    9: "invalid image",
    10: "wrong project",
    11: "failed to set boot partition",
}


class OtaInfo:
    def __init__(self, value):
        fields = secure_channel.parse_fields(value)
        self.available_size = secure_channel.parse_integer(secure_channel.required_field(fields, 0))
        self.software_version = secure_channel.required_field(fields, 1)
        self.chip_id = secure_channel.parse_integer(secure_channel.required_field(fields, 2))
        self.build_environment = secure_channel.required_field(fields, 3)
        self.mac_address = fields.get(4)
        self.build_time = secure_channel.parse_integer(fields[5]) if 5 in fields else None

    def description(self):
        mac = ":".join("%02x" % b for b in self.mac_address) if self.mac_address else "unknown"
        if self.build_time is None:
            built = "unknown"
        else:
            built = datetime.datetime.fromtimestamp(self.build_time, datetime.timezone.utc).isoformat()
        return "MAC address %s, built at %s" % (mac, built)


def platformio_environments(project_dir):
    """Returns the set of environment names in platformio.ini, as bytes since that's how we compare them."""
    with open(os.path.join(project_dir, "platformio.ini"), encoding="utf-8") as f:
        return {name.encode() for name in re.findall(r"^\[env:([^\]]+)\]", f.read(), re.MULTILINE)}


def image_chip_model(image):
    """Reads an image header's chip_id, and converts it to what esp_chip_info() reports."""
    chip_id = struct.unpack_from("<H", image, IMAGE_CHIP_ID_OFFSET)[0]
    return CHIP_MODEL_ESP32 if chip_id == CHIP_ID_ESP32_IN_IMAGE else chip_id


def encode_ota_begin(image, reboot_when_done=True):
    fields = secure_channel.encode_integer_field(0, len(image))
    fields += secure_channel.encode_tlv(1, hashlib.sha256(image).digest())
    fields += secure_channel.encode_integer_field(2, image_chip_model(image))
    if reboot_when_done:
        fields += secure_channel.encode_tlv(3)
    return fields


def run_build(project_dir, build_environment):
    """Starts a build, and returns the process."""
    return subprocess.Popen(["pio", "run", "-e", build_environment], cwd=project_dir)


class Uploader:
    def __init__(self, private_key, project_dir=REPO_ROOT, port=secure_channel.DEFAULT_PORT, start_build=run_build):
        self.private_key = private_key
        self.project_dir = project_dir
        self.port = port
        self.start_build = start_build
        self.build_lock = threading.Lock()
        self.seen_addresses = set()
        self.seen_lock = threading.Lock()

    def log(self, address, message):
        print("[%s] %s" % (address, message), flush=True)

    def on_server_heard(self, address):
        """Connects once to each address. Returns the thread if this is a new one."""
        with self.seen_lock:
            if address in self.seen_addresses:
                return None
            self.seen_addresses.add(address)
        thread = threading.Thread(target=self.update_server, args=(address,), daemon=True)
        thread.start()
        return thread

    def update_server(self, address):
        client = None
        try:
            client = secure_channel.SecureChannelClient(address, self.private_key, port=self.port)
            client.connect()
            self.update_session(address, client)
            client.close()
        except (OSError, secure_channel.ProtocolError, secure_channel.ConnectionEnded, ValueError) as err:
            self.log(address, "failed: %s" % err)
            if client is not None:
                client.close(status=1)
        except secure_channel.SessionClosed as err:
            self.log(address, "the server closed the session with status %d" % err.status)
            client.close_socket()

    def update_session(self, address, client):
        client.send_tlv(OTA_QUERY)
        info = self.wait_for(client, OTA_INFO, client.timeout, ignore=())
        info = OtaInfo(info)
        if info.build_environment not in platformio_environments(self.project_dir):
            self.log(address, "unknown environment %r" % info.build_environment)
            return
        build_environment = info.build_environment.decode()
        self.log(address, "is running %s with %s, building" % (build_environment, info.description()))
        if not self.build_with_keepalive(address, client, build_environment):
            self.log(address, "build of %s failed" % build_environment)
            return
        build_dir = os.path.join(self.project_dir, ".pio", "build", build_environment)
        with open(os.path.join(build_dir, "software_version.bin"), "rb") as f:
            software_version = f.read()
        with open(os.path.join(build_dir, "firmware.bin"), "rb") as f:
            image = f.read()
        if software_version == info.software_version:
            self.log(address, "is already running this version")
            return
        if len(image) > info.available_size:
            self.log(address, "has room for %d bytes but the image is %d" % (info.available_size, len(image)))
            return
        if image_chip_model(image) != info.chip_id:
            self.log(address, "has chip %d but the image is for %d" % (info.chip_id, image_chip_model(image)))
            return
        self.upload(address, client, image)

    def build_with_keepalive(self, address, client, build_environment):
        """Builds one at a time, sending OtaQuery periodically to keep the session open. Returns whether it worked."""
        last_keepalive = time.monotonic()

        def keepalive():
            nonlocal last_keepalive
            if time.monotonic() - last_keepalive >= KEEPALIVE_INTERVAL_SECONDS:
                client.send_tlv(OTA_QUERY)
                last_keepalive = time.monotonic()
            self.discard_received(client)

        while not self.build_lock.acquire(timeout=1):
            keepalive()
        try:
            process = self.start_build(self.project_dir, build_environment)
            try:
                while process.poll() is None:
                    time.sleep(1)
                    keepalive()
            except BaseException:
                process.kill()
                raise
            return process.returncode == 0
        finally:
            self.build_lock.release()

    @staticmethod
    def discard_received(client):
        """Reads what the server has sent, which is only answers to keepalives."""
        try:
            while True:
                client.receive(timeout=0)
        except TimeoutError:
            pass

    @staticmethod
    def wait_for(client, tlv_type, timeout, ignore=(OTA_INFO,)):
        deadline = time.monotonic() + timeout
        while True:
            received_type, value = client.receive(timeout=max(0.0, deadline - time.monotonic()))
            if received_type == tlv_type:
                return value
            if received_type == OTA_RESULT:
                raise ValueError(result_description(value))
            if received_type not in ignore:
                raise ValueError("expected TLV %d but got %d" % (tlv_type, received_type))

    def upload(self, address, client, image):
        self.log(address, "uploading %d bytes" % len(image))
        start_time = time.monotonic()
        client.send_tlv(OTA_BEGIN, encode_ota_begin(image))
        self.check_ack(client, 0)
        sent = 0
        acknowledged = 0
        while acknowledged < len(image):
            while sent < len(image) and (sent - acknowledged) / DATA_CHUNK_SIZE < MAX_UNACKNOWLEDGED:
                chunk = image[sent : sent + DATA_CHUNK_SIZE]
                client.send_tlv(OTA_DATA, chunk)
                sent += len(chunk)
            acknowledged = min(sent, acknowledged + DATA_CHUNK_SIZE)
            self.check_ack(client, acknowledged)
        client.send_tlv(OTA_END)
        value = self.wait_for_result(client)
        status = result_status(value)
        if status != 0:
            raise ValueError(result_description(value))
        self.log(address, "updated in %.1fs" % (time.monotonic() - start_time))
        # The server is about to send a Close and reboot.
        try:
            client.receive(timeout=5)
        except (TimeoutError, secure_channel.SessionClosed, secure_channel.ConnectionEnded):
            pass

    def check_ack(self, client, expected):
        fields = secure_channel.parse_fields(self.wait_for(client, OTA_ACK, ACK_TIMEOUT_SECONDS))
        bytes_written = secure_channel.parse_integer(secure_channel.required_field(fields, 0))
        if bytes_written != expected:
            raise ValueError("server acknowledged %d bytes instead of %d" % (bytes_written, expected))

    @staticmethod
    def wait_for_result(client):
        deadline = time.monotonic() + RESULT_TIMEOUT_SECONDS
        while True:
            received_type, value = client.receive(timeout=max(0.0, deadline - time.monotonic()))
            if received_type == OTA_RESULT:
                return value


def result_status(value):
    return secure_channel.parse_integer(secure_channel.required_field(secure_channel.parse_fields(value), 0))


def result_description(value):
    status = result_status(value)
    return "server reported status %d: %s" % (status, STATUS_NAMES.get(status, "unknown"))


def is_our_payload(payload):
    return len(payload) > 0 and (payload[0] & UDP_VERSION_MASK) == UDP_VERSION


def open_discovery_socket(multicast_address=MULTICAST_ADDRESS, port=MULTICAST_PORT):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, "SO_REUSEPORT"):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    sock.bind(("", port))
    membership = struct.pack("=4s4s", socket.inet_aton(multicast_address), socket.inet_aton("0.0.0.0"))
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, membership)
    return sock


def discover_forever(uploader, sock):
    while True:
        payload, (address, _) = sock.recvfrom(1500)
        if is_our_payload(payload):
            uploader.on_server_heard(address)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--key", help="path to the PEM private key, defaults to the JL_CLIENT_KEY environment variable")
    args = parser.parse_args()
    private_key = secure_channel.load_private_key(secure_channel.find_private_key_path(args.key))
    print("Listening for devices on %s:%d" % (MULTICAST_ADDRESS, MULTICAST_PORT), flush=True)
    try:
        discover_forever(Uploader(private_key), open_discovery_socket())
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
