# Secure Channel, with OTA as the First Use

Status: implemented, tested natively, and partly tested on an ATOM Matrix running `vest`. See "Testing".

- **The secure channel** authenticates the client, sets up session keys and carries
  encrypted TLVs. It knows nothing about firmware.
- **OTA updates** are its first use. OTA only defines a few TLVs, as will later uses.

The **server** is the JazzLights device. The **client** is the machine holding a private
key, such as a laptop running an upload script.

## Goals

- A new use only defines new TLVs, with no change to the handshake, records or cryptography.
- Several clients can have sessions at once, and one session can interleave TLVs from
  several uses.
- Later firmware can add TLVs without breaking older peers.
- No new Arduino dependency: only lwIP sockets, mbedTLS and ESP-IDF's `app_update`, which are
  in `framework = arduino`, `framework = espidf, arduino` and `framework = espidf` builds alike.
- Works on the local network, without GitHub or the public Internet.
- Keep the binary size increase as small as possible.

## Non-goals

- Authenticating the server to the client, now or later.
- Securing the existing UDP multicast protocol.
- Hardware Secure Boot or flash encryption, which permanently burn eFuses.
- Environments where the channel can't run, which keep updating over serial:
  - No IP network (`JL_WIFI=0` without ESP32 Ethernet, such as NimBLE-only builds).
  - Ethernet only through the Arduino Ethernet library, which doesn't use lwIP. Environments that
    also have Wi-Fi use the channel over Wi-Fi.
- ESPHome builds, which have ESPHome's own OTA updates.

## Overview

```
Client                                  Server
|------------ ClientHello ------------>|  in the clear
|<----------- ServerHello -------------|  in the clear
|                                      |  both sides derive keys
|------------ ClientReady ------------>|  encrypted, signed
|----------- encrypted TLVs ---------->|  optional, needn't wait for ServerReady
|                                      |  check signature
|<----------- ServerReady -------------|  encrypted
|                                      |  act on the client's TLVs
|<---------- encrypted TLVs ---------->|  any number, such as OTA
|                                      |
|<-------------- Close --------------->|  from either side
```

## What the channel guarantees

The server knows that the client holds one of the built-in keys, that the session isn't a
replay, that every TLV came from that client unmodified, in order and exactly once, and that
only that client can read what the server sends.

The client knows that every TLV came from the peer it did the key exchange with, unmodified,
in order and exactly once. It doesn't know who that peer is. So for every use of the channel:

- A client must never send secrets to the server, such as a Wi-Fi password.
- A client must not trust what a server reports for anything that matters.
- A request may land on the wrong device, because someone in the middle can relay a session
  to a different real server. For OTA, the server's own checks still require the right chip
  and `project_name`.

## Part 1: Secure channel

### Keys

- Generate the long-term key pair once per client machine:
  ```
  openssl ecparam -name prime256v1 -genkey -noout -out ~/.config/jazzlights/client_private.pem
  openssl ec -in ~/.config/jazzlights/client_private.pem -pubout -out client_public.pem
  ```
- The private key never goes in the repository. Client scripts find it with `--key` or the
  `JL_CLIENT_KEY` environment variable.
- The public keys go in `src/jazzlights/network/secure_channel_keys.h`, as an array of
  65-byte uncompressed P-256 points (`0x04 || X || Y`). All keys have the same permissions.
  `secure_channel.py --key <pem>` prints a public key in the format that file uses.
- The server doesn't start while that list is empty, so firmware without keys doesn't open the
  port or run the task. The list in the repository has 8 keys, whose private keys are kept off the
  repository. Anyone running their own devices from it should replace them with their own.
- To rotate or revoke keys, ship firmware with a new list. If every private key is lost, fall
  back to a serial flash.

### Encoding

Everything on the wire is a TLV:

```
TLV = Type (varint) || Length (varint) || Value (Length bytes)
```

Type and Length are QUIC variable-length integers (RFC 9000, section 16). Any varint size is
valid, and our senders use the shortest form.

The maximum Length is 8192 for a `ClientHello`, and 4096 for every other top-level TLV, including
records, and for every TLV in the stream that records carry. Fields in a container are only
limited by it. These keep what the server buffers for each connection small enough for the device.

A Value is one of:

- **Fields:** nested TLVs. Each container numbers its own types.
- **Integer:** a varint that fills the Value exactly.
- **Bytes:** used as is.
- **Flag:** empty. Being present is what counts.

Parser rules:

- Unknown types are skipped at every level, without being stored.
- These are errors, which close the connection: a Length over its maximum or past the end of
  its container, a known field that's repeated or the wrong size, and a missing required field.
  That includes unknown types, whose Length is checked like any other, and containers with no
  fields defined yet, which must still be well formed.
- Fields can come in any order.
- Compare decoded varints, never raw bytes. Parse them into 64 bits and compare before
  narrowing.

### Top-level messages

| Type | Name | Direction | Value |
|---|---|---|---|
| 0 | `ClientHello` | client → server | fields |
| 1 | `ServerHello` | server → client | fields |
| 2 | `Record` | both | ciphertext and tag |

They must come in that order: one `ClientHello`, one `ServerHello`, then only `Record`s. A
known message out of order closes the connection.

Anyone on the network path can inject a top-level TLV, so skipped ones must have no effect on
the session and never extend a timeout. New meaning goes in hello fields or records, never in
a new top-level TLV.

### Handshake

The server listens on TCP port 6699, with one connection per session. There's no version or
cipher negotiation in v1. Sizes assume shortest-form varints.

#### 1. Client → server: `ClientHello` (71 bytes)

| Type | Field | Value | Notes |
|---|---|---|---|
| 0 | client_ephemeral_public | 65 bytes | fresh P-256 key, uncompressed |

The server closes the connection if this key isn't on the curve.

#### 2. Server → client: `ServerHello` (71 bytes)

| Type | Field | Value | Notes |
|---|---|---|---|
| 0 | server_ephemeral_public | 65 bytes | fresh P-256 key, uncompressed, never reused |

#### 3. Both sides derive keys

1. `Z = ECDH(own_ephemeral_private, peer_ephemeral_public)`, the X coordinate of the shared
   point as 32 big-endian bytes.
2. `HKDF-SHA256(salt = empty, Z, info = "JazzLightsSecureChannel v1 keys" || ClientHello || ServerHello)`
   gives 56 bytes, in this order: a 16-byte key and a 12-byte IV for client-to-server
   records, then a 16-byte key and a 12-byte IV for server-to-client records.
3. Delete the one-time private key and `Z`.

Here and in `signed_data`, each TLV is complete and exactly as sent, including its Type,
Length and any skipped fields, and each label is ASCII followed by a null byte.

#### 4. Client → server: `ClientReady` (157 bytes alone in a `Record`)

| Type | Field | Value | Notes |
|---|---|---|---|
| 0 | client_long_term_public | 65 bytes | uncompressed |
| 1 | signature | 64 bytes | raw `r \|\| s`, each 32 big-endian bytes: ECDSA P-256 over SHA-256(`signed_data`) |

```
signed_data = "JazzLightsSecureChannel v1 ClientReady"
              || ClientHello
              || ServerHello
              || client_long_term_public TLV
```

The client can send other TLVs right after `ClientReady`, without waiting for `ServerReady`.

#### 5. The server checks the signature

Before `ClientReady`, the server skips TLVs of type 3 and up like unknown ones.

When `ClientReady` arrives, the server compares `client_long_term_public` with each built-in
key, and if it finds a match, verifies the signature with that key. On failure it closes the
connection, with no other error report. A second `ClientReady` also closes the connection.

Until this succeeds, the server sends no encrypted data and handles no TLVs that follow
`ClientReady`.

#### 6. Server → client: `ServerReady` (20 bytes alone in a `Record`)

Its Value is fields, and none are defined yet. The server sends it right after checking the
signature, before handling any TLVs that follow `ClientReady`, and before sending any TLVs of
type 3 and up. Before `ServerReady`, the client skips TLVs of type 3 and up like unknown ones. A
second `ServerReady` closes the connection.

### Records

```
Record Value = AES-128-GCM(key, nonce, plaintext, aad) || tag (16 bytes)
```

- **nonce:** that direction's IV XORed with its record counter, as a 96-bit big-endian
  integer. The counter starts at 0 and goes up by one per record. The connection closes rather
  than letting it wrap.
- **aad:** the `Record`'s Type and Length, exactly as sent.
- **plaintext:** at least one byte of that direction's stream of TLVs. The stream carries on
  from one record to the next, even partway through a Type, Length or Value. TLVs, including
  the ready messages, don't need to line up with records: a record can hold several TLVs, and
  a TLV can span any number of records.

| Type | Name | Direction | Value |
|---|---|---|---|
| 0 | `ClientReady` | client → server | fields, see "Handshake" |
| 1 | `ServerReady` | server → client | fields, see "Handshake" |
| 2 | `Close` | both | fields: `status` (type 0, integer); 0 means no error, 1 means a use of the channel received a malformed TLV. Nothing can follow it, and the receiver closes the connection. |
| 3 and up | defined by each use | | see "Part 2: OTA" for the first ones |

A ready message in the wrong direction closes the connection. A TLV is handled only once every
record holding part of it has passed authentication. If a
record fails authentication, or the connection ends without a `Close`, the session has failed
and the connection closes.

The channel itself reports errors only by closing the connection, without sending a `Close`. Each
use can send a `Close` with a status through `SecureSession`. A session ended cleanly only if a
`Close` with status 0 was sent or received, with nothing already received after it.

### Feature negotiation

Features are negotiated with new TLVs: a peer offers a feature, the other answers, and neither
uses it until it has seen the answer. Offers go in the ready messages or later TLVs, or in the
hellos if they change the key derivation or record format, such as a new version or curve. The
first release defines no features.

### Sessions

- Each session has its own connection, keys and state.
- The server allows 4 connections at once, since each can have up to a TCP window of 5744 bytes
  waiting in lwIP, which takes about 7KB of heap. If it's full, a new connection replaces the
  oldest one still in its handshake. If all are authenticated, the new connection is closed.
- The server closes a connection if the handshake hasn't finished 10s after it accepted it,
  whatever else it receives in that time. That's one deadline, covering `ClientHello` and
  `ClientReady` together.
- The server closes a session after 60s with no authenticated records from the client. A use
  with nothing to send for longer defines its own keepalive TLV.

### Server implementation

- `TlvReader`, `TlvWriter` and `ParseTlvFields` are in `src/jazzlights/network/tlv.h`. They, and the
  interface below, use `uint64_t` for the lengths of TLVs and Values, since that's what a varint can
  hold.
- `SecureChannelServer` is in `src/jazzlights/network/`. One FreeRTOS task serves all connections
  using `poll()` and non-blocking reads, as `Esp32SocketManager` does. It runs at the lowest
  priority above idle, on core 0, with an 8KB stack, so that handshakes don't get in the way of
  rendering or of the network stack. It starts once the network stack is up.
- Each use implements this interface and registers it for its range of types with
  `RegisterHandler()`. Types with no handler are skipped. Handlers run on the shared task, so they
  must not block for long. The channel reassembles a TLV that spans records, so `OnTlv` always gets
  the whole Value.
  ```cpp
  class SecureSession {
   public:
    virtual bool Send(uint64_t type, const uint8_t* value, uint64_t length) = 0;
    virtual void Close(uint64_t status) = 0;
  };

  class SecureChannelHandler {
   public:
    virtual ~SecureChannelHandler() = default;
    virtual void OnTlv(SecureSession& session, uint64_t type, const uint8_t* value, uint64_t length) = 0;
    // Called once for each authenticated session, after it ended and outside of any OnTlv.
    // `clean` is false if the session failed or the connection was lost.
    virtual void OnSessionEnd(SecureSession& session, bool clean) = 0;
  };
  ```
- `Send` puts each TLV in its own record, and splits one that doesn't fit in its 4KB buffer.
  It waits up to 500ms for room in the socket's buffer, and closes the session if the client
  stops reading.
- `CloseAllSessionsAndFlush()` sends a `Close` with status 0 on every authenticated session and
  closes the other connections. It then waits up to 1s for each client to close its side, which
  shows that what was sent has arrived.
- New files are gated with a `JL_SECURE_CHANNEL` macro in `util/config.h`, because `extras/`
  compiles `src/jazzlights/` natively without mbedTLS. The TLV code doesn't need the gate.
- `src/CMakeLists.txt` requires the `mbedtls` component, because without it our sources would
  compile against mbedTLS's default configuration instead of ESP-IDF's, which has a different ABI.
- It uses the classic mbedTLS 3.6 API: `mbedtls_ecp_*`, `mbedtls_ecdh_*`, `mbedtls_ecdsa_verify`,
  `mbedtls_hkdf`, `mbedtls_gcm_*` and `mbedtls_sha256_*`. Randomness comes from `esp_fill_random()`.
  That's only truly random while Wi-Fi or Bluetooth is running, so Ethernet-only builds with
  Bluetooth disabled, such as `gauntlet_esp_ethernet_only`, get weaker session keys. Those builds
  are only used for testing.
  `sdkconfig.defaults` enables `CONFIG_MBEDTLS_HKDF_C`, which the prebuilt Arduino libraries already
  have.
- Memory, allocated per connection:
  - A receive buffer, created when the first bytes arrive. It starts at 256 bytes and grows as
    needed to hold either the `ClientHello`, or a record behind any partial TLV, so up to about
    8.2KB. If it can't grow, the connection closes. TLVs that get skipped are never stored. It grows
    to fit each record as soon as its size is known, rather than doubling.
  - Failed allocations are logged with the free heap and the largest free block.
  - Two GCM contexts, once the keys are derived, and a running SHA-256 of `signed_data`, which
    means the hellos needn't be kept after that.
  - An idle authenticated session takes about 1.7KB of heap in all, including lwIP's.
- The task also has one shared 4KB send buffer. Handling a `ClientHello` or a `ClientReady`
  briefly lowers free heap by up to about 10KB, mostly in mbedTLS, and includes a temporary buffer
  for the HKDF `info`, which is about the size of both hellos. If something can't be allocated,
  the connection closes.
- The channel keeps at least 8KB of heap free for the Wi-Fi and Bluetooth stacks and the rest of
  the firmware. It refuses a new connection unless 2KB more is free, and closes a connection
  instead of handling its `ClientHello` or `ClientReady` unless 12KB more is free, or instead of
  growing its receive buffer unless the whole new size is, since `realloc()` might need that much
  before it frees the old buffer. These are logged.
- The task uses at most about 3.2KB of its 8KB stack, including during an OTA update, and the
  rollback task about 1.8KB of its 4KB.

### Client implementation

- `extras/scripts/secure_channel.py` has a `SecureChannelClient` class and its own TLV encoder
  and parser, using the Python `cryptography` package. It converts the DER signature to raw
  `r || s`.
- Each use has its own script on top of that module.
- A client finds servers by listening for our UDP multicast payloads on `224.0.0.169:6699`, which
  it recognizes by their version in the high nibble of the first byte, and connects once to each
  source IP address it hears. Clients can also be given addresses.
- A device normally stops sending on Wi-Fi or Ethernet while it follows another device it heard on
  that network, since everyone there hears the leader anyway. With `JL_OTA`, it keeps sending there,
  but only every 1.5-2.5s, at random, so that clients can still find it.
  `NetworkManager::SetSendEchoesOccasionally()` turns that on, and each UDP network rate-limits the
  messages it received itself.

## Part 2: OTA

OTA is enabled with a `JL_OTA` macro in `util/config.h`, on by default wherever the secure channel
is. On a device with no OTA partition, `OtaInfo` still works and `OtaBegin` fails.

### TLVs

```
Client                                  Server
|-------------- OtaQuery ------------->|
|<-------------- OtaInfo --------------|
|                                      |  client decides whether to update
|-------------- OtaBegin ------------->|
|                                      |  check image, start the OTA
|<------------ OtaAck (0) -------------|
|-------------- OtaData -------------->|
|-------------- OtaData -------------->|  without waiting for OtaAck
|<------ OtaAck (bytes received) ------|
|                                      |  ... until the whole image is sent
|--------------- OtaEnd -------------->|
|                                      |  check image, set boot partition
|<------------ OtaResult --------------|
```

| Type | Name | Direction | Value |
|---|---|---|---|
| 3 | `OtaQuery` | client → server | fields; none yet |
| 4 | `OtaInfo` | server → client | fields, below |
| 5 | `OtaBegin` | client → server | fields, below |
| 6 | `OtaData` | client → server | bytes: the next bytes of the image. Clients send as many as fit in one record, which is 4077, or fewer at the end, and the server accepts any size up to the maximum Length. |
| 7 | `OtaAck` | server → client | fields: `bytes_received` (type 0, integer). The first, with 0, answers `OtaBegin`, and there's one after each `OtaData`, as soon as the server has taken it, which is before it's written. |
| 8 | `OtaEnd` | client → server | fields; none yet |
| 9 | `OtaResult` | server → client | fields: `status` (type 0, integer), see below. Also answers an `OtaBegin` or `OtaEnd` that can't go ahead. |

A server skips the types that only go from server to client if it receives them.

`OtaResult` statuses:

| Status | Meaning |
|---|---|
| 0 | success |
| 1 | no OTA partition |
| 2 | `image_size` isn't acceptable, or didn't match the data received |
| 3 | `image_chip_id` doesn't match this device |
| 4 | another client is updating |
| 5 | `esp_ota_begin()` failed, such as when the running image isn't marked valid yet |
| 6 | this session isn't updating |
| 7 | `esp_ota_write()` failed, which answers the `OtaData` or `OtaEnd` after the one it failed on |
| 8 | `image_sha256` doesn't match |
| 9 | `esp_ota_end()` failed |
| 10 | wrong `project_name` |
| 11 | `esp_ota_set_boot_partition()` failed |

`OtaInfo` fields:

| Type | Field | Value | Notes |
|---|---|---|---|
| 0 | available_size | integer | size of the `esp_ota_get_next_update_partition(nullptr)` partition, 0 if there's none |
| 1 | software_version | 20 or 52 bytes | SHA-1 of the jazzlights git commit this firmware was built from, then a SHA-256 of the code changes since that commit, omitted if there were none |
| 2 | image_chip_id | integer | `esp_chip_info()` model |
| 3 | build_environment | bytes | PlatformIO environment name this firmware was built with, such as `vest` |
| 4 | mac_address | 6 bytes | the device ID that the synchronization protocol uses, which is its MAC address unless it has none |
| 5 | build_time | integer | when this firmware was built, in seconds since the UNIX epoch, see below |

The client starts by sending `OtaQuery`, and the server answers with `OtaInfo`. The client then
picks the image for the device's `build_environment` and decides whether to update. For example,
it doesn't if the device is already running that version. If it does, it sends `OtaBegin`.
`OtaInfo` is untrusted, so it must only affect which public image the client sends, if any.

`OtaBegin` fields:

| Type | Field | Value | Notes |
|---|---|---|---|
| 0 | image_size | integer | must fit in `esp_ota_get_next_update_partition()` |
| 1 | image_sha256 | 32 bytes | SHA-256 of `firmware.bin` (the app image, not the merged binary) |
| 2 | image_chip_id | integer | must match the `esp_chip_info()` model |
| 3 | reboot_when_done | flag | optional |

A malformed `OtaQuery`, `OtaBegin` or `OtaEnd` makes the server send a `Close` with status 1. When
`OtaBegin` arrives, the server sends a failed `OtaResult` unless all of these hold:

- `esp_ota_get_next_update_partition(nullptr)` isn't null.
- `image_size` isn't 0 and fits in that partition, and `image_chip_id` matches.
- No client is already updating. That includes this session, whose update then carries on.

The server doesn't compare versions, so it accepts upgrades and downgrades alike. Otherwise it calls `esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &handle)`, and a
separate task writes each `OtaData` with `esp_ota_write()` while the channel's task receives the
next one. `OtaData` from a session that isn't updating is
discarded, and `OtaEnd` from one gets a failed `OtaResult`.

If the session ends before the update finishes, the server calls `esp_ota_abort()`. If a write
or a check under "Finishing" fails, or the data goes past `image_size`, it also sends a failed
`OtaResult`, and the session is no longer updating, though it stays open.

### Finishing

When `OtaEnd` arrives, the server checks that:

1. Exactly `image_size` bytes were received.
2. The running SHA-256 of the plaintext equals `image_sha256`.
3. `esp_ota_end()` succeeds, which also checks the image's chip.
4. The new image's `esp_app_desc_t` has the same `project_name` as the running firmware. That's
   `arduino-lib-builder` on Arduino builds and `jazzlights` on `espidf, arduino` builds.

It then calls `esp_ota_set_boot_partition()` and sends a successful `OtaResult`. If
`reboot_when_done` was present, it sends `Close` on every session, flushes, and calls
`esp_restart()`.

### Rollback

`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` is already in both the prebuilt Arduino libraries and
our `sdkconfig.defaults`.

- Where the Arduino core is used, overriding its weak `verifyRollbackLater()` to return `true`
  makes `initArduino()` leave a new image in `ESP_OTA_IMG_PENDING_VERIFY`. Without the Arduino
  core, `main.cpp` leaves it that way itself.
- Once the new firmware has rendered at least one frame and has stayed connected to the network
  for 30s without interruption, `OtaRollbackGuard` calls
  `esp_ota_mark_app_valid_cancel_rollback()` from the primary runloop. Until then `esp_ota_begin()`
  fails, so `OtaBegin` gets a failed `OtaResult`.
- If the new firmware hasn't done that within 10 minutes of booting, `OtaRollbackGuard` calls
  `esp_ota_mark_app_invalid_rollback_and_reboot()`. So firmware that's broken in a way that keeps it
  running can still be replaced, and a device that's simply out of reach of its network for those
  10 minutes also goes back to its previous firmware. Clients see their sessions fail, with no
  `Close`.
- That timer has its own small task, so a stuck primary runloop can't stop it. The task only exists
  until the firmware is confirmed.
- If there's no other firmware to roll back to, the firmware carries on, and can still be confirmed
  later.
- The bootloader also rolls back if the firmware reboots for any other reason before it's confirmed,
  such as by crashing.

The prebuilt bootloaders (`framework-arduinoespressif32-libs/*/bin/bootloader_*.elf`) have rollback
support on ESP32, ESP32-S3, ESP32-C3 and ESP32-C6. Other builds compile their own bootloader from
our sdkconfig.

### Server implementation

- `OtaHandler` is in `src/jazzlights/network/`, implementing `SecureChannelHandler`. During an
  update it needs a SHA-256 context, and a writer task with a 2.5KB stack, of which it uses about
  1KB, and a 4KB buffer. Writing 4KB takes 64-75ms, mostly erasing a flash sector, which is about
  as long as receiving it. With the writer task, the channel's task copies each `OtaData` into the
  buffer, acknowledges it and goes back to receiving, and only waits for the writer if the previous
  `OtaData` is still being written.
- `extras/scripts/software_version.py` is a `pre:` extra script for every environment, which
  computes `software_version` when building the firmware, and saves it as `software_version.bin`
  next to `firmware.bin`. The code changes are `git diff HEAD --binary` followed by the name and
  contents of any untracked files that aren't ignored, sorted by name. Without git, the version is
  20 zero bytes. It generates one header in the build directory with this, `build_environment` and
  `build_time`, which only `ota_handler.cpp` includes, so that a change doesn't recompile everything.
  `build_time` is when this `software_version` was first built for this environment in this build
  directory, so that building again without changes doesn't recompile anything.
- The update doesn't change what the LEDs show.
- During an update, `OtaHandler` calls a callback that `primary_runloop.cpp` uses to pause BLE.
  `Esp32BleNetwork` and `Esp32NimBleNetwork` then stop scanning and advertising until the update
  ends. They scan continuously, and the radio they share with Wi-Fi made updates take 60-75s on
  `vest` instead of 42-47s. Devices still synchronize over Wi-Fi meanwhile.

### Client implementation

- `extras/scripts/ota_upload.py` builds on `secure_channel.py`, and runs until interrupted. Usage:
  `ota_upload.py --key <pem>`, or `JL_CLIENT_KEY` in place of `--key`. Options:
  - `--environment <env>` leaves devices running any other environment alone.
  - `--mac <address>`, which can be repeated, leaves devices with any other MAC address alone, as
    `OtaInfo` reports it. With `--environment` as well, those devices get that environment even if
    they're running another one, like a serial upload would do. Since the software version is the
    same for every environment built from one checkout, a device that runs another environment
    gets updated even if its software version matches.
  - `--no-build` uses what's already in `.pio/build/<env>` instead of building.
  - `--once` listens for devices for 5s, or `--discovery-seconds`, updates them, and exits.
  - `--host <address>`, which can be repeated, updates those devices instead of discovering any,
    and exits.
  - With `--once` or `--host`, it prints what happened to each device, and exits with a failure
    status if an update failed or it found nothing to update. A device that refuses the
    connection, since its firmware has no secure channel or no keys, is skipped.
- `extras/scripts/ota_target.py` adds an "OTA Upload" task to every ESP32 environment, which the
  pioarduino extension for VS Code shows under Project Tasks, then the environment, then Custom.
  It builds the environment, then runs `ota_upload.py --environment <env> --no-build --once`, or
  `--host` with the address given by `--upload-port` or `JL_OTA_HOST`. From the command line, that's
  `pio run -e vest -t ota`. It needs `JL_CLIENT_KEY`, and runs `ota_upload.py` with `uv`, since
  PlatformIO's own Python doesn't have the `cryptography` package. An environment that's for one
  particular device, like the `ds33_` ones, gives its MAC address with `custom_ota_mac_address` in
  `platformio.ini`, which the task passes as `--mac`.
- For each server it discovers, it works in this order, with builds running one at a time:
  1. Connect and send `OtaQuery`.
  2. Close the session if `build_environment` isn't an environment in `platformio.ini`. Otherwise
     run `pio run -e <build_environment>`, and close the session if that fails. While waiting for
     the build, send `OtaQuery` every 30s to keep the session open, and ignore the answers.
  3. Close the session if `software_version` matches the `software_version.bin` next to
     `.pio/build/<build_environment>/firmware.bin`, if that image is larger than `available_size`,
     or if its chip doesn't match `image_chip_id`. The chip is the 16-bit little-endian `chip_id`
     at byte 12 of `firmware.bin`. It has the same values as `image_chip_id`, except that ESP32 is
     0 there and 1 in `esp_chip_info()`.
  4. Send `OtaBegin` with `reboot_when_done`. Once its `OtaAck` arrives, send a
     `PatternOverride`, see below, then the image and `OtaEnd`, and wait for
     `OtaResult`. It sends the image without waiting for each `OtaAck`, and leaves TCP's flow
     control to hold it back: waiting for acknowledgments, with up to 4 outstanding, was no
     faster, since the device's 5744-byte TCP receive window and its flash writes are what limit
     the speed. It only sends what the socket takes without blocking, and reads each `OtaAck` as
     it arrives, which it checks against the bytes sent. So a full buffer on either side can't
     leave both waiting for the other. It gives up if the device takes no data and acknowledges
     none for 30s, and stops as soon as an `OtaResult` reports a failure.
- It logs its progress every 10% of the image, with the rate, and again when it has sent `OtaEnd`,
  which is when the device checks the image and switches which partition it boots from.
- The `PatternOverride` makes the device show solid purple, `0x00000400`, during the update, since
  flash writes disrupt the LED output and a pattern that doesn't change might make that less
  visible. `--update-pattern` picks other pattern bits, or `none` sends no `PatternOverride`.

## Pattern override

A `PatternOverride` TLV, of type 10, makes the device show one pattern on its LEDs instead of the
synchronized ones. It's on wherever the secure channel is, and isn't part of OTA.

- A 4-byte Value holds the pattern bits, in network byte order. The device shows that pattern
  until the session that sent it ends, or until a `PatternOverride` from any session ends it.
- An empty Value ends the override, and does nothing if there's none.
- Any other length is malformed, and closes the session.
- The latest `PatternOverride` wins, and its session is the one whose end ends the override.
- It only changes this device's LEDs, not what it sends to other devices, and has no effect while
  the device's LEDs are turned off. `Player::SetPatternOverride()` implements it, and can be called
  from any task.
- There's no answer. Firmware from before `PatternOverride` skips it, since it has no handler.

## Binary size

Flash and static RAM added to each environment:

| Environment | Channel flash | Channel RAM | OTA flash | OTA RAM | Total flash |
|---|---|---|---|---|---|
| `vest` | 18,244 | 136 | 6,572 | 280 | 24,816 |
| `vest_c6` | 19,574 | 144 | 7,778 | 280 | 27,352 |
| `vest_espidf` | 16,300 | 152 | 6,568 | 280 | 22,868 |

With all of it, `vest` uses 91.5% of its app partition, `vest_c6` 95.1%, and `vest_espidf` 59.4%.

On `vest`, about 11KB of the channel is its own code. The rest is what it newly pulls in, mainly
AES-GCM, and lwIP's and the VFS's code for `listen()`, `accept()` and `poll()`. mbedTLS's ECP,
bignum, ECDH, ECDSA and SHA-256 are already linked for Wi-Fi WPA3, and `app_update` already is. At
run time, the channel's task adds its 8KB stack and 4KB send buffer, which firmware with no keys
doesn't allocate. During an update, the OTA writer task adds a 2.5KB stack and a 4KB buffer. The
rollback task adds a 4KB stack, only until new firmware is confirmed.

## Testing

- `extras/secure_channel_test` builds the real `SecureChannelServer` and `OtaHandler` natively,
  under ASan and UBSan, against the mbedTLS version in ESP-IDF. Headers in its `shim/` directory
  stand in for ESP-IDF, FreeRTOS and lwIP, keep the flash in memory, and report whatever free
  heap a test picks.
- `extras/scripts/test_secure_channel.py` runs against that server, and CI runs it too. Setting
  `JL_TEST_HOST`, `JL_TEST_KEY` and `JL_TEST_KEY2` runs it against a real device instead, skipping
  the tests that need the fake flash or the fake heap. The handshake timeout, idle timeout and
  rollback guard tests wait for timers, which takes over a minute and a half, so they only run
  with `JL_TEST_TIMEOUTS=1`, which CI sets.
- `test/test_tlv` is a native test of the TLV code, which `pio test -e native` runs.
- `secure_channel.py` can act as a fake server, and can send malformed messages to a real one.
- An `Echo` TLV of type 1000 sends its Value back, which checks the channel without touching flash.
  It's only compiled in with `JL_SECURE_CHANNEL_ECHO`, which the `vest_secure_channel_test`
  environment sets for use on hardware.

### Covered natively

- Malformed input:
  - varints: longer than needed (accepted), cut off
  - Lengths: past their container; exactly the maximum and over it, for a `ClientHello`, other
    top-level TLVs, records and TLVs in the stream; fields larger than 4096 in a container
  - fields: repeated, wrong size, missing; unknown at every level (skipped)
  - top-level: a known message out of order; injected data, which must not extend a timeout
  - tampering: a hello with a changed skipped field or re-encoded varint; a `Record` with its
    Type or Length re-encoded
  - ready messages: split across records; sharing a record with other TLVs; repeated; in the
    wrong direction; after an unknown TLV; after a TLV of type 3 or up, which must be skipped
  - records: several TLVs in one; a TLV split at every possible byte; an unknown TLV of 4096
    bytes spanning records; no plaintext; anything after `Close`
- The channel:
  - handshake: happy path; wrong private key; a public key not in the firmware; `ClientHello`
    and `ClientReady` replayed; `client_long_term_public` changed after signing
  - TLVs right behind `ClientReady`: handled after `ServerReady`, and not acted on if the
    signature is bad
  - records: a flipped bit, including in the last record of a spanning TLV; a stream cut short
    without a `Close`, including partway through a TLV
  - sessions: a silent client; 4 clients at once; one failing while others carry on; `Echo`
    interleaved with an OTA update; all 4 connections taken, by sessions and by unfinished
    handshakes; a flood of new connections during a session; the handshake and idle timeouts
  - limits: a `ClientHello` of exactly 8192 bytes and a `ClientReady` of exactly 4096, with large
    skipped fields;
    endless skipped TLVs instead of `ClientReady`, which must be closed after 10s
  - heap, with fake heap: too little for a new connection, a `ClientHello`, or a receive buffer
    that fits the largest `ClientHello`, which must each be closed; and just enough for the
    largest `ClientHello` and `Echo`
- `PatternOverride`: setting, changing and ending it; ending it when there's none; its session
  ending, cleanly or not; another session ending, which must leave it; the latest session owning
  it; and every wrong length, which must close the session
- The OTA client:
  - `software_version.py` on a clean checkout, a changed tracked file, a new untracked file and an
    ignored file
  - a fake server sending a `build_environment` that isn't in `platformio.ini`, such as `../..`,
    which must make the client close the session
  - a build that fails, which must make the client close the session and carry on with others
  - discovery: one connection per server despite repeated payloads, and no second connection
    after a server reboots
  - a build that outlasts the idle timeout, which must keep the session open
  - builds running one at a time
  - an image that's too large or for the wrong chip, or that's already running
- OTA, with fake flash:
  - `OtaQuery`, and with no OTA partition
  - a client that skips the update because the versions match
  - a client that sends a whole image, which must arrive unchanged
  - two clients sending `OtaBegin` at once
  - `OtaData` or `OtaEnd` without an `OtaBegin`, and `OtaBegin` before the running image is
    marked valid
  - a client failing mid-update, after which another client can update
  - a reboot, which must send `Close` to the other clients
  - too little or too much data, a wrong SHA-256, an image for the wrong chip or project
- Rollback, with fake flash and shorter times:
  - firmware that works, which must get confirmed and must not roll back
  - no network, no rendering and a stuck primary runloop, which must each roll back on time
  - nothing to roll back to, which must carry on, and can still get confirmed later

### Tested on an ATOM Matrix

These ran over Wi-Fi, against `vest_secure_channel_test` unless stated otherwise:

- `test_secure_channel.py`: the 90 tests that can run on hardware pass.
- A handshake takes about 1s.
- An OTA update of a 1.8MB image from `ota_upload.py`'s update logic takes 38-40s, about
  46KB/s. That was 42-47s when the channel's task wrote each `OtaData` itself, and 60-75s before
  BLE was paused during updates. The device rebooted into it, and marked it valid 30s after connecting to
  Wi-Fi.
- With the new image running, the client skips the update because the versions match.
- LEDs keep rendering during an update. On `vest`, the computed frame rate drops from about 88 to
  50-83 FPS, the written one from 71 to 44-68 FPS, and the longest gap between frames goes from
  under 55ms to up to 71ms.
- An image that never marks itself valid rolls back after 10 minutes, to the previous image.
- An image that crashes at boot rolls back to the previous image on the next boot.
- Cutting the power, by unplugging USB, leaves the running image in place and lets the next update
  succeed, whether it happens just after the transfer starts, partway through it, or after the last
  `OtaData` while the device checks the image. So does a reset through the EN pin partway through
  a transfer.
- An ESP32-S3 image sent with an `OtaBegin` that claims the ESP32's chip gets rejected by
  `esp_ota_end()`, with status 9.
- `vest_espidf` (`framework = espidf, arduino`) and `vest_idf` (`framework = espidf`) update
  themselves in 32s and 27s, and mark the new image valid 30s after booting.
- Discovery finds a device that follows another one over Wi-Fi, which sends every 1.7-2.5s, and the
  device leading it. The OTA Upload task without an address found both, saw that the follower was
  up to date, and skipped the leader, whose firmware has no secure channel.
- The OTA Upload task, run as `pio run -e vest -t ota --upload-port 10.41.41.21`, built `vest` and
  updated the device in 41s.
- `pio run -e ds33_dev_a -t ota`, whose environment has device A's MAC address, updated device A in
  28s and left the ATOM Matrix running `vest` alone. `ota_upload.py --environment vest --mac` then
  put `vest` on device A, and the task put `ds33_dev_a` back. Until a new image is marked valid,
  30s after it has a connection, the next update fails with status 5.
- Memory on `vest`, which runs Bluedroid and Wi-Fi on a classic ESP32: about 38KB of heap is free
  with no connections, and `vest_espidf` and `vest_idf` have about 85KB. 4 idle sessions take
  about 7KB. 4 sessions that each hold a partial 4096-byte TLV don't all fit on `vest`: 2 of them
  close because the heap floor stops their receive buffers from growing, and the device carries
  on. Free heap still briefly drops to between 2KB and 8KB, because lwIP holds what arrives
  before the channel reads it, which the floor doesn't cover.

On macOS, the first connections from a Python that hasn't been allowed on the local network fail
with "No route to host". Allowing it in the Local Network privacy settings fixes that.
