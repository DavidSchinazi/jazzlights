#include <sstream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

extern char version[];

const char* sysinfo() {
  static char res[512];

  using namespace std;
  stringstream out;

  out << "sysinfo " << version << " ";

  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) < 0) {
    out << "unknown";
  } else {
    out << hostname;
  }
  out << " ";

  struct ifaddrs* addrs, *addr;
  if (getifaddrs(&addrs) < 0) {
    out << "unknown";
  } else {
    addr = addrs;
    const char* sep = "";

    while (addr) {
      if (addr->ifa_addr && addr->ifa_addr->sa_family == AF_INET) {
        struct sockaddr_in* paddr = (struct sockaddr_in*)addr->ifa_addr;
        const char* saddr = inet_ntoa(paddr->sin_addr);
        if (strcmp(saddr, "127.0.0.1")) {
          out << sep << addr->ifa_name << ':' << saddr;
          sep = ",";
        }
      }
      addr = addr->ifa_next;
    }
    freeifaddrs(addrs);
  }

  strncpy(res, out.str().c_str(), sizeof(res));
  return res;
}
