#ifndef JAZZLIGHTS_VEST_H
#define JAZZLIGHTS_VEST_H

#if WEARABLE

namespace jazzlights {

void vestSetup(void);
void vestLoop(void);

std::string wifiStatus(Milliseconds currentTime);
std::string bleStatus(Milliseconds currentTime);
std::string otherStatus(Player& player, Milliseconds currentTime);

}  // namespace jazzlights

#endif  // WEARABLE

#endif  // JAZZLIGHTS_VEST_H
