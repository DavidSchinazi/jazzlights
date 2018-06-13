# How to contribute?

First of all, thanks for your help!

Everyone is welcome to contribute by submitting bug reports, writing new
patterns, improving documentation, writing examples, testing on different
boards, profiling and optimizing, etc.

If you're planning a big code change, it's a good idea to to open an 
issue and discuss your plans first. Otherwise, follow the usual process - 
fork a repo, make the changes, submit pull request.

By contributing your code, you agree to license your contribution under the 
terms of the Apache License v2.0.

## Setting up development environment

While you can hack on the library using only Arduino IDE (we're targeting 1.8.5), 
it is much easier to develop on desktop and use the included demo application to 
test your work.

For this, you will need a reasonably modern C++ compiler (C++14 or better), 
CMake (3.9+), and GLFW library. On Ubuntu 18.04 LTS this will set you up:

```shell
apt-get update
apt-get install -y build-essential cmake git libglfw3-dev

git clone https://github.com/unisparks/unisparks.git
cd unisparks
make all
make demo
```

## Code style

Follow [Arduino Style Guide for Writing Libraries](https://www.arduino.cc/en/Reference/APIStyleGuide), 
except for the part that asks you to target users who have no programming expeience - our intended 
audience are not complete beginners. Keep it simple, but don't dumb it down.
