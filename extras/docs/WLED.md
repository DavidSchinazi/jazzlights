# JazzLights Usermod for WLED

Status: draft design, for review. Nothing here is implemented yet.

[WLED](https://github.com/wled/WLED) is popular firmware for addressable LEDs. This doc
plans a WLED **usermod** (WLED's plugin mechanism) that lets a WLED device take part in
JazzLights synchronization, so that a WLED device and a vest can show matching patterns.

The division of work:

- **WLED** connects to Wi-Fi, generates patterns, drives the LED strips, and provides the
  web UI, presets, playlists and everything else WLED users expect.
- **The usermod** runs the JazzLights protocol over IP (UDP multicast) and Bluetooth Low
  Energy, and converts between WLED's state and JazzLights pattern bits.

## Goals

1. **Leader (primary goal).** When the WLED device leads, other JazzLights devices show the
   JazzLights pattern closest to what WLED is showing. Close is enough. It doesn't have to
   be exact.
2. **Follower (secondary goal).** When a JazzLights device with higher precedence is
   around, the WLED device shows something close to that device's pattern.
3. **A mode setting** chooses between leader and follower. All it changes is the device's
   JazzLights precedence, so the protocol's existing election decides who leads.
4. **No protocol change is required** for goals 1 to 3. Protocol changes that would convey
   WLED patterns better are discussed separately, in
   [Possible protocol changes](#possible-protocol-changes), because updating every deployed
   device is expensive.
5. Changes to WLED itself are kept to zero. Everything goes through the usermod API.

## Non-goals

- Conveying brightness, speed or effect parameters to other JazzLights devices. The protocol
  has no field for them. Each device keeps its own brightness.
- Mirroring several WLED segments. The usermod follows one segment (see
  [Segments](#segments)).
- Replacing WLED's own sync (UDP notifier, DDP, E1.31, ESP-NOW). Those keep working
  alongside JazzLights, and are still the best way to sync WLED devices with each other.

## Background: what JazzLights can express today

The protocol is in [wire.cpp](../../src/jazzlights/protocol/wire.cpp) and the election in
[engine.cpp](../../src/jazzlights/protocol/engine.cpp). Each device advertises a message
whose relevant fields are:

- `precedence` (16 bits): the device with the highest precedence leads, with ties broken by
  device ID. A device's precedence is a base value, plus a gain that decays over 10 minutes
  after the last local user input.
- `currentPattern` and `nextPattern` (32 bits each), and `currentPatternStartTime`. Patterns
  last 10 seconds (`kEffectDuration`), after which everyone moves to `nextPattern`. A device
  that loops repeats the same pattern, restarting it every 10 seconds.

All visual information is in the 32 **pattern bits**. Bit numbers below follow
`PatternBit()`, which counts from 1 at the most significant bit. Decoding is in
`PatternFromBits()` in [player.cpp](../../src/jazzlights/render/player.cpp).

| Pattern bits | Effect | Palette? |
|---|---|---|
| Low nibble ≠ 0, bits 1-2 = `11` | spin plasma (`sp`). Hiphotic on all-linear devices. | yes |
| Low nibble ≠ 0, bits 1-2 = `10` | hiphotic | yes |
| Low nibble ≠ 0, bits 1-2 = `01` | flame. Rings on all-linear devices and motor devices. | yes |
| Low nibble ≠ 0, bits 1-2 = `00` | rings | yes |
| `0x000000_0` type 0, byte `0x00`-`0x07` | solid black, red, green, blue, purple, cyan, yellow, white | no |
| type 0, byte `0x15` | solid warm white | no |
| type 0, bytes `0x08`-`0x0E`, `0x16` | glow (slow breathing) in red, green, blue, purple, cyan, yellow, white, warm | no |
| type 0, bytes `0x12`, `0x13`, `0x14` | glitter, the-matrix, threesine | no |
| type 0, bytes `0x0F`, `0x10`, `0x11` | sync-test, calibration, follow-strand | no |
| type 2: `RRGGBB20` | **any solid RGB color** ("coloring") | no |
| type 3, subtype 0, bit 1 = 1 / 0 | metaballs / colored-bursts | yes |

The palette is in bits 17-19, as written by `ApplyPalette()`: 0 heat, 1 lava, 2 ocean,
3 cloud, 4 party, 5 forest, 6 and 7 rainbow. These are FastLED's stock palettes.

The regular rotation only ever produces the first four rows (four effects times seven
palettes). Everything else is only shown when a device selects it explicitly, but every
device on current firmware renders it.

Three things make WLED a good fit:

- WLED ships the same FastLED palettes: Party, Cloud, Lava, Ocean, Forest and Rainbow are
  palettes 6 to 11. WLED's Party and Rainbow are gamma-corrected variants, so they're close
  but not identical. WLED has no Heat palette, and its Fire palette (35) is the nearest.
- Three JazzLights effects were ported from the Sound Reactive fork of WLED: hiphotic,
  metaballs and colored-bursts. WLED's 2D Hiphotic uses literally the same formula,
  `sin8(cos8(x·sx + t/3) + sin8(y·sy + t/4) + t)`.
- Type 2 encodes any solid color exactly, which covers WLED's most used effect, Solid.

## Architecture

```
            WLED firmware                           JazzLights usermod
 +----------------------------------+    +-------------------------------------+
 | Wi-Fi / Ethernet (esp_netif)  ---+--->| Esp32SocketManager (UDP multicast)  |
 | web UI, presets, playlists       |    | Esp32NimBleNetwork (BLE adverts)    |
 | segments, effects, palettes   ---+--->| WledToJazzLights() mapping          |
 | LED drivers                   <--+----| JazzLightsToWled() mapping          |
 +----------------------------------+    | NetworkManager + ProtocolEngine     |
                                         +-------------------------------------+
```

The usermod reuses JazzLights' transport and protocol code as is, without the `Player`, the
renderer, or FastLED:

- `ProtocolEngine` is already a pure state machine that knows nothing about rendering. The
  usermod implements its `Delegate`.
- `NetworkManager` aggregates the transports.
- `Esp32SocketManager` runs UDP multicast on WLED's Wi-Fi interface. This is exactly what
  the ESPHome component does in
  [jazzlights_component.cpp](../esphome/components/jazzlights/jazzlights_component.cpp):
  wait for the `esp_netif` of the station interface, then call `AddInterface()`. When WLED
  uses Ethernet, its interface is added as `NetworkType::kEthernet`.
- `Esp32NimBleNetwork` for Bluetooth (see [Bluetooth](#bluetooth)).

None of `src/jazzlights/protocol`, `network` or `util` include FastLED, which matters
because WLED has its own `CRGB` and its own copies of the FastLED palettes (in
`fastled_slim`), which would clash with FastLED's.

The usermod's `loop()` does what `Player::Render()` does for the protocol: drain received
messages into the engine, call `CheckLeaderAndPattern()`, and hand the message to send to
the `NetworkManager`. The mapping code runs only when WLED's state or the followed pattern
changes, not every loop.

### Build and packaging

WLED's main branch now builds on ESP-IDF 5.5 with Arduino core 3.3 (Tasmota's platform),
and usermods are PlatformIO libraries listed in `custom_usermods`. That setting accepts
external sources such as git URLs and `symlink://` paths, so the usermod can live outside
the WLED tree.

The usermod lives in this repo under `extras/wled/`, like `extras/esphome/`.
A user builds WLED with, for example:

```ini
[env:esp32s3_jazzlights]
extends = env:esp32s3dev_8MB_opi
custom_usermods = ${env:esp32s3dev_8MB_opi.custom_usermods} jazzlights = symlink://../jazzlights/extras/wled
```

The build's release name has to change too, so that WLED's own updates don't silently
replace it with stock WLED. See [Firmware updates](#firmware-updates).

`extras/wled/library.json` would compile the usermod's own files plus the needed subset of
`src/jazzlights` (via `srcFilter`), and set `JL_CONFIG=WLED` (a new config, like the other
`JL_CONFIG_*` values) along with `JL_WIFI`, `JL_ESP32_NIMBLE` and the other build flags.

To verify during implementation: PlatformIO can't take a subdirectory of a git repository
as a library, so consuming it straight from GitHub may need a `library.json` at the root
of this repo, or a small separate repository. `symlink://` works with a local checkout.

CI: a GitHub Actions job that checks out WLED at a pinned commit and builds the usermod
for one WLED environment per supported chip (see [Supported chips](#supported-chips)).

## Modes and precedence

The usermod has three modes, chosen in its settings page:

| Mode | Base precedence | Gain | Behavior |
|---|---|---|---|
| Off | - | - | No JazzLights traffic at all, like WLED being off once its 10 seconds at precedence 0 are over. |
| Follower | 300 | 100 | Follows any wearable. Leads only when alone, or with orrery planets and creatures. |
| Leader | 20000 | 1000 | Leads every regular device. |

For reference, the current base + gain values are: creature 1+0, orrery planet 100+100,
orrery leader 222+0, wands, staffs, hats and shoes 500+100, vest and default 1000+1000,
gauntlet 4000+1000, xmas tree 5000+100, clouds 6000+100, sound-reactive dome 10000+0,
Core2 "override" 36000+5000, and admin 60000.

Reasons for the defaults:

- **Leader at 20000** is above everything that normally runs, and at or above 10000, which
  is the threshold (`kCreatureOverridePrecedence`) for creatures to follow a non-creature.
  It stays below 36000, so a Core2 in "override" mode, and orrery leaders that watch for
  that threshold, still take over.
- **Follower at 300** is below every wearable. It's above creatures (1) and orrery planets
  (100) so that a WLED follower doesn't end up mirroring a creature's pattern.
- **Gain** counts WLED changes made by a person as user input (see below), so if two WLED
  leaders are around, the one most recently changed leads. In follower mode, a local
  change gives a gain of 100, enough to lead creatures but no wearable.

Both base and gain are editable in the settings, for unusual setups.

### Which WLED changes count as user input

WLED tells usermods why its state changed through `onStateChange(callMode)`:

- `CALL_MODE_DIRECT_CHANGE` (web UI and API), `CALL_MODE_BUTTON` and
  `CALL_MODE_BUTTON_PRESET` are a person, so they count as user input and give the
  precedence gain.
- Playlists also report `CALL_MODE_DIRECT_CHANGE` (`applyPresetFromPlaylist()`), so the
  usermod tells them apart with WLED's `currentPlaylist` global, and gives no gain while a
  playlist runs.
- WLED's own sync (`CALL_MODE_NOTIFICATION`), Hue, Alexa and nightlight progress update
  the pattern without giving the gain.
- Changes the usermod makes itself while following are ignored, so they don't loop back.

### Engine change needed: holding a pattern without stopping following

The existing way to pin a pattern while leading, `Delegate::ForcedLeadingPattern()`, sets
the engine's `loop_` flag. And while `loop_` is set, the engine ignores every originator
below admin precedence (`kAdminPrecedence`), so a WLED leader would never yield, even to a
Core2 in override mode, and a WLED follower that once led alone would never follow again.

So the engine needs a small, local change (the wire format doesn't change): a way to set
the pattern this device advertises while leading, held until it changes, without the "stick
as leader" behavior of `loop_`. For example, `ProtocolEngine::SetLeadingPattern(
std::optional<PatternBits>)`. While leading, the engine advertises
`currentPattern = nextPattern = held pattern` and restarts it every 10 seconds, like a
loop. As soon as a better originator shows up, the engine follows it as usual.

Rotation patterns also restart every 10 seconds when looping. Existing effects are tuned so
that this restart is barely visible, for instance the rings period divides 10 seconds.

### When WLED is turned off

When WLED is off (`bri == 0`), the device removes itself from the protocol, in either
mode, in two steps:

1. For 10 seconds, it keeps listening and advertising as usual, but with precedence 0.
   Every other device has a higher precedence, so the devices that were following it elect
   another leader as soon as they hear this, without waiting for its entry to time out.
   10 seconds is longer than `kOriginationTimeDiscard` (9 seconds), so devices that miss
   these messages time out its entry before it goes silent anyway.
2. Then it stops listening and sending: it pauses Bluetooth (`SetPaused()`), stops
   handling UDP, and stops advertising.

The protocol has no explicit goodbye, so this needs no protocol change. To verify with an
engine test: a precedence decrease from a known originator takes effect immediately.

When WLED is turned back on, the device rejoins with its normal precedence, including
during the 10 seconds at precedence 0. In leader mode, this counts as user input.

## WLED to JazzLights (leader)

When the WLED device leads, the usermod converts the state of the followed segment into
pattern bits. `WledToJazzLights()` is a pure function of:

- the effect ID (`seg.mode`),
- the effective palette: the 16 colors WLED actually renders with, after resolving
  "Default" to the effect's default palette and building color-based palettes
  (`* Color 1`, `* Colors 1&2`...) from the segment colors,
- the segment colors (`seg.colors[0..2]`), with any white channel folded into RGB.

It ignores speed, intensity, custom sliders and brightness, which the protocol can't carry.

A changed result is applied with `SetLeadingPattern()`, debounced by about 250 ms so that
dragging the color wheel doesn't restart other devices' effects dozens of times per second.

### Palettes

Exact equivalents are hard-coded:

| WLED palette | JazzLights palette |
|---|---|
| Party (6) | party |
| Cloud (7) | cloud |
| Lava (8) | lava |
| Ocean (9) | ocean |
| Forest (10) | forest |
| Rainbow (11), Rainbow Bands (12) | rainbow |
| Fire (35) | heat |

Every other palette, including custom palettes, random palettes and palettes built from the
segment colors, goes to the nearest of the seven JazzLights palettes. The distance compares
the colors that each palette contains, regardless of their order, since a rotated rainbow is
still a rainbow. For example, a sum over the 16 entries of the nearest-color distance in a
roughly perceptual space such as CIELAB or OKLab. During implementation, a native test will
print the resulting mapping for all built-in WLED palettes so it can be reviewed and
corrected with overrides.

The colors an effect actually shows don't always come from its palette. When the palette
is "Default" (0), most WLED effects draw with the segment colors instead
(`Segment::color_from_palette()` returns color 1, 2 or 3 in that case), unless the effect
declares its own default palette (`pal=` in its metadata). And some effects, such as
Solid, have no palette setting at all. So the closest palette is computed from:

- the selected palette, when it isn't Default;
- the effect's default palette, when the palette is Default and the effect declares one;
- otherwise, a palette built from the segment colors, like `* Colors Only`.

This only relies on the effect's metadata string (`strip.getModeData()`), so it also works
for effects the table doesn't know about.

### Effects

WLED has about 220 effects. The mapping lives in a table in the code, not in this doc, and
follows three rules:

1. **Same effect.** WLED effects that JazzLights also has map to that effect, with the
   palette mapped as above. These are Hiphotic, Metaballs, Colored Bursts, Matrix, Glitter
   and Fire 2012 (flame).
2. **Same look.** WLED effects that clearly look like a JazzLights effect map to it, with
   the palette mapped. For example:
   - Solid goes to coloring with color 1, which is exact.
   - Breathe, Fade and Heartbeat go to glow in the nearest of its 8 colors, or to coloring
     if color 1 is far from all of them.
   - Plasmas and noise fields go to spin plasma.
   - Fires go to flame, with heat when the palette is Default.
   - A palette scrolling across the device, as in Palette, Colorwaves or Pacifica, goes to
     rings.
   - Sparkles and twinkles go to glitter.
   - Sine waves go to threesine.
3. **Not sure.** Every other effect goes to **rings with the closest palette**, using the
   palette mapping above. This includes chases, strobes, wipes, audio-reactive effects, and
   effects the table doesn't know about at all, such as effects added by a newer WLED or by
   other usermods.

Each entry is a judgment call, so the native test prints the full table for review during
implementation.

Hiphotic, Metaballs and Colored Bursts are 2D effects in WLED. On a 1D strip, WLED falls
back to Solid, so the mapping does the same and sends coloring.

### Segments

The usermod follows WLED's main segment by default (`strip.getMainSegment()`, the first
selected segment), with a setting to pick a segment ID instead. If the followed segment is
turned off while others are on, the device is treated as off.

## JazzLights to WLED (follower)

When the WLED device follows another device, it should show something close to that
device's pattern. There are two ways to do this, which aren't exclusive.

### Option A: translate to a WLED effect

`JazzLightsToWled()` maps the pattern bits back to a WLED effect, palette and colors, and
applies them to the followed segment:

| JazzLights | WLED |
|---|---|
| rings + palette | Palette (65) on 1D, or Rainbow (9) for the rainbow palette; on 2D, Palette with its rotation |
| hiphotic + palette | Hiphotic on 2D; Noise Pal or Colorwaves on 1D |
| spin plasma + palette | Rotozoomer on 2D; Plasma on 1D |
| flame + palette | Fire 2012, Fire palette for heat |
| metaballs / colored-bursts + palette | Metaballs / Colored Bursts on 2D; Plasma / Juggle on 1D |
| glitter, the-matrix, threesine | Twinklefox + Rainbow, Matrix (2D) or Rain (1D), Sine |
| solid, coloring | Solid with that color |
| glow | Breathe with that color |
| sync-test, calibration, follow-strand, mapping, unknown | keep the previous WLED state |

Palettes go back through the table above, with heat going to Fire.

Details:

- Before following, the usermod saves the segment's mode, palette, colors and sliders, and
  restores them when it stops following, like WLED does around realtime mode.
- It applies changes without notifying WLED's own UDP sync (`CALL_MODE_NO_NOTIFY`), and
  without saving anything to flash. A setting could instead let them propagate to other
  WLED devices.
- It sets `strip.timebase` so that WLED's effect clock `strip.now` counts from the
  followed pattern's start time. Most WLED effects are a function of `strip.now`, so WLED
  followers would then be in sync with each other, though not with JazzLights devices.

Cost: small. It's the leader's mapping table run backwards, plus state save and restore.
Result: approximate, as close as the leader direction.

### Option B: render JazzLights effects inside WLED

The usermod registers its own WLED effect, "JazzLights", with `strip.addEffect()`. While
following, it switches the segment to that effect. The effect's function runs JazzLights'
own `Effect` classes on the segment's pixels, with the pattern and start time from the
engine, exactly like `Player::Render()` does on a vest.

Since JazzLights effects are deterministic given the pattern bits, the start time and
`PredictableRandom`, the WLED device would show **the same pattern as the vest, in sync**,
not an approximation. The usermod could also register JazzLights' palettes (Heat
especially) as usermod palettes, which WLED now supports.

Cost: significant.

- JazzLights effects depend on FastLED's types and math, and WLED can't link FastLED since
  its own `CRGB` and palette symbols would clash. Non-ESP32 builds of JazzLights already
  replace FastLED with a small reimplementation in
  [fastled_wrapper.h](../../src/jazzlights/render/fastled_wrapper.h), which could be reused
  if it moved into the `jazzlights` namespace, or which could be aliased to WLED's
  `fastled_slim`.
- The `Layout` would be built from the segment's geometry: `x = index` on a 1D segment,
  `(x, y)` on a 2D matrix.
- More flash: the effects, `PredictableRandom`, `XYIndexStore`.

### Decision

Option A first, as it reuses the leader's tables and is enough for "close". Option B is a
later, optional phase, if pixel-level sync with vests turns out to matter.

## Platform concerns

### Bluetooth

This is the biggest technical risk.

- WLED's prebuilt Arduino libraries may not enable a Bluetooth stack, or may enable
  Bluedroid, which this repo already struggles with on newer Arduino cores (see the comment
  about pioarduino 55 and `esp_gap_ble_api.h` in `platformio.ini`). The plan is to use
  NimBLE from the NimBLE-Arduino library, like the `[_nimble_arduino]` environments, which
  only needs the Bluetooth controller from the framework. First implementation step: check
  that the controller is available in WLED's default platform, and that the Arduino core
  doesn't release its memory at boot.
- WLED disables Wi-Fi modem sleep on ESP32 by default (`noWifiSleep = true`). ESP-IDF
  requires modem sleep when Wi-Fi and Bluetooth run together, so the usermod has to force
  it on while Bluetooth is enabled, and explain that in its settings page.
- Flash and RAM: NimBLE adds roughly 100-200 KB of flash and tens of KB of RAM, and 4 MB
  WLED builds are tight. See [Supported chips](#supported-chips).
- LED output: the ESPHome README notes that RMT flickers on the classic ESP32 while Wi-Fi
  and Bluetooth are both active. WLED has its own RMT driver with Bluetooth-aware
  interrupts, plus I2S and parallel outputs, so this might be fine, but it needs testing.

### Supported chips

For now, the usermod supports the chips that both WLED and JazzLights already support:

| Chip | Transports |
|---|---|
| ESP32 | IP and Bluetooth |
| ESP32-S3 | IP and Bluetooth |
| ESP32-C3 | IP and Bluetooth |
| ESP32-C6 | IP and Bluetooth |

The other chips WLED supports (ESP32-S2, ESP32-C5, ESP32-P4 and ESP8266) are out of scope
until JazzLights supports them.

Flash is the likely limit, on 4 MB boards in particular. When a build doesn't fit, the
first fallback is a build flag that leaves Bluetooth out and keeps IP sync. We'd only drop
a chip if even that doesn't fit.

### IP

JazzLights devices sync over UDP multicast on 224.0.0.169 port 6699, on their own Wi-Fi
network. For IP sync, the WLED device has to join that same network, through WLED's normal
Wi-Fi settings. When WLED runs as an access point, or is on another network, only Bluetooth
works. Port 6699 doesn't clash with any WLED port.

### Firmware updates

The usermod uses WLED's own OTA updates. The JazzLights secure channel isn't included.

A usermod is compiled into the WLED firmware, so any update flashes a whole new image. If
that image wasn't built with the usermod, JazzLights is gone. Its settings stay in
`cfg.json`, but nothing reads them. WLED's update page has two paths:

1. **Manual upload of a `.bin`.** Before flashing, WLED checks that the new image's release
   name (`WLED_RELEASE_NAME`, stored in the image's metadata) matches the running one
   (`shouldAllowOTA()`). If it doesn't, the update is rejected, unless the user ticks
   "Ignore firmware validation".
2. **One-click update.** The page fetches the latest GitHub release of the repo baked into
   the build (`WLED_REPO`), downloads the asset named `*_<version>_<release name>.bin`, and
   uploads it. On WLED's current branch, `WLED_REPO` falls back to `wled/wled`, since
   WLED's metadata script doesn't set it on that branch yet.

A build that extends an official WLED environment inherits its release name, for example
`ESP32-S3_8MB_opi`. One click would then download the official image, which passes the
release name check, and the usermod would be gone without warning.

So **builds with the usermod get their own release name**, the official one with
`_JazzLights` appended, such as `ESP32-S3_8MB_opi_JazzLights`. Then:

- **One-click update:** finds no asset with that suffix, and stops with "Firmware not
  found".
- **Manual upload of a stock image:** rejected with "Firmware release name mismatch".
- **Images built with the usermod:** update as usual. The only way to lose the usermod is
  to tick "Ignore firmware validation".
- **First install on a device running stock WLED:** goes over USB, or over the air by
  ticking "Ignore firmware validation" once.

Setting the name: ideally the usermod's `library.json` has an `extraScript` that appends
`_JazzLights` to the project's `WLED_RELEASE_NAME`, so nobody can forget it. To verify
during implementation: that a library script can change a define used by WLED's own
`wled_metadata.cpp`. If it can't, the build instructions set the name, replacing the
inherited flag with `build_unflags` to avoid a redefinition.

**Optional, later: keep one-click updates.** Point `WLED_REPO` at a repo where we publish
WLED + usermod images, with asset names ending in `_<version>_<release name>.bin`. This
repo isn't a good fit, because `<version>` comes from that repo's own release tags. A small
separate repo would work.

**During an update**, WLED calls `onUpdateBegin(true)`, and `onUpdateBegin(false)` if the
update fails. The usermod pauses Bluetooth and UDP on the first call, to free memory and
radio time for the download, and resumes on the second.

## Configuration and UI

Settings, saved in WLED's `cfg.json` through `addToConfig()` / `readFromConfig()`:

- mode: off, follower or leader;
- base precedence and gain, prefilled from the mode;
- segment to follow: main or a segment ID;
- Bluetooth: on or off;
- in follower mode: whether to propagate to WLED sync.

The Info tab (`addToJsonInfo()`) shows: leading or following, the leader's device ID and
hop count, the current JazzLights pattern name (for example `rings-ocean`), and which
transports are up. `addToJsonState()` and `readFromJsonState()` expose the mode, so it can
be switched from the JSON API and Home Assistant.

## Testing

- **Native tests** for `WledToJazzLights()` and `JazzLightsToWled()`: round trips, every
  WLED effect ID maps to something, the palette distance table for every built-in palette.
  The mapping code shouldn't depend on WLED headers, so it builds in `extras/` without WLED.
- **Engine tests** for `SetLeadingPattern()`: a leader yields to higher precedence, a
  follower that led alone follows again, the held pattern restarts every 10 seconds.
- **Engine test** for turning off: once the device switches to precedence 0, the devices
  that followed it elect someone else right away.
- **Updates**: on hardware, a one-click update and a manual upload of a stock image are
  both refused, an image built with the usermod updates, and Bluetooth and UDP resume if
  an update fails.
- **Hardware**: an ATOM S3 Lite (ESP32-S3) and an ATOM Matrix (classic ESP32, 4 MB)
  running WLED with the usermod, next to a vest, in each mode, over Wi-Fi and over
  Bluetooth only. Also check LED flicker with Wi-Fi and Bluetooth both on, and turning
  WLED off and on.

## Implementation phases

1. Build only: the usermod compiles in WLED with the protocol and network code, Bluetooth
   checks done, the release name set, CI job.
2. Engine change (`SetLeadingPattern()`) with its tests.
3. Leader mode, with the WLED to JazzLights tables and their tests.
4. Follower mode with option A.
5. Optionally, option B.

## Possible protocol changes

**Everything in this section is optional, and separate from the plan above.**

### Cheaper than it looks

Today's parsers ignore what they don't understand:

- Over UDP, `ParseProtocolMessage()` ignores trailing bytes after the fixed fields, and the
  receive buffer is 1500 bytes.
- Over BLE, it ignores unknown extension byte flags and trailing bytes. The advertisement
  has room for 29 bytes, and non-creature devices use at most 23 of them.

So **new optional fields can be appended without breaking deployed devices**. Only devices
that want to use them need an update. The remaining cost is that a deployed device that
relays a message over several hops rebuilds it from `ProtocolMessage` and drops the
fields it doesn't know, so the extra information only crosses updated devices.

Also, palette value 7 currently duplicates rainbow. It's the one free palette code.

### Design principle: keep the base pattern meaningful

New information should refine the existing pattern bits, not replace them. The leader
still sends the closest existing pattern, as in the plan above, so deployed devices behave
exactly as they would without the change, and updated devices use the refinement. New
reserved patterns, on the other hand, would show solid red on deployed devices, since
that's what `PatternFromBits()` returns for unknown reserved patterns.

### Candidate fields

In rough order of value per byte:

1. **Two colors (6 bytes).** WLED's colors 1 and 2. Followers could build a palette from
   them, which covers color-based palettes and lets glow use any color. BLE has just
   enough room.
2. **WLED effect and palette IDs (2 bytes).** Lets WLED followers reproduce a WLED leader
   exactly, including through vests relaying over Bluetooth. It also lets JazzLights devices
   implement some WLED effects over time, falling back to the base pattern for the rest.
3. **Speed and intensity (2 bytes).** Only useful together with 2.
4. **Custom palette (up to 48 bytes, UDP only).** 16 RGB entries. Doesn't fit in BLE.

Fields 1 and 2 together take 8 bytes, which exceeds the 6 bytes left in BLE. We'd have to
pick, or add a second advertisement, which is a bigger change.

### Recommendation

Make no protocol change for now. Ship the plan above, and see which gaps people actually
notice. If colors turn out to matter most, field 1 is the smallest change with the
biggest effect.

## Decisions so far

- Follower rendering: option A first, option B later and optional.
- The usermod lives in `extras/wled/` in this repo.
- Leader precedence is 20000 + 1000: creatures follow it, and a Core2 in override mode
  still takes over.
- Follower precedence is 300 + 100.
- When WLED is turned off, the device advertises precedence 0 for 10 seconds, then stops
  listening and sending.
- WLED effects without a confident match, including unknown ones, go to rings with the
  closest palette.
- Support the chips that both WLED and JazzLights already support: ESP32, ESP32-S3,
  ESP32-C3 and ESP32-C6. Drop one only once we know it can't fit.
- The doc gives the mapping principles. The full effect table lives in the code.
- Builds with the usermod append `_JazzLights` to their release name, so WLED's updates
  don't replace them with stock WLED.

There are no open questions right now.
