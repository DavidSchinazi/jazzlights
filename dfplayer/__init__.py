# -*- coding: utf-8 -*-
import os
import sys
import logging

from gevent import monkey, sleep, spawn


def main():
    monkey.patch_all()

    # have to do those imports after monkey patch
    from .player import Player
    from .effects import register_all as register_all_effects
    from .ui_browser import run as run_browser_ui
    from .ui_desktop import run as run_desktop_ui

    logging.basicConfig(level=logging.INFO, format='%(message)s')
    if len(sys.argv) > 1 and (sys.argv[1])[:9] == '--listen=':
        (host, port) = (sys.argv[1])[9:].split(':')
        port = int(port)
    else:
        host = '127.0.0.1'
        port = 8080

    player = Player()
    player.load_playlist('playlist')
    player.play(0)
    print player

    register_all_effects()

    tasks = [lambda : player.run()]
    tasks.append(lambda : run_browser_ui(player, host, port))
    tasks.append(lambda : run_desktop_ui(player))

    try:
        while len(tasks) > 1:
            spawn(tasks.pop(0))
        tasks.pop()()
    except KeyboardInterrupt:
        player.pause()
        print '\nInterrupted'
        exit(0)


