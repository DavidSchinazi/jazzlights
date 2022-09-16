#include "jazzlights/util/log.hpp"
#include <Arduino.h>
#include <string>

namespace jazzlights {

extern const char* level_str[];

void defaultHandler(const LogMessage& msg) {
  std::string logMsg =
    std::string(level_str[static_cast<int>(msg.level)]) +
    ": " + msg.text;
  Serial.println(logMsg.c_str());
};

}  // namespace jazzlights

