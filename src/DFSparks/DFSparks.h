#ifndef DFSPARKS_H
#define DFSPARKS_H

#include "DFSparks_Color.h"
#include "DFSparks_Network.h"
#include "DFSparks_Player.h"

using DFSparks_Player = dfsparks::Player;
using DFSparks_NetworkPlayer = dfsparks::NetworkPlayer;

template <typename UDP>
using DFSparks_WifiUdpPlayer = dfsparks::WiFiUdpPlayer<UDP>;

#endif /* DFSPARKS_H */