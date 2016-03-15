#ifndef DFWP_SERVER_H
#define DFWP_SERVER_H
#include <netinet/in.h>

namespace dfwearables {
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
} // namespace

#endif /* DFWP_SERVER_H */