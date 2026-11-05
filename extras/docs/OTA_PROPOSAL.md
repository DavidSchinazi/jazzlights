# Proposal: Secure Channel, with OTA as the First Use

Status: proposal, not implemented.

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
  in both `framework = arduino` and `framework = espidf, arduino` builds.
- Works on the local network, without GitHub or the public Internet.
- Keep the binary size increase as small as possible.

## Non-goals

- Authenticating the server to the client, now or later.
- Securing the existing UDP multicast protocol.
- Hardware Secure Boot or flash encryption, which permanently burn eFuses.
- Environments without an IP network (NimBLE-only, `JL_WIFI=0`), which keep updating over
  serial.

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
- To rotate or revoke keys, ship firmware with a new list. If every private key is lost, fall
  back to a serial flash.

### Encoding

Everything on the wire is a TLV:

```
TLV = Type (varint) || Length (varint) || Value (Length bytes)
```

Type and Length are QUIC variable-length integers (RFC 9000, section 16). Any varint size is
valid, and our senders use the shortest form. The maximum Length is 16384.

A Value is one of:

- **Fields:** nested TLVs. Each container numbers its own types.
- **Integer:** a varint that fills the Value exactly.
- **Bytes:** used as is.
- **Flag:** empty. Being present is what counts.

Parser rules:

- Unknown types are skipped at every level, without being stored.
- These are errors, which close the connection: a Length over the maximum or past the end of
  its container, a known field that's repeated or the wrong size, and a missing required field.
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

Its Value is fields, and none are defined yet. The server sends it before handling any TLVs
that follow `ClientReady`, and before sending any TLVs of type 3 and up. Before `ServerReady`,
the client skips TLVs of type 3 and up like unknown ones. A second `ServerReady` closes the
connection.

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
| 2 | `Close` | both | fields: `status` (type 0, integer); 0 means no error. Nothing can follow it, and the receiver closes the connection. |
| 3 and up | defined by each use | | see "Part 2: OTA" for the first ones |

A ready message in the wrong direction closes the connection. A TLV is handled only once every
record holding part of it has passed authentication. If a
record fails authentication, or the connection ends without a `Close`, the session has failed
and the connection closes.

### Feature negotiation

Features are negotiated with new TLVs: a peer offers a feature, the other answers, and neither
uses it until it has seen the answer. Offers go in the ready messages or later TLVs, or in the
hellos if they change the key derivation or record format, such as a new version or curve. The
first release defines no features.

### Sessions

- Each session has its own connection, keys and state.
- The server allows 8 connections at once. If it's full, a new connection replaces the oldest one still in its handshake. If all are authenticated, the new
  connection is closed.
- The server closes a connection if `ClientHello` or `ClientReady` hasn't arrived within 10s,
  whatever else it receives in that time.
- The server closes a session after 60s with no records from the client. A use
  with nothing to send for longer defines its own keepalive TLV.

### Server implementation

- Add `TLVReader` and `TLVWriter` in `src/jazzlights/network/tlv.h`.
- Add `SecureChannelServer` in `src/jazzlights/network/`. One FreeRTOS task serves all
  connections using `select()` and non-blocking reads, as `Esp32SocketManager` does.
- Each use implements this interface and registers it for its range of types. Types with no
  handler are skipped. Handlers run on the shared task, so they must not block for long. The
  channel reassembles a TLV that spans records, so `OnTLV` always gets the whole Value.
  ```cpp
  class SecureSession {
   public:
    bool Send(uint64_t type, const uint8_t* value, size_t length);
    void Close(uint64_t status);
  };

  class SecureChannelHandler {
   public:
    virtual ~SecureChannelHandler() = default;
    virtual void OnTLV(SecureSession& session, uint64_t type, const uint8_t* value, size_t length) = 0;
    // `clean` is false if the session failed or the connection was lost.
    virtual void OnSessionEnd(SecureSession& session, bool clean) = 0;
  };
  ```
- `Send` puts each TLV in its own record, and splits one that doesn't fit in its 4KB buffer.
  It waits only briefly for room in the socket's buffer, and closes the session if the client
  stops reading.
- Gate new files with a `JL_SECURE_CHANNEL` macro in `util/config.h`, because `extras/` compiles
  `src/jazzlights/` natively without mbedTLS. The TLV code doesn't need the gate.
- Use the classic mbedTLS 3.6 API: `mbedtls_ecp_*`, `mbedtls_ecdh_*`, `mbedtls_ecdsa_verify`,
  `mbedtls_hkdf`, `mbedtls_gcm_*` and `mbedtls_sha256_*`. Get randomness from
  `esp_fill_random()`.
- Memory, allocated per connection:
  - A receive buffer, created when the TCP connection is accepted. It starts small and grows
    as needed to hold a hello, a record and any partial TLV, so up to about 32KB. If it can't
    grow, the connection closes.
  - Two GCM contexts, once the keys are derived, and a running SHA-256 of `signed_data`, which
    means the hellos needn't be kept after that.
- The task also has one shared 4KB send buffer, and a few KB of temporary mbedTLS heap during
  each handshake.

### Client implementation

- `extras/scripts/secure_channel.py` has a `SecureChannelClient` class and its own TLV encoder
  and parser, using the Python `cryptography` package. It converts the DER signature to raw
  `r || s`.
- Each use has its own script on top of that module.
- A client finds servers by listening for our UDP multicast payloads on `224.0.0.169:6699`, and
  connects once to each source IP address it hears.

## Part 2: OTA

OTA is enabled with a `JL_OTA` macro in `util/config.h`, on by default when there's an IP
network (Wi-Fi or Ethernet).

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
|-------------- OtaData -------------->|  at most 4 unacknowledged
|<------ OtaAck (bytes written) -------|
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
| 6 | `OtaData` | client → server | bytes: the next 4096 bytes of the image, or fewer at the end |
| 7 | `OtaAck` | server → client | fields: `bytes_written` (type 0, integer). The first, with 0, answers `OtaBegin`, and there's one after each `OtaData`. |
| 8 | `OtaEnd` | client → server | fields; none yet |
| 9 | `OtaResult` | server → client | fields: `status` (type 0, integer); 0 means success. Also answers an `OtaBegin` or `OtaEnd` that can't go ahead. |

`OtaInfo` fields:

| Type | Field | Value | Notes |
|---|---|---|---|
| 0 | available_size | integer | size of the `esp_ota_get_next_update_partition(nullptr)` partition, 0 if there's none |
| 1 | software_version | 20 or 52 bytes | SHA-1 of the jazzlights git commit this firmware was built from, then a SHA-256 of the code changes since that commit, omitted if there were none |
| 2 | image_chip_id | integer | `esp_chip_info()` model |
| 3 | build_environment | bytes | PlatformIO environment name this firmware was built with, such as `vest` |

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

When `OtaBegin` arrives, the server sends a failed `OtaResult` unless all of these hold:

- `esp_ota_get_next_update_partition(nullptr)` isn't null.
- `image_size` and `image_chip_id` are acceptable.
- No client is already updating.

The server doesn't compare versions, so it accepts upgrades and downgrades alike. Otherwise it calls `esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &handle)`, and writes
each `OtaData` with `esp_ota_write()`. `OtaData` from a session that isn't updating is
discarded, and `OtaEnd` from one gets a failed `OtaResult`.

If the session ends before the update finishes, the server calls `esp_ota_abort()`. If a write
or a check under "Finishing" fails, it also sends a failed `OtaResult`.

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

- Override the core's weak `verifyRollbackLater()` to return `true`, so `initArduino()` leaves
  a new image in `ESP_OTA_IMG_PENDING_VERIFY`.
- Once the new firmware has rendered LEDs and connected to the network for 30s, call
  `esp_ota_mark_app_valid_cancel_rollback()`. Until then `esp_ota_begin()` fails, so `OtaBegin`
  gets a failed `OtaResult`.

To check: the prebuilt bootloader (`framework-arduinoespressif32-libs/*/bin/bootloader_*.elf`)
was built with rollback support. The `espidf, arduino` builds compile their own bootloader from
our sdkconfig.

### Server implementation

- Add `OTAHandler` in `src/jazzlights/network/`, implementing `SecureChannelHandler`. It needs a
  SHA-256 context during an update.
- Add `extras/scripts/software_version.py` as a `pre:` extra script, which computes
  `software_version` when building the firmware, and saves it as `software_version.bin` next to
  `firmware.bin`. The code changes are `git diff HEAD` and any untracked files that aren't
  ignored. It generates one source file with this and `build_environment`, so that a change
  doesn't recompile everything.
- The update doesn't change what the LEDs show.

### Client implementation

- `extras/scripts/ota_upload.py` builds on `secure_channel.py`, and runs until interrupted. Usage:
  `ota_upload.py --key <pem>`.
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
  4. Send `OtaBegin` with `reboot_when_done`, then the image and `OtaEnd`, and wait for
     `OtaResult`.

## Binary size

Measure on `vest`, `vest_c6` and `vest_espidf`, with the channel and OTA separately. mbedTLS's
ECP, bignum, ECDH, ECDSA, GCM and SHA-256 are in the link map of `vest`, mostly for Wi-Fi WPA3,
though some may be dropped as unused today. `app_update` is already linked.

## Testing

- `secure_channel.py` can act as a fake server, and can send malformed messages to a real one.
- A small `Echo` TLV, compiled only into test builds, checks the channel without touching flash.
- Malformed input, in those modes and in a native test of `TLVReader`:
  - varints: longer than needed (accepted), cut off
  - Lengths: past their container; exactly 16384 and over it, at every level
  - fields: repeated, wrong size, missing; unknown at every level (skipped)
  - top-level: a known message out of order; injected data, which must not extend a timeout
  - tampering: a hello with a changed skipped field or re-encoded varint; a `Record` with its
    Type or Length re-encoded
  - ready messages: split across records; sharing a record with other TLVs; repeated; in the
    wrong direction; after an unknown TLV; after a TLV of type 3 or up, which must be skipped
  - records: several TLVs in one; a TLV split at every possible byte; an unknown TLV of 16384
    bytes spanning records; no plaintext; anything after `Close`
- The channel, on hardware:
  - handshake: happy path; wrong private key; a public key not in the firmware; `ClientHello`
    and `ClientReady` replayed; `client_long_term_public` changed after signing
  - TLVs right behind `ClientReady`: handled after `ServerReady`, and not acted on if the
    signature is bad
  - records: a flipped bit, including in the last record of a spanning TLV; a stream cut short
    without a `Close`, including partway through a TLV
  - sessions: a silent client; 8 clients at once; one failing while others carry on; `Echo`
    interleaved with an OTA update; all 8 connections taken, by sessions and by unfinished
    handshakes; a flood of new connections during a session
  - limits: a `ClientHello` and a `ClientReady` of exactly 16384 bytes with large skipped fields;
    many connections growing their buffers until allocation fails, which must only close those
    connections; endless skipped TLVs instead of `ClientReady`, which must be closed after 10s
- The OTA client:
  - `software_version.py` on a clean checkout, a changed tracked file and a new untracked file
  - a fake server sending a `build_environment` that isn't in `platformio.ini`, such as `../..`,
    which must make the client close the session
  - a build that fails, which must make the client close the session and carry on with others
  - discovery: one connection per server despite repeated payloads, and no second connection
    after a server reboots
  - a build that outlasts the idle timeout, which must keep the session open
- OTA, on hardware:
  - `OtaQuery` on a build with and without code changes, and with no OTA partition
  - a client that skips the update because the versions match
  - a client that builds and sends the image for the device's `build_environment`
  - two clients sending `OtaBegin` at once
  - `OtaData` or `OtaEnd` without an `OtaBegin`, and `OtaBegin` before the running image is
    marked valid
  - a client failing mid-update, after which another client can update
  - a reboot, which must send `Close` to the other clients
  - a power cut during the transfer
  - LEDs showing their normal patterns throughout an update
  - an image for the wrong chip
  - an image that crashes at boot, or never marks itself valid, which must roll back
- Measure the transfer time for a full 1.7MB image, how long a handshake takes on classic ESP32,
  how much handshakes and flash writes delay other sessions and LED rendering, and how much memory
  each connection costs.
