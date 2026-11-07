#!/usr/bin/env python3
"""Keeps JazzLights devices on the local network up to date, see "Part 2: OTA" in extras/docs/OTA_PROPOSAL.md.

For every device it discovers, this builds the firmware from this checkout for that device's PlatformIO environment,
and uploads it if it differs from what's running. This runs until interrupted, unless given --once or --host, which make
it update what it finds and exit, with a failure status if any update failed or it found nothing to update.

With --mac, it only updates the devices with those MAC addresses, which are what they report in OtaInfo, and also what
they use as their ID in JazzLights logs. With --environment as well, those devices get that environment even if they're
running a different one, like a serial upload would do.
"""

import argparse
import collections
import datetime
import hashlib
import os
import re
import select
import socket
import struct
import subprocess
import sys
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
PATTERN_OVERRIDE = 10

# What devices show while they're being updated, which is solid purple. Writing to flash disrupts the LED output, and a
# pattern that doesn't change might make that less visible. The device goes back to its usual patterns when the
# session ends.
DEFAULT_UPDATE_PATTERN = 0x00000400

# As much as fits in one record, after the OtaData's one-byte Type and two-byte Length.
DATA_CHUNK_SIZE = secure_channel.DEFAULT_MAX_RECORD_PLAINTEXT - 3
PROGRESS_STEP_PERCENT = 10
KEEPALIVE_INTERVAL_SECONDS = 30
# How long --once listens for devices. They send a payload every 100ms.
DEFAULT_DISCOVERY_SECONDS = 5
# Flash writes and image verification can take a while, particularly with several devices at once. The upload fails if
# it sends nothing and gets no OtaAck for this long.
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

    def mac_text(self):
        return format_mac_address(self.mac_address) if self.mac_address else None

    def build_time_text(self):
        if self.build_time is None:
            return "unknown"
        return datetime.datetime.fromtimestamp(self.build_time, datetime.timezone.utc).isoformat()

    def description(self):
        return "MAC address %s, built at %s" % (self.mac_text() or "unknown", self.build_time_text())


def format_mac_address(mac_address):
    return ":".join("%02x" % b for b in mac_address)


def parse_mac_address(text):
    """Accepts six hexadecimal bytes separated by colons or dashes, or not separated, and returns them formatted like
    format_mac_address() does."""
    digits = re.sub(r"[:-]", "", text.strip())
    if not re.fullmatch(r"[0-9a-fA-F]{12}", digits):
        raise ValueError("%r isn't a MAC address" % text)
    return format_mac_address(bytes.fromhex(digits))


def parse_update_pattern(text):
    if text == "none":
        return None
    pattern = int(text, 0)
    if not 0 <= pattern <= 0xFFFFFFFF:
        raise ValueError("%r doesn't fit in 32 bits" % text)
    return pattern


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
    """Updates devices. With `environment`, it leaves devices running any other environment alone. With `mac_addresses`,
    it only updates devices with one of those MAC addresses, and gives them `environment` whatever they're running. With
    `build` false, it uses what's already in the build directory instead of building."""

    def __init__(
        self,
        private_key,
        project_dir=REPO_ROOT,
        port=secure_channel.DEFAULT_PORT,
        start_build=run_build,
        environment=None,
        build=True,
        mac_addresses=None,
        update_pattern=DEFAULT_UPDATE_PATTERN,
    ):
        self.private_key = private_key
        self.project_dir = project_dir
        self.port = port
        self.start_build = start_build
        self.environment = environment
        self.build = build
        self.update_pattern = update_pattern
        self.mac_addresses = None if mac_addresses is None else {parse_mac_address(mac) for mac in mac_addresses}
        self.build_lock = threading.Lock()
        self.seen_addresses = set()
        self.seen_lock = threading.Lock()
        # What happened to each device: "updated", "up to date", "skipped" or "failed".
        self.outcomes = {}

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
        outcome = "failed"
        try:
            client = secure_channel.SecureChannelClient(address, self.private_key, port=self.port)
            client.connect()
            outcome = self.update_session(address, client)
            client.close()
        except ConnectionRefusedError:
            # It's a JazzLights device, but its firmware doesn't have the secure channel, or it has no keys.
            self.log(address, "doesn't accept secure channel connections, so it can't be updated over OTA")
            outcome = "skipped"
        except (OSError, secure_channel.ProtocolError, secure_channel.ConnectionEnded, ValueError) as err:
            self.log(address, "failed: %s" % err)
            if client is not None:
                client.close(status=1)
        except secure_channel.SessionClosed as err:
            self.log(address, "the server closed the session with status %d" % err.status)
            client.close_socket()
        with self.seen_lock:
            self.outcomes[address] = outcome

    def update_session(self, address, client):
        """Returns what happened, as in self.outcomes."""
        client.send_tlv(OTA_QUERY)
        info = self.wait_for(client, OTA_INFO, client.timeout, ignore=())
        info = OtaInfo(info)
        if self.mac_addresses is not None and info.mac_text() not in self.mac_addresses:
            self.log(
                address,
                "has MAC address %s, not %s, and is running %s, built at %s"
                % (
                    info.mac_text(),
                    " or ".join(sorted(self.mac_addresses)),
                    info.build_environment.decode(errors="replace"),
                    info.build_time_text(),
                ),
            )
            return "skipped"
        running = info.build_environment
        if self.mac_addresses is not None and self.environment is not None:
            # This is the device the environment is for, so it gets it even if it's running another one.
            running_text = running.decode(errors="replace")
            build_environment = self.environment
            if build_environment.encode() not in platformio_environments(self.project_dir):
                self.log(address, "unknown environment %r" % build_environment)
                return "failed"
        else:
            if running not in platformio_environments(self.project_dir):
                self.log(address, "unknown environment %r" % running)
                return "skipped"
            running_text = build_environment = running.decode()
            if self.environment is not None and build_environment != self.environment:
                self.log(address, "is running %s, not %s" % (build_environment, self.environment))
                return "skipped"
        if self.build:
            building = "building" if build_environment == running_text else "building %s" % build_environment
            self.log(address, "is running %s with %s, %s" % (running_text, info.description(), building))
            if not self.build_with_keepalive(address, client, build_environment):
                self.log(address, "build of %s failed" % build_environment)
                return "failed"
        else:
            self.log(address, "is running %s with %s" % (running_text, info.description()))
        build_dir = os.path.join(self.project_dir, ".pio", "build", build_environment)
        with open(os.path.join(build_dir, "software_version.bin"), "rb") as f:
            software_version = f.read()
        with open(os.path.join(build_dir, "firmware.bin"), "rb") as f:
            image = f.read()
        if software_version == info.software_version and build_environment == running_text:
            self.log(address, "is already running this version")
            return "up to date"
        if len(image) > info.available_size:
            self.log(address, "has room for %d bytes but the image is %d" % (info.available_size, len(image)))
            return "failed"
        if image_chip_model(image) != info.chip_id:
            self.log(address, "has chip %d but the image is for %d" % (info.chip_id, image_chip_model(image)))
            return "failed"
        self.upload(address, client, image)
        return "updated"

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
        if self.update_pattern is not None:
            # Firmware without PatternOverride skips it.
            client.send_tlv(PATTERN_OVERRIDE, struct.pack(">I", self.update_pattern))
        self.send_data(address, client, image, start_time)
        client.send_tlv(OTA_END)
        # The device checks the image and then switches which partition it boots from.
        self.log(address, "sent everything, waiting for the device to check the image")
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

    def send_data(self, address, client, image, start_time):
        """Sends every OtaData without waiting for its OtaAck, which made no difference to the speed, since the device's
        TCP receive window and its flash writes are what limit it. TCP's flow control holds this back instead. It only
        sends what the socket takes without blocking, and reads OtaAcks as they arrive, so that neither side gets stuck
        with a full buffer waiting for the other."""
        sock = client.sock
        pending = bytearray()  # Sealed records that the socket hasn't taken yet.
        sealed = 0  # How many bytes of the image are in records so far.
        expected_acks = collections.deque()
        acknowledged = 0
        reported_percent = 0
        last_progress = time.monotonic()
        while acknowledged < len(image):
            if not pending and sealed < len(image):
                chunk = image[sealed : sealed + DATA_CHUNK_SIZE]
                pending += client.seal_record(secure_channel.encode_tlv(OTA_DATA, chunk))
                sealed += len(chunk)
                expected_acks.append(sealed)
            readable, writable, _ = select.select([sock], [sock] if pending else [], [], 1.0)
            if writable:
                try:
                    sent = sock.send(pending)
                except BlockingIOError:
                    sent = 0
                if sent > 0:
                    del pending[:sent]
                    last_progress = time.monotonic()
            if readable:
                for bytes_received in self.receive_acks(client):
                    if not expected_acks or bytes_received != expected_acks.popleft():
                        raise ValueError("server acknowledged %d bytes out of order" % bytes_received)
                    acknowledged = bytes_received
                    last_progress = time.monotonic()
                    percent = acknowledged * 100 // len(image)
                    if percent >= reported_percent + PROGRESS_STEP_PERCENT:
                        reported_percent = percent - percent % PROGRESS_STEP_PERCENT
                        elapsed = time.monotonic() - start_time
                        self.log(
                            address,
                            "%d%%, %d of %d bytes in %.1fs, %.0fKB/s"
                            % (
                                reported_percent,
                                acknowledged,
                                len(image),
                                elapsed,
                                acknowledged / 1000 / max(elapsed, 0.001),
                            ),
                        )
            if time.monotonic() - last_progress > ACK_TIMEOUT_SECONDS:
                raise TimeoutError("the server took no data and acknowledged none for %ds" % ACK_TIMEOUT_SECONDS)

    @staticmethod
    def receive_acks(client):
        """Returns bytes_received from each OtaAck that has arrived, reading what's there without waiting for more."""
        acks = []
        while True:
            try:
                tlv_type, value = client.receive(timeout=0)
            except TimeoutError:
                return acks
            if tlv_type == OTA_ACK:
                fields = secure_channel.parse_fields(value)
                acks.append(secure_channel.parse_integer(secure_channel.required_field(fields, 0)))
            elif tlv_type == OTA_RESULT:
                raise ValueError(result_description(value))
            elif tlv_type != OTA_INFO:
                raise ValueError("expected TLV %d but got %d" % (OTA_ACK, tlv_type))

    def check_ack(self, client, expected):
        fields = secure_channel.parse_fields(self.wait_for(client, OTA_ACK, ACK_TIMEOUT_SECONDS))
        bytes_received = secure_channel.parse_integer(secure_channel.required_field(fields, 0))
        if bytes_received != expected:
            raise ValueError("server acknowledged %d bytes instead of %d" % (bytes_received, expected))

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


def discover_once(uploader, sock, seconds):
    """Listens for devices for this long, then waits for every update that started."""
    threads = []
    deadline = time.monotonic() + seconds
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        sock.settimeout(remaining)
        try:
            payload, (address, _) = sock.recvfrom(1500)
        except TimeoutError:
            break
        if is_our_payload(payload):
            thread = uploader.on_server_heard(address)
            if thread is not None:
                threads.append(thread)
    for thread in threads:
        thread.join()


def update_hosts(uploader, hosts):
    threads = [uploader.on_server_heard(host) for host in hosts]
    for thread in threads:
        if thread is not None:
            thread.join()


def summarize(uploader, environment):
    """Prints what happened to each device, and returns the exit status."""
    for address, outcome in sorted(uploader.outcomes.items()):
        print("[%s] %s" % (address, outcome), flush=True)
    outcomes = list(uploader.outcomes.values())
    if "failed" in outcomes:
        return 1
    if not any(outcome in ("updated", "up to date") for outcome in outcomes):
        if uploader.mac_addresses is not None:
            which = " with MAC address " + " or ".join(sorted(uploader.mac_addresses))
        elif environment:
            which = " running " + environment
        else:
            which = ""
        print("Found no device%s to update" % which, flush=True)
        return 1
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--key", help="path to the PEM private key, defaults to the JL_CLIENT_KEY environment variable")
    parser.add_argument(
        "--environment",
        help="only update devices running this PlatformIO environment, or with --mac, install it on those devices",
    )
    parser.add_argument(
        "--mac",
        action="append",
        type=parse_mac_address,
        help="only update the device with this MAC address, which can be given more than once",
    )
    parser.add_argument(
        "--no-build", action="store_true", help="use what's already built instead of building each environment"
    )
    parser.add_argument(
        "--once",
        action="store_true",
        help="listen for devices for --discovery-seconds, update them, and exit",
    )
    parser.add_argument(
        "--discovery-seconds",
        type=float,
        default=DEFAULT_DISCOVERY_SECONDS,
        help="how long --once listens for devices, %(default)ss by default",
    )
    parser.add_argument(
        "--host", action="append", default=[], help="update the device at this address instead of discovering them"
    )
    parser.add_argument(
        "--update-pattern",
        type=parse_update_pattern,
        default=DEFAULT_UPDATE_PATTERN,
        help="pattern bits that devices show while they're being updated, such as 0x00000400 for solid purple, or none to "
        "leave their patterns alone",
    )
    args = parser.parse_args()
    private_key = secure_channel.load_private_key(secure_channel.find_private_key_path(args.key))
    uploader = Uploader(
        private_key,
        environment=args.environment,
        build=not args.no_build,
        mac_addresses=args.mac,
        update_pattern=args.update_pattern,
    )
    if args.host:
        update_hosts(uploader, args.host)
        return summarize(uploader, args.environment)
    print("Listening for devices on %s:%d" % (MULTICAST_ADDRESS, MULTICAST_PORT), flush=True)
    if args.once:
        discover_once(uploader, open_discovery_socket(), args.discovery_seconds)
        return summarize(uploader, args.environment)
    try:
        discover_forever(uploader, open_discovery_socket())
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
