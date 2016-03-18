#ifndef DFSPARKS_UDP_H
#define DFSPARKS_UDP_H
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace dfsparks {
   //namespace udp {

      class Socket {
      public:
         Socket();
         ~Socket();

         Socket& set_reuseaddr(bool v);
         Socket& set_broadcast(bool v);
         Socket& set_rcvtimeout(uint64_t millis);
         Socket& setopt(int name, const void* val, size_t valsize);

         void bind(const char *host, int port);
         void sendto(const char *host, int port, void *data, size_t len);
         size_t recvfrom(void *buf, size_t len, int flags=0);

      private:
         int fd;
      };


      // class Client {
      // public:
      //    Client(int portno, std::function<void>(void*, size_t) cb, 
      //       size_t bufsize = 65535);
      //    Client(const Client&) = delete;
      //    ~Client();

      //    void start();
      //    void stop();

      //    Client& operator=(Client&) = delete;

      // private:
      //    int portno;
      //    size_t bufsize;
      //    std::atomic<bool> stopped;
      //    std::function<void>(void*, size_t) callback;
      //    std::thread thread;
      // };

   //} // namespace udp
} // namespace dfsparks

#endif /* DFSPARKS_UDP_H */