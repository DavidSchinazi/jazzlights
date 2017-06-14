#ifndef UNISPARKS_H
#define UNISPARKS_H

#include "unisparks/color.h"
#include "unisparks/log.h"
#include "unisparks/networks/arduinoethernet.h"
#include "unisparks/networks/esp8266wifi.h"
#include "unisparks/networks/udpsocket.h"
#include "unisparks/player.h"

typedef unisparks::PixelMatrix Unisparks_PixelMatrix;
typedef unisparks::PixelMap Unisparks_PixelMaP;
typedef unisparks::NetworkPlayer Unisparks_Player;
typedef unisparks::RgbaColor Unisparks_Color;

// For backwards compartibility
typedef unisparks::PixelMatrix DFSparks_PixelMatrix;
typedef unisparks::PixelMap DFSparks_PixelMaP;
typedef unisparks::NetworkPlayer DFSparks_Player;
typedef unisparks::RgbaColor DFSparks_Color;


#ifdef ESP8266
	typedef unisparks::Esp8266Network Unisparks_Esp8266Network;
	typedef unisparks::Esp8266Network DFSparks_Esp8266Network;
#endif

#endif /* UNISPARKS_H */