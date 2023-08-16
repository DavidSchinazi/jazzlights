#!/usr/bin/env python3

# Test program that can receive updates over IP multicast and display them in raw form.
# See also transmitter, implemented in sender.py

import socket
import binascii

def get_ip():
  s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  s.settimeout(0)        
  try:
    # doesn't even have to be reachable
    s.connect(('10.254.254.254', 1))
    IP = s.getsockname()[0]
  except Exception:
    IP = '127.0.0.1'
  finally:
    s.close()
  return IP
    
def main():
  MCAST_GRP = '239.255.223.01'
  MCAST_PORT = 0xDF0D
  
  sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
  try:
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  except AttributeError:
    pass
  sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 32) 
  sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)

  sock.bind((MCAST_GRP, MCAST_PORT))
  #host = socket.gethostbyname(socket.gethostname()) # Eye Pi4 returns 127.0.0.1
  host = get_ip()
  print ('host: {}'.format(host))
  #sock.setsockopt(socket.SOL_IP, socket.IP_MULTICAST_IF, socket.inet_aton(host))
  sock.setsockopt(socket.SOL_IP, socket.IP_ADD_MEMBERSHIP, 
                   socket.inet_aton(MCAST_GRP) + socket.inet_aton(host))

  while 1:
    try:
      data, addr = sock.recvfrom(1024)
    except socket.error:
      print ('Exception')
    else:
      hexdata = binascii.hexlify(data)
      print ('Data = %s' % hexdata)

if __name__ == '__main__':
  main()
  
