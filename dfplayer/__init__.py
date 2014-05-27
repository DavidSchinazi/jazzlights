import os
import sys
import json
import gevent
import logging

from flask import Flask,request,render_template,make_response,abort
from flask.ext.socketio import SocketIO, emit
from pprint import pprint

app = Flask(__name__)
socketio = SocketIO(app)
appdir = os.path.abspath(os.path.dirname(__file__))

playlist = ['Clip #%d' % i for i in xrange(1,5)]
player = { 'status': 'paused', 'clipIdx': 0, 'clipName': playlist[0], 
    'volume': 0.5, 'elapsed': 0 }

def workthread():
    while True:
        socketio.emit('player_state', player, namespace="/player" )
        gevent.sleep(0.5)
        if player['status'] == 'playing':
            player['elapsed'] += 0.5


@app.route('/')
def index():
    return render_template('index.html')


@socketio.on('connect', namespace="/player")
def connect():
    emit('playlist', playlist )
    emit('player_state', player )

@socketio.on('disconnect', namespace="/player")
def disconnect():
    pass

@socketio.on('play', namespace="/player")
def play(msg):
    idx = msg['clipIdx'] % len(playlist)
    player['clipIdx'] = idx
    player['clipName'] = playlist[idx]
    player['elapsed'] = 0
    player['status'] = 'playing'     
    emit('player_state', player, broadcast=True )


@socketio.on('pause', namespace="/player")
def pause():
    player['status'] = 'paused'     
    emit('player_state', player, broadcast=True )


@socketio.on('resume', namespace="/player")
def resume():
    player['status'] = 'playing'     
    emit('player_state', player, broadcast=True )


@socketio.on('next', namespace="/player")
def next():
    play({'clipIdx': (player['clipIdx'] + 1) % len(playlist)})


@socketio.on('prev', namespace="/player")
def prev():
    play({'clipIdx': (player['clipIdx'] - 1) % len(playlist)})


@socketio.on('volume', namespace="/player")
def volume(msg):
    player['volume'] = msg['volume']     
    emit('player_state', player, broadcast=True )


def main():
    logging.basicConfig(level=logging.DEBUG, format='%(message)s')
    if len(sys.argv)>1 and sys.argv[1][:9] == '--listen=':
        (host,port) = sys.argv[1][9:].split(':')
        port = int(port)
    else:
        host = '127.0.0.1'
        port = 8080

    gevent.spawn(workthread)
    print "Starting app on %s:%d" % (host,port)    
    socketio.run(app,host=host, port=port)    