#include <sstream>
#include <system_error>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "socket.h"

namespace dfsparks {
	//namespace udp {
	
		// ====================================================================
		// Socket
		// ====================================================================

		sockaddr_in mkaddr(const char *host, int port) {
			sockaddr_in addr;	
		    memset((char *)&addr, 0, sizeof(addr));
		    addr.sin_family = AF_INET;
			addr.sin_port = htons((unsigned short)port);
			if (!strcmp(host, "*")) {
  				addr.sin_addr.s_addr = htonl(INADDR_ANY);
  			} 
  			else {
  			  	addr.sin_addr.s_addr = inet_addr(host);	
  			}
  			return addr;
		}

        Socket::Socket() {
        	fd = socket(AF_INET, SOCK_DGRAM, 0);
  			if(fd < 0) {
				throw std::system_error(errno, std::system_category());
			}
        }
        
        Socket::~Socket() {
        	close(fd);
        }

        Socket& Socket::set_reuseaddr(bool v) {
        	int optval = v;
        	return setopt(SO_REUSEADDR, &optval, sizeof(optval));
        }

        Socket& Socket::set_broadcast(bool v) {
        	int optval = v;
        	return setopt(SO_BROADCAST, &optval, sizeof(optval));
        }

     	Socket& Socket::set_rcvtimeout(uint64_t millis) {
     		struct timeval tv;
			tv.tv_sec = millis/1000;
			tv.tv_usec = (millis % 1000)*1000;
			return setopt(SO_RCVTIMEO, (char *)&tv, sizeof(tv));
     	}
        
        Socket& Socket::setopt(int option_name, const void *val, size_t valsize) {
  			if (::setsockopt(fd, SOL_SOCKET, option_name, val, valsize) < 0) {
				throw std::system_error(errno, std::system_category(), "Can't set socket option");
			}
        	return *this;
        }

         void Socket::bind(const char *host, int port) {
         	sockaddr_in addr = mkaddr(host, port);
			if (::bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
				std::stringstream msg;
				msg << "Can't bind socket to " << host << ":" << port;
    			throw std::system_error(errno, std::system_category(), msg.str());
  			}
        }
        
        void Socket::sendto(const char *host, int port, void *data, 
        	size_t len) {
         	sockaddr_in clientaddr = mkaddr(host, port);
			if (::sendto(fd, data, len, 0, (struct sockaddr *)&clientaddr, 
				sizeof(clientaddr)) < 0) {
				std::stringstream msg;
				msg << "Can't send " << len << " bytes to " << host << ":" << port;
			    throw std::system_error(errno, std::system_category(), msg.str());
			 }
        }

        size_t Socket::recvfrom(void *buf, size_t len, int flags) {
        	sockaddr_in serveraddr;
        	socklen_t serverlen = sizeof(serveraddr);
		    int n = ::recvfrom(fd, buf, len, flags, 
		    	reinterpret_cast<sockaddr*>(&serveraddr), &serverlen);
		    if (n<0) {
			    throw std::system_error(errno, std::system_category());
		    }
		    return n;
        }

		// // ====================================================================
		// // Receiver
		// // ====================================================================
		// static void receive(int portno, void *buf, size_t bufsize, 
		//    std::function<void(void*, size_t)> callback,
		//    std::atomic<bool> &stopped) {
			
		// 	try {
		// 		Socket sock;
		// 		int sockfd;
		// 	    int serverlen;

		// 	    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		// 	    if (sockfd < 0) 
		// 	        error("ERROR opening socket");

		// 	    /* gethostbyname: get the server's DNS entry */
		// 	    server = gethostbyname(hostname);
		// 	    if (server == NULL) {
		// 	        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
		// 	        exit(0);
		// 	    }

		// 	    /* build the server's Internet address */
		// 	    bzero((char *) &serveraddr, sizeof(serveraddr));
		// 	    serveraddr.sin_family = AF_INET;
		// 	    bcopy((char *)server->h_addr, 
		// 		  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
		// 	    serveraddr.sin_port = htons(portno);

		// 	    /* get a message from the user */
		// 	    bzero(buf, BUFSIZE);
		// 	    printf("Please enter msg: ");
		// 	    fgets(buf, BUFSIZE, stdin);

		// 	    /* send the message to the server */
		// 	    serverlen = sizeof(serveraddr);
		// 	    n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
		// 	    if (n < 0) 
		// 	      error("ERROR in sendto");
			    
		// 	    /* print the server's reply */
		// 	    n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
		// 	    if (n < 0) 
		// 	      error("ERROR in recvfrom");
		// 	    printf("Echo from server: %s", buf);
		// 	    return 0;

		// 	}
		// }


  //   Receiver::Receiver(int portno, std::function<void>(void*, 
  //   	size_t) cb, size_t bufsize) : protno(portno), bufsize(bufsize), 
  //   	stopped(true), callback(cb) {
  //   }

  //   Receiver::~Receiver() {
  //   	stop();
  //   }

  //   void Receiver::start() {
  //   	bool was_stopped = stopped.exchange(false);
  //   	if (was_stopped) {
  //   		thread.swap(std::thread(listen, portno, &buf[0], buf.size(), callback, stopped));
  //   	}
  //   }
     
  //   void Receiver::stop() {
  //   	bool was_stopped = stopped.exchange(true);
  //   	if (!was_stopped) {
	 //    	thread.join();
  //   	}
  //   }

	//} // namespace udp
} // namespace dfsparks




