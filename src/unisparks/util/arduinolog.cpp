#include "unisparks/util/log.hpp"
#include <Arduino.h>

namespace unisparks {

extern const char* level_str[];

void defaultHandler(const LogMessage& msg) {
  Serial.print(level_str[static_cast<int>(msg.level)]);
  Serial.print(": ");
  Serial.println(msg.text);
};

} // namespace unisparks

