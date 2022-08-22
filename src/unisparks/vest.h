#ifndef UNISPARKS_VEST_H
#define UNISPARKS_VEST_H

#if WEARABLE

namespace unisparks {

void vestSetup(void);
void vestLoop(void);

std::string wifiStatus(Milliseconds currentTime);
std::string bleStatus(Milliseconds currentTime);
std::string otherStatus(Player& player, Milliseconds currentTime);

} // namespace unisparks

#endif // WEARABLE

#endif // UNISPARKS_VEST_H
