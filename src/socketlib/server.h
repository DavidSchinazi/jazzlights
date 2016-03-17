#ifndef DFSPARKS_SERVER_H
#define DFSPARKS_SERVER_H
#include <netinet/in.h>

namespace dfsparks {
   
   class Server {
   public:
      Server();
      Server(const Server&) = delete;
      ~Server();

      Server& operator=(Server&) = delete;

      void broadcast_solid_color(uint32_t color); 

   private:
      int sockfd; 
      sockaddr_in clientaddr;  
   };
} // namespace dfsparks

#endif /* DFSPARKS_SERVER_H */