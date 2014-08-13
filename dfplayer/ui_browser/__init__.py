# -*- coding: utf-8 -*-
# Licensed under The MIT License

import os
import sys
import time
import base64
import gevent
import logging
import StringIO

from flask import Flask, request, render_template, make_response, abort
from flask.ext.socketio import SocketIO, emit

from ..util import catch_and_log, PROJECT_DIR, PACKAGE_DIR

STREAM_FRAMES = False
# STREAM_FRAMES = True

app = Flask(__name__)
socketio = SocketIO(app)
player = None


def workthread():
    while True:
        with catch_and_log():
            gevent.sleep((0.5 if not STREAM_FRAMES else 0.1))
            socketio.emit('player_state', {
                'status': player.status,
                'volume': player.volume,
                'elapsed': player.elapsed_time,
                'clipName': player.clip_name,
                }, namespace='/player')

            if STREAM_FRAMES and player.status == 'playing':
                img = player.get_frame_image()
                outbuf = StringIO.StringIO()
                img.save(outbuf, 'JPEG')
                imgdata = base64.b64encode(outbuf.getvalue())
                outbuf.close()

                # print ">>>", len(imgdata)

                socketio.emit('frame', {'data': imgdata},
                              namespace='/player')


@app.route('/')
def index():
    return render_template('index.html')


@app.route('/currframe')
def currframe():
    pass


@socketio.on('play', namespace='/player')
@catch_and_log()
def play(msg):
    print 'Play'
    player.play(msg['clipIdx'])


@socketio.on('pause', namespace='/player')
@catch_and_log()
def pause():
    print 'Pause'
    player.pause()


@socketio.on('resume', namespace='/player')
@catch_and_log()
def resume():
    print 'Resume'
    player.resume()


@socketio.on('next', namespace='/player')
@catch_and_log()
def next():
    print 'Next'
    player.next()


@socketio.on('prev', namespace='/player')
@catch_and_log()
def prev():
    print 'Prev'
    player.prev()


@socketio.on('volume', namespace='/player')
@catch_and_log()
def volume(msg):
    print 'Volume', msg['volume']
    player.volume = msg['volume']


@socketio.on('play-effect', namespace='/player')
@catch_and_log()
def play_effect(msg):
    print 'Effect ', msg
    player.play_effect( **msg )


@socketio.on('next-preset', namespace='/player')
@catch_and_log()
def next_preset():
    print 'Next preset'
    player.select_next_preset( True )


@socketio.on('prev-preset', namespace='/player')
@catch_and_log()
def prev_preset():
    print 'Prev preset'
    player.select_next_preset( False )


def run(with_player, host, port):
    global player
    player = with_player
    gevent.spawn(workthread)
    print 'HTTP server listening on %s:%d' % (host, port)
    socketio.run(app, host=host, port=port)


