# Setting up WLED with JazzLights

These steps put WLED with the JazzLights usermod on a vest controller, so that it syncs
with JazzLights devices. For how it works, see [WLED.md](../docs/WLED.md).

## Supported boards

| Board | Environment | Vest LEDs | Notes |
|---|---|---|---|
| M5Stack ATOM Matrix | `jazzlights_atom_matrix` | GPIO 26 | The 5x5 screen is on GPIO 27. Low on memory for large 2D effects. |
| M5Stack ATOMS3R | `jazzlights_atom_s3r` | GPIO 2 | 8 MB of PSRAM: the best choice for 2D effects. |
| M5Stack ATOM S3 Lite | `jazzlights_atom_s3_lite` | GPIO 2 | |
| M5Stamp C3U | `jazzlights_m5stamp_c3u` | GPIO 1 | |
| M5Stack NanoC6 | `jazzlights_m5_nano_c6` | GPIO 2 | No Bluetooth: syncs over Wi-Fi only. |

Each environment sets up a vest's 360 LEDs on a fresh install, plus the screen on the ATOM
Matrix. Only the ATOM Matrix and the ATOMS3R have been tested on hardware so far.

## What you need

- A checkout of this repository, and PlatformIO (`pip install platformio`).
- Node.js 20 on your `PATH`, which WLED's build needs for its web UI. On macOS:

  ```sh
  brew install node@20
  export PATH="/opt/homebrew/opt/node@20/bin:$PATH"
  ```

Run the commands below from the root of this repository, with `<environment>` from the
table. The first build clones WLED into `extras/.wled` and takes a few minutes.

## Steps

1. **Erase the board**, over USB. This has to be its own command:

   ```sh
   extras/wled/build.sh -e <environment> -t erase
   ```

   If it can't connect, put the board in download mode and try again. On the ATOM S3
   boards, hold the reset button on the side for about 2 seconds, until the green light
   comes on. On the M5Stamp C3U and the NanoC6, hold the button while plugging in USB.

2. **Flash WLED with the usermod:**

   ```sh
   extras/wled/build.sh -e <environment> -t upload -t monitor
   ```

   If you had to use download mode, press reset once it's done. On the ATOM Matrix, the
   first boot saves WLED's settings and reboots once, to switch the LED driver to one that
   works alongside Bluetooth. The serial output only says "Ada": see
   [Logs](#logs) for more.

3. **Connect it to Wi-Fi.** Join the `WLED-AP` network (password `wled1234`), open
   `http://4.3.2.1`, enter your network under **Config → WiFi Setup**, and save. Find the
   device's new address in your router, or in the serial output of a debug image. If your
   phone or laptop can't join `WLED-AP`, try again: the usermod pauses Bluetooth every
   other second so that it can. You can also set up Wi-Fi over USB at
   https://www.improv-wifi.com in Chrome.

4. **Check the LEDs** under **Config → LED Preferences**. After an erase, they're already
   right: 360 LEDs on the vest's pin, and on the ATOM Matrix, 25 LEDs on GPIO 27 first,
   then the vest's 360 on GPIO 26, starting at 25. WLED keeps these settings across
   updates, so check them on a board that ran WLED before.

5. **Map the vest's LEDs to a 2D canvas**, so that WLED's 2D effects look right on it.
   Generate a ledmap, with `--no-screen` on every board but the ATOM Matrix:

   ```sh
   extras/wled/generate_vest_ledmap.py --no-screen > ledmap.json   # Other boards.
   extras/wled/generate_vest_ledmap.py > ledmap.json               # ATOM Matrix.
   ```

   On an ATOM Matrix whose LED settings have the vest first, add `--vest-first`. Upload
   `ledmap.json` in WLED's file editor at `http://<device>/edit`, then reboot it, from
   **Config → Security & Updates** or with its reset button.

6. **Set up segments.** On every board but the ATOM Matrix, there's nothing to do: WLED's
   only segment covers the whole vest. On the ATOM Matrix, the canvas has the vest on the
   left and the screen on the right, so make two segments: segment 0 for the vest, columns
   0 to 20 and rows 0 to 19, and segment 1 for the screen, columns 20 to 25 and rows 0 to 5
   (stop values are exclusive). Switch the usermod's mode to off or leader while doing so
   (next step), since following redraws WLED's web UI every 10 seconds. Then save a preset,
   and choose it under **Config → LED Preferences → Apply preset at boot**, since WLED
   doesn't keep segments across reboots otherwise.

7. **Choose the mode** under **Config → Usermods → JazzLights**:
   - **Follower**, the default: shows what nearby JazzLights devices show, unless someone
     changes WLED, which then leads for 10 minutes.
   - **Leader**: JazzLights devices around show the closest pattern to WLED's effect.
   - **Off**: no JazzLights at all.

8. **Check that it works.** WLED's **Info** tab shows whether the usermod leads or
   follows, and which device it follows. In leader mode, **Fire 2012** should rise from
   the vest's hem.

## Updating

Images built with the usermod update from WLED's update page in one click, from
[DavidSchinazi/jazzlights-wled](https://github.com/DavidSchinazi/jazzlights-wled), and
refuse stock WLED images, which would remove the usermod. Images built before that repo
existed need one more update over USB with step 2. Your settings, ledmap and presets stay.

## Logs

Regular images don't print logs. For WLED's and JazzLights' logs, flash a debug image:

```sh
PLATFORMIO_BUILD_FLAGS=-DWLED_DEBUG extras/wled/build.sh -e <environment> -t upload -t monitor
```

The ATOM Matrix may not have room for it: then add `-DJL_DISABLE_BLUETOOTH=1` to the
flags. On the M5Stamp C3U, debug output takes GPIO 1, the vest's pin. See
[Build and packaging](../docs/WLED.md#build-and-packaging) for sending logs over the
network instead.

## Without a checkout of this repository

Add the usermod to any WLED build from GitHub, as described in
[Build and packaging](../docs/WLED.md#build-and-packaging), then follow the steps from 3.
