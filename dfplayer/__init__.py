# -*- coding: utf-8 -*-

import argparse
import logging
import os
import sys

from gevent import monkey, sleep, spawn


def main():
    monkey.patch_all()

    # have to do those imports after monkey patch
    from .player import Player
    from .effects import register_all as register_all_effects
    from .ui_browser import run as run_browser_ui
    from .ui_desktop import run as run_desktop_ui

    arg_parser = argparse.ArgumentParser(description='Start player')
    arg_parser.add_argument('--listen')
    arg_parser.add_argument('--no-reset', action='store_true')
    arg_parser.add_argument('--disable-net', action='store_true')
    arg_parser.add_argument('--line-in', action='store_true')
    args = arg_parser.parse_args()

    logging.basicConfig(level=logging.INFO, format='%(message)s')

    if args.listen:
        (host, port) = args.listen.split(':')
        port = int(port)
    else:
        host = '127.0.0.1'
        port = 8080

    player = Player('playlist', args.line_in, not args.disable_net)
    if args.no_reset:
        player.disable_reset()
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


