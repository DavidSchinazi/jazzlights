# JazzLights Usermod for WLED

Status: approved, being implemented. Phases 1 to 4 (building the usermod in WLED, engine
changes, leader mode and follower mode) are done, see
[Implementation phases](#implementation-phases). On an ATOM Matrix with a vest, sync works
in both modes.

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
- Conveying several WLED segments when leading. The leader maps one segment (see
  [Segments](#segments)).
- Replacing WLED's own sync (UDP notifier, DDP, E1.31, ESP-NOW). Those keep working
  alongside JazzLights, and are still the best way to sync WLED devices with each other.
- Rendering JazzLights' own effects inside WLED. A follower shows the closest WLED effect
  instead, see [JazzLights to WLED](#jazzlights-to-wled-follower).

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
 | segments, effects, palettes   ---+--->| WLEDToJazzLights() mapping          |
 | LED drivers                   <--+----| JazzLightsToWLED() mapping          |
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
- `Esp32NimBleNetwork` for Bluetooth, using the NimBLE stack built into WLED's ESP-IDF
  libraries (see [Bluetooth](#bluetooth)).

None of `src/jazzlights/protocol`, `network` or `util` include FastLED, which matters
because WLED has its own `CRGB` and its own copies of the FastLED palettes (in
`fastled_slim`), which would clash with FastLED's.

JazzLights logs (`jll_info()` and friends) go to WLED's debug output (`DEBUG_PRINTF`, which
WLED can also send over the network with `WLED_DEBUG_HOST`), through a log handler installed
with `SetLogHandler()`, like the ESPHome component sends them to ESPHome's logger.
`JL_LOG_HANDLER` enables that handler for both. With `WLED_DEBUG` on, this includes the
protocol logs for every message sent and received, which is what checking wire compatibility
on hardware needs. Without it, only fatal logs are printed.

The radio timing doesn't depend on WLED's `loop()`, which can take tens of milliseconds:
`Esp32NimBleNetwork` alternates between scanning and advertising from its own task and
timer, and `Esp32SocketManager` has its own task too.

The usermod's `loop()` does what `Player::Render()` does for the protocol: drain received
messages into the engine, call `CheckLeaderAndPattern()`, and hand the message to send to
the `NetworkManager`. The mapping code runs only when WLED's state or the followed pattern
changes, not every loop.

### Build and packaging

WLED's main branch builds on ESP-IDF 5.5 with Arduino core 3.3 (Tasmota's platform), and
usermods are PlatformIO libraries listed in `custom_usermods`. That setting accepts external
sources such as `symlink://` paths, as long as each one is on its own line, so the usermod
lives in this repo, under `extras/wled/`:

- `library.json` declares the usermod as a PlatformIO library. It compiles
  `jazzlights_usermod.cpp` plus the needed subset of `src/jazzlights` (the protocol, the
  network code and `util`, but not the renderer, the effects or FastLED), and sets
  `JL_WLED=1` (a new flag) and `JL_ESP32_NIMBLE=1`. `JL_WLED` turns on the log handler
  and turns off the secure channel, as `JL_ESPHOME` does. `JL_CONFIG` and `JL_CONTROLLER`
  still describe the device, such as a vest on an ATOM Matrix, and come from the build
  environment.
- `src` is a symlink to `../../src`. Listing the sources through a `..` path instead makes
  PlatformIO compile them into WLED's own `src` build folder, which breaks WLED's
  dependency resolution.
- `jazzlights_wled.py` is the library's build script. It sets the release name, see
  [Firmware updates](#firmware-updates).
- `jazzlights_usermod.cpp` is the usermod.
- `platformio_override.ini` has ready-made WLED environments with the usermod, for the
  boards that JazzLights devices use, one per supported chip. Each extends an official WLED
  environment, sets `JL_CONFIG=VEST` and the board's `JL_CONTROLLER`, and uses the vest's
  LED pins from [layout_data.h](../../src/jazzlights/layout/layout_data.h):

  | Environment | Extends | Board settings |
  |---|---|---|
  | `jazzlights_atom_matrix` | `esp32dev` | 25 LEDs on GPIO 27 (the ATOM's 5x5 matrix), then 360 on 26 (the vest), button on 39 |
  | `jazzlights_atom_s3_lite` | `esp32s3dev_8MB_none` | LEDs on GPIO 2, button on 41 |
  | `jazzlights_m5stamp_c3u` | `esp32c3dev` | LEDs on GPIO 1 |
  | `jazzlights_m5_nano_c6` | `esp32c6dev_4MB` | LEDs on GPIO 2, no audioreactive, no Bluetooth |

  They keep WLED's default usermods (currently audioreactive), except on the NanoC6, which
  has no microphone. WLED's LED settings can still be changed from its web UI.

  On the ATOM Matrix, the vest's 360 LEDs don't form a grid that WLED's 2D panel settings
  can describe, so `generate_vest_ledmap.py` writes a WLED `ledmap.json` instead. It reads
  the vest's coordinates from `layout_data_vest.cpp`, rounds them to a 20 x 19 grid, where
  each LED gets its own cell, and places the ATOM's 5x5 screen to the right, in the top 5
  rows of a 25 x 19 canvas. A ledmap with a width and height also makes WLED 2D at boot,
  so WLED's 2D panel settings aren't needed. Setup, once:
  1. Check the LED order in WLED's LED Preferences. The map expects the preset's order:
     the screen first (GPIO 27, 25 LEDs, starting at 0), then the vest (GPIO 26, 360 LEDs,
     starting at 25). WLED keeps its LED settings across firmware updates, so a device
     first set up with an earlier preset can have the vest first; pass `--vest-first` to
     the script for those. With the wrong order, every LED is off by 25: the vest looks
     scrambled, and the screen shows part of the vest.
  2. `extras/wled/generate_vest_ledmap.py > ledmap.json`, then upload it in WLED's file
     editor (`http://<device>/edit`) and reboot.
  3. In WLED's segments, make segment 0 the vest, columns 0 to 20 and rows 0 to 19, and
     add segment 1 for the screen, columns 20 to 25 and rows 0 to 5 (stop values are
     exclusive). Switch the usermod's mode to off or leader while doing so, see
     [JazzLights to WLED](#jazzlights-to-wled-follower).

  The vest's layout matches [VEST.md](VEST.md): its strand starts at the top, so row 0 is
  the top of the vest, and WLED's effects that rise, like Fire 2012, rise from its hem.
  The columns go around the wearer as seen from outside: the front left panel from the
  zipper, the back, then the front right panel back to the zipper. The screen is the
  right way up when JazzLights' text reads correctly on it. If the vest still shows up
  mirrored or upside down, regenerate with `--flip-x` or `--flip-y`.
- `build.sh` builds them. The first run clones WLED, at a commit pinned in the script, into
  `extras/.wled`, and every run copies `platformio_override.ini` there:

  ```sh
  extras/wled/build.sh                                       # All environments.
  extras/wled/build.sh -e jazzlights_atom_s3_lite -t upload -t monitor
  ```

  Run `-t erase` as its own command: WLED's platform turns off PlatformIO's library
  dependency resolution whenever `erase` is among the targets, so a build in the same
  command can't find `WiFi.h`.

  WLED's build bundles its web UI with Node.js, which needs to be on the `PATH`. WLED asks
  for Node 20. On macOS: `brew install node@20`, then add `/opt/homebrew/opt/node@20/bin`
  to the `PATH`.

Two details of `build.sh`:

- The WLED checkout can't be inside `extras/wled`, or PlatformIO would see WLED's own
  libraries as part of the usermod's library.
- It sets `PLATFORMIO_CORE_DIR` to `extras/.wled/.platformio`. WLED's platform installs a
  different Arduino core under the same package name as this repo's `platformio.ini`, so
  sharing `~/.platformio` would make each build replace the other's Arduino core.

To verify later: PlatformIO can't take a subdirectory of a git repository as a library, so
consuming the usermod straight from GitHub, without a local checkout of this repo, may need
a `library.json` at the root of this repo, or a small separate repository.

CI: the `wled` job in `.github/workflows/jazzlights.yml` runs `extras/wled/build.sh`, which
builds every environment above.

## Modes and precedence

The usermod has three modes, chosen in its settings page:

| Mode | Base precedence | Gain | Behavior |
|---|---|---|---|
| Off | - | - | No JazzLights traffic at all, like WLED being off once its 10 seconds at precedence 0 are over. |
| Follower | 300 | 100 | Follows any wearable. Leads only when alone, or with orrery planets and creatures, or for 10 minutes after a person changes WLED. |
| Leader | 20000 | 1000 | Leads every regular device. Follows a Core2 in override mode or an admin device when one is around. |

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
  leaders are around, the one most recently changed leads.

Both base and gain are editable in the settings, for unusual setups.

### What a person's change does in each mode

In the engine today, local user input does two things for 10 minutes (`kInputDuration`):
it adds the precedence gain, decaying over that time, and it makes the device ignore every
other device below admin precedence, so the device leads with what the person picked. The
usermod uses these differently per mode:

- **Follower:** both, as on a vest. A person's change pauses following for 10 minutes,
  during which WLED shows what the person picked and advertises it. Then the device follows
  again. Only an admin device can interrupt the pause.
- **Leader:** the gain only. The device doesn't ignore anyone, so a Core2 in override mode
  or an admin device always takes over, even right after a change.

This needs a second small engine change, next to `SetLeadingPattern()`:
`SetUserInputKeepsLeading()`, on by default so existing devices behave as today. See
[Engine changes](#engine-changes).

When a higher precedence device takes over a WLED leader, WLED's LEDs follow it too, with
the same code as follower mode (see [JazzLights to WLED](#jazzlights-to-wled-follower)),
and go back to WLED's own state when it leaves.

### Which WLED changes count as user input

WLED tells usermods why its state changed through `onStateChange(callMode)`:

- `CALL_MODE_DIRECT_CHANGE` (web UI and API), `CALL_MODE_BUTTON` and
  `CALL_MODE_BUTTON_PRESET` are a person, so they count as user input and give the
  precedence gain.
- Playlists also report `CALL_MODE_DIRECT_CHANGE` (`applyPresetFromPlaylist()`), so the
  usermod tells them apart with WLED's `currentPlaylist` global, and gives no gain while a
  playlist runs.
- A person's change only counts if it changes the segment's effect, palette or colors
  within a second, so that adjusting brightness or speed doesn't take over other devices.
- WLED's own sync (`CALL_MODE_NOTIFICATION`), Hue, Alexa and nightlight progress update
  the pattern without giving the gain.
- Changes the usermod makes itself while following are ignored, so they don't loop back.

### Engine changes

Two small changes, both local to the engine. The wire format doesn't change.

**Holding a pattern without sticking as leader.**

The existing way to pin a pattern while leading, `Delegate::ForcedLeadingPattern()`, sets
the engine's `loop_` flag. And while `loop_` is set, the engine ignores every originator
below admin precedence (`kAdminPrecedence`), so a WLED leader would never yield, even to a
Core2 in override mode, and a WLED follower that once led alone would never follow again.

So `ProtocolEngine::SetLeadingPattern(std::optional<PatternBits>)` sets the pattern this
device advertises whenever it leads, held until the next call, without the "stick as leader"
behavior of `loop_`:

- While leading, the engine advertises `currentPattern = nextPattern = held pattern` and
  restarts it every 10 seconds, like a loop.
- A new pattern takes effect at the next `CheckLeaderAndPattern()`, restarting right away.
  Setting the same one again changes nothing.
- As soon as a better originator shows up, the engine follows it as usual. When it leaves,
  the engine leads with the held pattern again, restarting it.
- `std::nullopt` resumes the regular rotation after the current pattern.

A held pattern restarts every 10 seconds, on every device. For several effects that
restart is visible. See [Transitions](#transitions).

**Gain without ignoring others.** Recent user input both adds the precedence gain and
makes the device ignore everyone below admin precedence. A WLED leader needs only the first
(see [What a person's change does in each mode](#what-a-persons-change-does-in-each-mode)),
so `SetUserInputKeepsLeading(false)` turns off the second. It's on by default. Also, user
input used to come only with a pattern change (`GoToNextPattern()`, `SetPattern()`), so
`RecordUserInput()` records it without changing the pattern, for a person's WLED change.

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

The protocol has no explicit goodbye, so this needs no protocol change. Reading the engine,
this works: an update from an originator's next hop replaces its precedence right away,
including a decrease. And a device that relayed the WLED device's messages and starts
following someone else makes the devices behind it retract the WLED device's entry
("abandonment" in `HandleReceivedMessage()`), so the change spreads over several hops.
`TestPrecedenceDropReleasesFollowers` confirms both.

When WLED is turned back on, the device rejoins with its normal precedence, including
during the 10 seconds at precedence 0. In leader mode, this counts as user input.

## WLED to JazzLights (leader)

When the WLED device leads, the usermod converts the state of the followed segment into
pattern bits. The conversion lives in `src/jazzlights/wled/mapping.cpp`, which doesn't
depend on WLED's headers, so it's tested natively. `WLEDToJazzLights()` is a pure function
of a `WLEDSegmentState`, which the usermod reads from the segment:

- the effect ID (`seg.mode`), and whether WLED shows Solid instead because it's a 2D-only
  effect on a 1D segment (from the effect's metadata string);
- whether the palette is Default;
- the palette whose colors the effect shows, see [Palettes](#palettes), and its 16 colors
  from `Segment::loadPalette()`;
- the first segment color, with any white channel folded into RGB.

It ignores speed, intensity, custom sliders and brightness, which the protocol can't carry.

The usermod only recomputes the mapping when the segment's effect, palette, colors or
dimensions change. A changed result is applied with `SetLeadingPattern()` once it has been
stable for 250 ms, so that dragging the color wheel doesn't restart other devices' effects
dozens of times per second. The usermod sets the leading pattern in both modes, since a
follower also leads when it's alone.

### Randomizing the unused bits

The mapping only chooses the bits that select the effect and palette. The other bits still
matter: each effect seeds its randomness from the pattern bits
(`PredictableRandom::ResetWithFrameStart()`), so they choose, for example, where rings are
centered or which scales hiphotic uses. Sending the same bits for every "rings-ocean"
would always show the same rings.

So, like the Core2 UI, the leader passes the result through `RandomizePatternBits()`,
which randomizes those bits and leaves the effect and palette bits alone. It does this once
per change of the mapped effect and palette, not on every 10 second restart, so a held
pattern keeps the same look. `RandomizePatternBits()` leaves most reserved patterns
unchanged, which is fine since they don't use these bits.

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
segment colors, goes to the closest of the seven JazzLights palettes by hue
(`ClosestPalette()`): each palette becomes a histogram of its OKLab hues in 12 bins of 30
degrees, each color weighted by its saturation, plus one bin for black, white and greys.
The closest palette is the one whose histogram differs the least. This ignores the order of
the colors, since a rotated rainbow is still a rainbow.

A first version compared the colors themselves in OKLab instead. Lightness dominated there,
so heat, which spans black to white, came out closest to many unrelated palettes, such as
Temperature and Fireworks.

`pio test -e native -f test_wled -v` prints the result for every built-in WLED palette.
Most look right. Some debatable ones, to correct with overrides if they matter: Magenta,
Light Pink and Pink Candy go to cloud, Traffic Light to forest, and Orange & Teal to heat.
WLED's palette colors are compared as stored, without the gamma correction WLED applies
when it shows them.

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

The table is `CategoryOfWLEDEffect()` in `src/jazzlights/wled/mapping.cpp`. Each entry is
a judgment call, so `pio test -e native -f test_wled -v` prints the result for every WLED
effect, by name, for review.

Some WLED effects only run on 2D segments, such as Hiphotic, Metaballs and Colored Bursts.
On a 1D segment WLED shows Solid instead, so the mapping does the same and sends coloring.

For glow, a color is close enough to one of the 8 glow colors when their OKLab distance,
after scaling both to full brightness, is at most 0.1. That keeps for example pure greens
and chartreuse as glow-green, but sends orange, amber, azure and violet as coloring.

### Segments

A WLED segment is a group of LEDs running one effect, with its own palette, colors and
sliders: a range of LEDs, or on a 2D setup, a rectangle of the canvas. All the LEDs of a
JazzLights device show the same pattern and palette, so:

- When following, the usermod shows the followed pattern on every active segment, each at
  its own size and with its own 1D or 2D equivalent.
- When leading, the usermod maps one segment, WLED's main segment by default
  (`strip.getMainSegment()`, the first selected segment), with a setting to pick a segment
  ID instead. To have all segments match while leading, select them all in WLED's UI
  before changing the effect, which WLED then applies to every selected segment.

If the mapped segment is turned off while others are on, the device is treated as off.

### Transitions

JazzLights patterns last 10 seconds, and every message carries the current pattern, its
start time and the next pattern. So all devices switch together at each 10 second
boundary, even those that missed recent messages. A WLED leader uses this as follows.

**While WLED stays on one effect,** the leader advertises
`currentPattern = nextPattern = held pattern`, and moves the start time forward by 10
seconds at each boundary. Followers know the next pattern is the same one, so they all
restart it at the same moment.

**When a person changes WLED** (web UI, button, API), the leader switches right away,
with a new start time. Followers switch as soon as they hear it: within milliseconds over
Wi-Fi, and around a second per hop over Bluetooth. This isn't seamless, since there was no
advance notice, but it's how button presses on JazzLights devices already work.

**Playlists** are where advance notice could make changes seamless. A playlist knows its
next preset and when it starts. A restart is a jump anyway, so at the last 10 second
boundary before a playlist change, the leader could restart the pattern partway through
(start time = change time - 10 seconds), and announce the upcoming preset's pattern as
`nextPattern`. Followers would then switch by themselves, exactly at the change. The
catch: WLED keeps the playlist's entries, position and timer as `static` variables in
`playlist.cpp`, out of reach of usermods. The options are:

- Switch immediately, as for a person's change. This is the plan.
- Infer the playlist from `presets.json` and the `currentPreset` global. This is fragile,
  and doesn't work with shuffle.
- Propose a small WLED change exposing the next entry and the time left. This goes against
  the goal of not changing WLED, so it's a later option at most.

**The cost of holding a pattern:** every 10 second restart calls the effect's `Begin()`
again, which resets its state, and resets its clock to 0.

| Effect | Restart |
|---|---|
| rings, glitter, threesine, glow | Nearly invisible: they repeat every 10 seconds. |
| flame | Visible: the heat map is cleared, so the flame goes dark and reignites. |
| the-matrix, colored-bursts | Visible: their trails are cleared. |
| hiphotic, spin plasma, metaballs | Visible: they're functions of time that don't repeat every 10 seconds, so they jump. |
| solid colors, coloring | Invisible. |

Devices looping one pattern have this problem today. A WLED leader would hold patterns for
minutes, so it would show more. Followers can't fix it alone: to keep running one effect
across restarts, they would need to agree on when the hold began, and a device that joins
during a hold can't know that. It takes a protocol change, the "hold cycles" field in
[Candidate fields](#candidate-fields).

A WLED follower sets `strip.timebase` only when the followed pattern changes,
not when the same pattern restarts, so WLED's own effects don't jump every 10 seconds.

## JazzLights to WLED (follower)

When the WLED device follows another device, in follower mode or when a higher precedence
device takes over a WLED leader, it should show something close to that device's pattern.
It does so by translating the pattern to the closest WLED effect.

`JazzLightsToWLED()`, in `src/jazzlights/wled/mapping.cpp`, maps the pattern bits back to
a WLED effect, palette and colors, and the usermod applies them to the followed segment:

| JazzLights | WLED on a 2D segment | WLED on a 1D segment |
|---|---|---|
| rings + palette | Octopus | Rainbow |
| spin plasma + palette | Rotozoomer | Plasma |
| hiphotic + palette | Hiphotic | Noise Pal |
| flame + palette | Fire 2012 | same |
| metaballs + palette | Metaballs | Plasma |
| colored-bursts + palette | Colored Bursts | Juggle |
| glitter | Twinklefox with the Rainbow palette | same |
| the-matrix | Matrix | Rain |
| threesine | Sine with the Rainbow palette | same |
| solid, coloring | Solid with that color | same |
| glow | Breathe with that color, fading to black | same |
| sync-test, calibration, follow-strand, mapping, unknown | keep the current WLED state | same |

Palettes go back through the exact table in [Palettes](#palettes), with heat going to Fire.
`TestJazzLightsToWLED` checks that, on a 2D segment, the leader direction maps every one of
these back to the same JazzLights effect and palette, and on a 1D segment, to the same
palette.

Details:

- It shows the followed pattern on every active segment, see [Segments](#segments).
- When it starts showing a followed pattern, the usermod saves each segment's effect,
  palette and colors, and restores them when it stops following, like WLED does around
  realtime mode. It leaves the sliders alone: `setMode()` keeps them. So the effects it
  picks move whatever the sliders and checkboxes are. For example, rings don't go to
  WLED's Palette effect, which stays still unless its "Animate Shift" checkbox is on.
- When a person changes a segment's effect, palette or colors, or adds or removes a
  segment, while it shows a followed pattern, their change becomes the state to go back
  to. In follower mode, that also
  pauses following, as described in
  [What a person's change does in each mode](#what-a-persons-change-does-in-each-mode), so
  their change shows right away. In leader mode, following continues, so their change shows
  until the followed pattern changes, and comes back when the override ends.
- Like JazzLights devices, it switches to the next pattern at the end of the current one,
  without waiting for the leader's next message.
- While following, the leader direction is paused, since the segments show what the
  usermod applied. It resumes from the restored state.
- It applies changes without notifying WLED's own UDP sync (`CALL_MODE_NO_NOTIFY`), and
  without saving anything to flash. A setting could instead let them propagate to other
  WLED devices.
- Each applied pattern, about every 10 seconds, makes WLED push its state to the web UI,
  which redraws its segment list, so a segment being added there disappears before it's
  confirmed. WLED does the same while another WLED device syncs it. To edit segments,
  switch the mode to off or leader first, then back. On the ATOM Matrix, that was easy
  enough that the usermod doesn't suppress the push, which would keep the web UI from
  showing the followed effect until a reload.
- It changes the segment directly (`setMode()`, `setPalette()`, `setColor()`) and then
  calls `stateUpdated(CALL_MODE_NO_NOTIFY)`, not `colorUpdated()`. `colorUpdated()` first
  calls `applyValuesToSelectedSegs()`, which copies WLED's global effect and palette back
  onto the selected segments, and would undo the change.
- It sets `strip.timebase` so that WLED's effect clock `strip.now` counts from the
  followed pattern's start time, but only when the shown pattern changes, not at each 10
  second restart (see [Transitions](#transitions)). Most WLED effects are a function of
  `strip.now`, so WLED followers would then be in sync with each other, though not with
  JazzLights devices.

Cost: small. It's the leader's mapping table run backwards, plus state save and restore.
Result: approximate, as close as the leader direction.

## Platform concerns

### Bluetooth

What phase 1 found:

- **Stack.** WLED's prebuilt ESP-IDF libraries have Bluetooth and NimBLE enabled
  (`CONFIG_BT_NIMBLE_ENABLED`), so the usermod uses ESP-IDF's NimBLE, like the
  `framework = espidf` builds of this repo. It doesn't need the NimBLE-Arduino library,
  which the friend's prototype (see [Prior work](#prior-work)) used, and which would clash
  with the built-in stack. The NimBLE configuration is the prebuilt one: for example the
  central role stays compiled in, though we don't use it.
- **Controller memory.** At boot, Arduino core 3.3 releases the Bluetooth controller's
  memory unless a Bluetooth library includes `esp32-hal-bt-mem.h`, whose constructor marks
  Bluetooth as in use. The `btStarted()` workaround in `esp32_nimble.cpp` no longer does
  that, so `esp32_nimble.cpp` now includes that header when it exists. Phase 1 checked in
  the built image that the flag gets set before `app_main()` checks it.
- **Wi-Fi modem sleep.** ESP-IDF requires modem sleep while Wi-Fi and Bluetooth run
  together, and WLED disables it by default (`noWifiSleep = true`). The usermod's `setup()`
  runs after WLED reads its settings and before it starts Wi-Fi, so it sets `noWifiSleep`
  to false there when Bluetooth is on. The "Disable WiFi sleep" setting then shows as off.
- **Order.** The usermod initializes Bluetooth in `setup()`, which WLED calls before it
  starts Wi-Fi.
- **WLED's access point.** On an ATOM Matrix, phones could see `WLED-AP` but not join it
  while Bluetooth ran: they associated, then dropped off without getting an address. With
  Bluetooth left out of the build, they joined. Wi-Fi and Bluetooth share the radio, and we
  scan for Bluetooth almost continuously, which apparently leaves the access point too
  little radio time for DHCP. So while WLED's access point is on (WLED's `apActive`), the
  usermod pauses Bluetooth half of the time (1 second out of every 2), so that phones can
  join, and entirely while a client is connected (`WiFi.softAPgetStationNum()`), so that
  WLED's web UI works. Sync over Bluetooth slows down meanwhile. Bluetooth takes about
  85 KB of internal memory: free memory went from about 128 KB to about 43 KB.
- **LED driver crash on the classic ESP32.** WLED's RMT LED driver uses a high-level
  interrupt. When Bluetooth's high-level interrupt is compiled in (`CONFIG_BTDM_CTRL_HLI`,
  which WLED's ESP32 libraries enable), the driver shares interrupt 25 with the Bluetooth
  controller through ESP-IDF's HLI dispatcher (`hli_c_handler()`). The dispatcher calls
  every registered handler whose status register shows a pending interrupt, and the
  controller registers core 0's status register. But WLED binds the shared interrupt on the
  core that creates the LED buses, core 1, while the controller is pinned to core 0. So an
  LED interrupt on core 1 can run the controller's handler there while core 0 runs it too.
  On an ATOM Matrix this crashed (double exception in `r_rwbtdm_isr_wrapper`) seconds
  after boot. WLED doesn't ship Bluetooth, so this path is untested there.

  The workaround, in the usermod: when Bluetooth runs on a classic ESP32, it switches
  WLED's digital LED buses from the RMT driver to WLED's parallel I2S driver
  (`BUSDRV_PARHW`) before WLED creates them, both at boot, in `readFromConfig()`, and when
  the LED settings are saved, in `loop()`. It overrides a driver chosen in WLED's LED
  settings. The ESP32-S3, C3 and C6 don't use this path.

  WLED only calls `readFromConfig()` when its configuration has a usermods section, which
  a fresh install doesn't. So if `setup()` finds an LED bus on the RMT driver and
  `readFromConfig()` didn't run, the usermod saves WLED's configuration, which adds the
  section, and reboots once. WLED counts that as a software reset, which doesn't count
  towards its bootloop detection. If a bus still uses the RMT driver after that, because
  the parallel driver wasn't available, the usermod doesn't start Bluetooth, and says so in
  the Info tab.

  With this, the ATOM Matrix no longer crashes. Not yet checked: that the I2S driver
  doesn't flicker with Wi-Fi and Bluetooth on.

### Supported chips

For now, the usermod supports the chips that both WLED and JazzLights already support. Flash
use of the app partition, for WLED with its default usermods, at the pinned WLED commit:

| Chip | Environment | Stock WLED | With the usermod | Transports |
|---|---|---|---|---|
| ESP32, 4 MB | `jazzlights_atom_matrix` | 84.7% | 96.8% (+190 KB) | IP and Bluetooth |
| ESP32-S3, 8 MB | `jazzlights_atom_s3_lite` | 60.5% | 69.0% (+178 KB) | IP and Bluetooth |
| ESP32-C3, 4 MB | `jazzlights_m5stamp_c3u` | 83.8% | 96.5% (+201 KB) | IP and Bluetooth |
| ESP32-C6, 4 MB | `jazzlights_m5_nano_c6` | 87.2% | 86.9% | IP only |

Most of the cost is NimBLE: the protocol and network code alone take about 30 KB. The 4 MB
ESP32 and ESP32-C3 builds fit, with about 50 KB to spare.

On the ESP32-C6, NimBLE takes about 320 KB. Even without audioreactive, which the NanoC6
doesn't need, the 4 MB build with Bluetooth needs 107.7% of its app partition, and also
turning off WLED's infrared, MQTT, Alexa and Hue support only brings it to 104.8%. So, as
planned, its environment leaves Bluetooth out with `-D JL_DISABLE_BLUETOOTH=1` and keeps
IP sync. A larger app partition would fit Bluetooth, at the cost of WLED's file system,
which holds its settings and presets.

The other chips WLED supports (ESP32-S2, ESP32-C5, ESP32-P4 and ESP8266) are out of scope
until JazzLights supports them.

### IP

JazzLights devices sync over UDP multicast on 224.0.0.169 port 6699, on their own Wi-Fi
network. For IP sync, the WLED device has to join that same network, through WLED's normal
Wi-Fi settings. When WLED runs as an access point, or is on another network, only Bluetooth
works. Port 6699 doesn't clash with any WLED port.

When WLED connects or reconnects, it stops Wi-Fi with `WiFi.mode(WIFI_MODE_NULL)`, and the
Arduino core then destroys the network interfaces and creates new ones
(`wifiLowLevelDeinit()`). `Esp32SocketManager` keeps a pointer to each interface, so the
usermod only registers an interface while it has an address, and removes it with the new
`Esp32SocketManager::RemoveInterface()` when it loses it or WLED replaces it.

The device ID is the Bluetooth MAC address, or without Bluetooth, the Wi-Fi MAC address.

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

Setting the name: `jazzlights_wled.py`, the usermod library's build script, appends
`_JazzLights` to `WLED_RELEASE_NAME`, so nobody can forget it. PlatformIO creates the
project's build environment before running library scripts, and compiles WLED's own sources
with it afterwards, so the script changes the name there, for `wled_metadata.cpp`. It also
changes it in the global environment, where WLED's `output_bins.py` reads it to name the
image, for example `WLED_17.0.0-devV5_ESP32_JazzLights.bin`. Phase 1 checked both.

**Optional, later: keep one-click updates.** Point `WLED_REPO` at a repo where we publish
WLED + usermod images, with asset names ending in `_<version>_<release name>.bin`. This
repo isn't a good fit, because `<version>` comes from that repo's own release tags. A small
separate repo would work.

**During an update**, WLED calls `onUpdateBegin(true)`, and `onUpdateBegin(false)` if the
update fails. The usermod pauses Bluetooth and UDP on the first call, to free memory and
radio time for the download, and resumes on the second.

## Configuration and UI

Settings, saved in WLED's `cfg.json` through `addToConfig()` / `readFromConfig()`:

- mode: off, follower (the default) or leader. Switching to off at runtime acts like WLED
  being turned off. If the device boots with the mode off, turning it on needs a reboot,
  since Bluetooth has to start before Wi-Fi.
- base precedence and gain, prefilled from the mode: changing the mode without editing
  them resets them to the new mode's defaults;
- segment to follow: 255 for the main segment, or a segment ID;
- Bluetooth: on or off (needs a reboot, absent from builds without Bluetooth);
- later, in follower mode: whether to propagate to WLED sync.

The Info tab (`addToJsonInfo()`) shows the mode, whether the device leads or follows, the
leader's device ID, hop count and transport, and the current JazzLights pattern name (for
example `rings-ocean`). Later: `addToJsonState()` and `readFromJsonState()` to expose the
mode in WLED's state, for the JSON API and Home Assistant. Until then, the mode is in WLED's
configuration (`/json/cfg`).

## Testing

- **Native tests** in `test/test_wled` (done in phase 3):
  - `TestDecodeMatchesRenderer`: `DecodePattern()` names about 13,000 patterns (the
    rotation, every reserved pattern, arbitrary bits) exactly like the JazzLights renderer.
  - `TestPalettePatterns`, `TestPalettelessPatterns`: every pattern the leader can send
    decodes back to its effect and palette, also after `RandomizePatternBits()`.
  - `TestWLEDEffectIDs`: the effect and palette IDs in `mapping.h` have the expected WLED
    names. The usermod also checks the effect IDs against WLED's `FX.h` with
    `static_assert`.
  - `TestEveryWLEDEffectMaps`, `TestPaletteMapping`, `TestGlowColors`: every WLED effect and
    built-in palette maps to something, with the tables printed for review.
  - `TestSpecialCases`: Solid, the 2D-only fallback, and fires with the Default palette.
  - `TestJazzLightsToWLED` (phase 4): the follower direction, with its table printed for
    review.

  WLED's effect names and built-in palettes come from `test/test_wled/wled_data.h`, which
  `extras/wled/generate_test_data.py` generates from the pinned WLED checkout. It expands
  WLED's gradient palettes to 16 colors the way WLED does.
- **Engine tests**, in `test/test_protocol` (done in phase 2):
  - `TestLeadingPatternIsHeld`: the held pattern replaces the rotation, restarts every 10
    seconds, restarts right away when it changes, and the rotation resumes when cleared.
  - `TestLeadingPatternStillFollows`: a device holding a pattern follows a Core2 in
    override mode, and leads with its pattern again once it's gone.
  - `TestUserInputCanOnlyAddGain`: with `SetUserInputKeepsLeading(false)`, user input adds
    the gain but the device still follows better precedence.
  - `TestUserInputKeepsLeadingByDefault`: by default, user input pauses following for
    `kInputDuration`.
  - `TestPrecedenceDropReleasesFollowers`: when a leader drops to precedence 0, a device
    following it directly, and one following it through that device, elect someone else
    right away.
- **Updates**: on hardware, a one-click update and a manual upload of a stock image are
  both refused, an image built with the usermod updates, and Bluetooth and UDP resume if
  an update fails.
- **Hardware**: on an ATOM Matrix with a vest's LEDs, the generated ledmap and the
  screen's segment, sync works in both modes: as a leader, JazzLights devices show the
  closest pattern, and as a follower, WLED shows the followed pattern on the vest and the
  screen. Still to check: an ATOM S3 Lite
  (ESP32-S3), Bluetooth only, LED flicker, turning WLED off and on, firmware updates, and
  joining WLED's access point with Bluetooth on. The full plan: an ATOM S3 Lite (ESP32-S3) and an ATOM Matrix (classic ESP32, 4 MB)
  running WLED with the usermod, next to a vest, in each mode, over Wi-Fi and over
  Bluetooth only. Also check LED flicker with Wi-Fi and Bluetooth both on, and turning
  WLED off and on.

## Implementation phases

1. Build only: the usermod compiles in WLED with the protocol and network code, Bluetooth
   checks done, the release name set, CI job. **Done.** The usermod already runs the
   protocol over Wi-Fi, Ethernet and Bluetooth with the follower precedence, and the Info
   tab shows whether it leads or follows. It doesn't change WLED's LEDs yet, and when it
   leads it advertises the engine's regular rotation. WLED's reconnects turned out to
   replace the network interfaces, see [IP](#ip). On an ATOM Matrix, calling
   `esp_netif_init()` again after the Arduino core did caused no trouble. Not yet checked
   on hardware:
   - that forwarding logs from other tasks is safe when WLED sends its debug output over
     the network.
2. Engine changes (`SetLeadingPattern()`, `RecordUserInput()` and
   `SetUserInputKeepsLeading()`) with their tests. **Done.** The usermod doesn't use them
   yet.
3. Leader mode, with the WLED to JazzLights tables and their tests. **Done.** The usermod
   has the mode, precedence and segment settings, maps the followed segment to a leading
   pattern in both modes, records a person's changes as user input, leaves the protocol
   when WLED turns off, and pauses Bluetooth and UDP during firmware updates. The mapping
   is in a new `wled` module under `src/jazzlights`, which only depends on `util`. Not yet
   checked on hardware, along with phase 1's list.
4. Follower mode. **Done.** First hardware test, on an ATOM Matrix: NimBLE
   starts, and the usermod follows a device over Bluetooth and applies its pattern. It then
   crashed because of the LED driver issue in [Bluetooth](#bluetooth), now worked around.
   Since then, on the same ATOM Matrix with a vest, sync works in both modes, see
   [Testing](#testing). Not done: the setting
   to let followed patterns propagate to WLED's own sync.

## Prior work

A friend wrote a prototype usermod,
[claudiosv/wled-jazzlights-ble-sync#1](https://github.com/claudiosv/wled-jazzlights-ble-sync/pull/1),
which reimplements the Bluetooth protocol and the election from scratch with
NimBLE-Arduino, and maps the leader's pattern to a WLED effect and palette when following.
It was tested following a real JazzLights device on an ATOM, but not leading.

This plan differs mainly in that it reuses JazzLights' own protocol and network code, adds
IP sync, and maps WLED to JazzLights when leading (the prototype always advertises pattern
0, which is solid black). The prototype contributed these ideas, which this doc now
includes: randomizing the unused pattern bits, evidence that Bluetooth works inside WLED,
board presets, debug logging, and using `stateUpdated()` when following.

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
2. **Hold cycles (2 bytes).** How many 10 second cycles the current pattern has been held
   for, 0 when it just started. Updated followers would skip `Begin()` when the same pattern
   restarts, and run the effect clock from the start of the hold, so held patterns no
   longer restart visibly (see [Transitions](#transitions)). 2 bytes cover a week. Deployed
   devices keep restarting as they do today. Effects would need checking for clocks far
   past 10 seconds: for example glitter computes `256 * frame.time`, which overflows a
   32-bit int after about 2 hours.
3. **WLED effect and palette IDs (2 bytes).** Lets WLED followers reproduce a WLED leader
   exactly, including through vests relaying over Bluetooth. It also lets JazzLights devices
   implement some WLED effects over time, falling back to the base pattern for the rest.
4. **Speed and intensity (2 bytes).** Only useful together with 3.
5. **Custom palette (up to 48 bytes, UDP only).** 16 RGB entries. Doesn't fit in BLE.

The 6 bytes left in BLE fit either the two colors, or up to three of the 2-byte fields.
More would need a second advertisement, which is a bigger change.

### Recommendation

Make no protocol change for now. Ship the plan above, and see which gaps people actually
notice. If colors turn out to matter most, field 1 is the smallest change with the
biggest effect. If held patterns restarting every 10 seconds bothers people more, field 2
is.

## Decisions so far

- A follower translates the followed pattern to the closest WLED effect. Rendering
  JazzLights' own effects inside WLED is out of scope.
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
- The usermod is a standalone `JL_WLED` flag, not a `JL_CONFIG`, so it can run on any
  JazzLights device, such as a vest. The board presets are JazzLights' own boards.
- The doc gives the mapping principles. The full effect table lives in the code.
- Builds with the usermod append `_JazzLights` to their release name, so WLED's updates
  don't replace them with stock WLED.
- When leading, the unused pattern bits are randomized once per change.
- The usermod ships board presets, and sends JazzLights logs to WLED's debug output.
- WLED changes, playlists included, switch followers immediately, like a button press.
- All the LEDs of a device show the same pattern and palette: a follower applies the
  followed pattern to every segment, each at its own size.
- On the ATOM Matrix, a generated `ledmap.json` maps the vest's LEDs, and the 5x5 screen is
  a second segment of the same canvas.
- The default mode is follower, so that a newly flashed WLED device doesn't take over.
- Palettes without an exact equivalent go to the closest JazzLights palette by hue.
- A follower keeps the segment's sliders, and switches at the end of each 10 seconds without
  waiting for the leader's message.
- Bluetooth uses the NimBLE stack built into WLED's ESP-IDF libraries, not NimBLE-Arduino.
- The NanoC6 build leaves out audioreactive, since it has no microphone, and Bluetooth,
  since it doesn't fit otherwise.
- A person's change pauses following for 10 minutes in follower mode, and only adds the
  gain in leader mode, so override and admin always take over a WLED leader.
- When a WLED leader is taken over, its LEDs follow the new leader.

There are no open questions right now.
