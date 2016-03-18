#ifndef DFSPARKS_SERVER_H
#define DFSPARKS_SERVER_H
#include <atomic>

namespace dfsparks {
   namespace server {
   		void run(std::atomic<bool>& stopped);
   } // namespace server
} // namespace dfsparks

#endif /* DFSPARKS_SERVER_H */