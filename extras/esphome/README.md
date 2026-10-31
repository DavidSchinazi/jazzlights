# ESPHome Component for JazzLights

This directory contains an [ESPHome](https://esphome.io/) external component
that runs JazzLights on the clouds, so that they show up in
[Home Assistant](https://www.home-assistant.io/) as a regular ESPHome device.
This is an alternative to the custom integration in
[extras/home-assistant](../home-assistant), which talks to the WebSocket server
in the `clouds` PlatformIO environments.

The component runs the full JazzLights stack: rendering, the protocol engine,
Bluetooth via NimBLE, and UDP multicast via `Esp32SocketManager`. ESPHome
manages the Wi-Fi connection itself. It uses the ESP-IDF framework without the
Arduino core, and currently only supports the clouds on an ATOM Matrix.

## Usage

```sh
cd extras/esphome
cp secrets.example.yaml secrets.yaml  # Then fill in real values.
esphome run clouds.yaml
```

To use the component from another ESPHome configuration, add:

```yaml
external_components:
  # The github:// shorthand only finds components at the root of the repository, so this needs the long form.
  - source:
      type: git
      url: https://github.com/DavidSchinazi/jazzlights
      ref: main
      path: extras/esphome/components
    components: [jazzlights]

jazzlights:

light:
  - platform: jazzlights
    name: Clouds

button:
  - platform: jazzlights
    name: Next pattern
```

To sync with other JazzLights devices over Wi-Fi, the clouds need to be on the
same network as them, which is the `JazzLights` network for devices built with
PlatformIO. Bluetooth syncing works regardless of the Wi-Fi network.

## Entities

- **Light**: turning it on renders JazzLights patterns, which is shown as the
  `Clouds` effect. Picking a color, or the `None` effect, renders that color
  instead. Turning it off hands the clouds back to their default patterns at
  full brightness, like the WebSocket server does.
- **Next pattern button**: switches to the next JazzLights pattern.

## How it works

- `components/jazzlights` is the ESPHome component. Its `__init__.py` sets the
  JazzLights build flags (`JL_CONFIG=CLOUDS`, `JL_ESPHOME=1`, ...) and the
  sdkconfig options for NimBLE.
- `idf_components/jazzlights` builds `src/jazzlights` as an ESP-IDF component,
  and pulls in FastLED from git.
- `idf_components/arduino-esp32` stands in for the Arduino core that FastLED
  requires, using the shim in `extras/fastled_idf_shim`.

The component is incompatible with ESPHome's own Bluetooth components (such as
`esp32_ble_tracker` or `bluetooth_proxy`), since they use Bluedroid.
