from gevent import monkey, sleep, spawn
monkey.patch_all()

import os
import sys
import logging

from .player import Player
from .ui_web import run as run_web_ui
from .ui_gtk import run as run_gtk_ui

def main():
    logging.basicConfig(level=logging.INFO, format='%(message)s')
    if len(sys.argv)>1 and sys.argv[1][:9] == '--listen=':
        (host,port) = sys.argv[1][9:].split(':')
        port = int(port)
    else:
        host = '127.0.0.1'
        port = 8080

    player = Player()
    player.reload_playlist()
    player.pause()
    #player.resume()
    print player    

    tasks = [ lambda: player.run() ]
    tasks.append( lambda: run_web_ui(player,host,port) )
    tasks.append( lambda: run_gtk_ui(player) )    
        
    try:
        while len(tasks) > 1:
            spawn( tasks.pop(0) )
        tasks.pop()()
    except KeyboardInterrupt:
        player.pause()
        print "\nInterrupted"
        exit(0)


