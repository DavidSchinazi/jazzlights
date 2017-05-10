#ifndef DFSPARKS_H
#define DFSPARKS_H

#include "dfsparks/color.h"
#include "dfsparks/player.h"
#include "dfsparks/log.h"
#include "dfsparks/networks/esp8266wifi.h"

typedef dfsparks::PixelMatrix DFSparks_PixelMatrix;
typedef dfsparks::PixelMap DFSparks_PixelMaP;
typedef dfsparks::NetworkPlayer DFSparks_Player;
typedef dfsparks::RgbaColor DFSparks_Color;

#ifdef ESP8266
	typedef dfsparks::Esp8266Network DFSparks_Esp8266Network;
#endif

#endif /* DFSPARKS_H */