#!/usr/bin/env python3

import requests
import socket
from sys import argv
from time import sleep


if len(argv) > 1:
  path = argv[1]
else:
  path = 'jl-clouds-enabled'

if not path.startswith('/'):
  path = '/' + path

# In theory we could just pass in the hostname to requests, but the ESP32
# does not respond to mDNS AAAA requests and that leads requests to hang for
# a couple seconds before trying IPv4.
host = 'jazzlights-clouds.local'
address = socket.gethostbyname(host)
print('Resolved {h} to {a}'.format(h=host, a=address))

url = 'http://' + address + path

enable = True
while True:
  print('GET:')
  resp = requests.get(url)
  print(resp.status_code)
  print(resp.text)

  print('POST:' + ('enable' if enable else 'disable'))
  resp = requests.post(url, data='{"active": "' + ('true' if enable else 'false') + '"}')
  print(resp.status_code)
  print(resp.text)

  enable = not enable
  sleep(1)
