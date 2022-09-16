#include "jazzlights/util/log.hpp"
#include <stdio.h>

namespace jazzlights {

extern const char* level_str[];

void defaultHandler(const LogMessage& msg) {
  printf("%s: %s\n", level_str[static_cast<int>(msg.level)], msg.text);
};

}  // namespace jazzlights

