# -*- coding: utf-8 -*-
# Licensed under The MIT License

import argparse
import logging
import os
import sys

from gevent import monkey, sleep, spawn


def main():
    monkey.patch_all()

    arg_parser = argparse.ArgumentParser(description='Start player')
    arg_parser.add_argument('--listen')
    arg_parser.add_argument('--no-reset', action='store_true')
    arg_parser.add_argument('--disable-net', action='store_true')
    arg_parser.add_argument('--mpd', action='store_true')
    arg_parser.add_argument('--disable-fin', action='store_true')
    arg_parser.add_argument('--uimock', action='store_true')
    arg_parser.add_argument('--max', action='store_true')
    arg_parser.add_argument('--enable-kinect', action='store_true')
    args = arg_parser.parse_args()

    # have to do those imports after monkey patch
    if not args.uimock:
        from .player import Player
    else:
        from .mock_player import Player 
    from .effects import register_all as register_all_effects
    from .ui_browser import run as run_browser_ui
    from .ui_desktop import run as run_desktop_ui


    logging.basicConfig(level=logging.INFO, format='%(message)s')

    if args.listen:
        (host, port) = args.listen.split(':')
        port = int(port)
    else:
        host = '127.0.0.1'
        port = 8080
 
    player = Player(
        'playlist', args.mpd, not args.disable_net, not args.disable_fin,
        args.enable_kinect)

    if args.no_reset:
        player.disable_reset()
    player.play(0)
    print player

    register_all_effects()

    tasks = [lambda : player.run()]
    tasks.append(lambda : run_desktop_ui(player, args))
    tasks.append(lambda : run_browser_ui(player, host, port))

    try:
        while len(tasks) > 1:
            spawn(tasks.pop(0))
        tasks.pop()()
    except KeyboardInterrupt:
        player.pause()
        print '\nInterrupted'
        exit(0)


