# Home Assistant Integration for Jazzlights

This directory contains a [Home Assistant](https://www.home-assistant.io/)
integration that allows controlling the clouds.

It is designed to be installed as a
[custom component](https://developers.home-assistant.io/docs/creating_integration_file_structure#where-home-assistant-looks-for-integrations).
Assuming that your Home Assistant
[configuration folder](https://www.home-assistant.io/docs/configuration/)
is at `$CONFIG`, you can install the component by placing a symlink to
your copy of JazzLights:

```sh
  cd $CONFIG
  git clone https://github.com/DavidSchinazi/jazzlights
  mkdir custom_components
  cd custom_components
  ln -s ../jazzlights/extras/home-assistant/jazzlights .
```

Then, restart Home Assistant, and go to
`Settings > Devices & services > Add integration` and type `jazzlights`.
You should then be able to follow the prompts to enable and set up the
JazzLights integration.
