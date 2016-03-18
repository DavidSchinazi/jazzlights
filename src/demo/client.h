#ifndef DFSPARKS_CLIENT_H
#define DFSPARKS_CLIENT_H
#include <atomic>

namespace dfsparks {
   namespace client {
   		void run(std::atomic<bool>& stopped);
   } // namespace server
} // namespace dfsparks

#endif /* DFSPARKS_CLIENT_H */