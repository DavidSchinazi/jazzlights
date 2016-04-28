#ifndef DFSPARKS_H
#define DFSPARKS_H

#include "DFSparks_Network.h"
//#include "DFSparks_Frame.h"
#include "DFSparks_Color.h"
//#include "DFSparks_Strand.h"
#include "DFSparks_Matrix.h"

//#define DFSPARKS_MAX_FRAME_SIZE dfsparks::frame::max_size
#define DFSPARKS_UDP_PORT dfsparks::udp_port

// typedef dfsparks::Strand DFSparks_Strand;

template<int width, int height>
using DFSparks_Matrix = dfsparks::Matrix<width, height>;

template<typename UDP>
using DFSparks_WiFiUDPClient = dfsparks::WiFiUdpClient<UDP>;

#endif /* DFSPARKS_H */