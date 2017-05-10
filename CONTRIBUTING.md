# How to contribute to this project

First of all, thanks for taking the time to help!

## Makeing code changes

- It's not a bad idea to discuss your plans first so that we all know what's in the pipeline.
- Fork the repository.
- Make and test your changes. Ensure that they work on ESP8266 and Ubuntu PC.
- Create a pull request.

Follow [Arduino Style Guide for Writing Libraries](https://www.arduino.cc/en/Reference/APIStyleGuide), 
except for the part that asks you to target users who have no programming expeience - our intended 
audience are not complete beginners. Keep it simple, but don't dumb it down.

## Adding new effects

### Simple stateless effects

- Add rendering function to src/effects/simple.cpp
- Register your new effect as a functionalEffect in src/playlists/default.h

### Complicated or stateful effects

- Create header file, e.g. src/dfsparks/effects/coolstuff.h
- Create source file, e.g. src/dfsparks/effects/coolstuff.cpp
- Extend StatfulEffect (or BasicEffect if you're doing something unusual, or Effect
  if you want to start from scratch)
- Register your new effect in src/playlists/default.h
- Add new effect sources to `CMakeLists.txt`


