import os
import sys
import json
import logging

from flask import Flask,request,render_template,make_response,abort
from threading import Lock
from pprint import pprint

app = Flask(__name__)
appdir = os.path.abspath(os.path.dirname(__file__))

def render_json(json_dict):
    resp = make_response(json.dumps(json_dict)) 
    resp.headers['Content-Type'] = 'application/json'
    return resp


mutex = Lock()
player = { 'status': 'idle', 'playingIdx': 0, 
    'playList': ['Song #1', 'Song #2', 'Song #3'], 'volume': 0.5 }


@app.route('/')
def index():
    return render_template('index.html')


@app.route('/player/', methods=['GET'])
def status():
    return render_json(player)


@app.route('/player/toggle-play/', methods=['POST'])
def toggle_play():
    if player['status'] == 'idle':
        print "Playing..."
        player['status'] = 'playing'
    elif player['status'] == 'playing':
        print "Pausing..."
        player['status'] = 'paused'
    elif player['status'] == 'paused':
        print "Resuming..."
        player['status'] = 'playing'
    return render_json(player)


@app.route('/player/next/', methods=['POST'])
def next():
    player['playingIdx'] = (player['playingIdx'] + 1) % len(player['playList'])
    player['status'] = 'playing'
    return render_json(player)


@app.route('/player/prev/', methods=['POST'])
def prev():
    player['playingIdx'] = (player['playingIdx'] - 1) % len(player['playList'])
    player['status'] = 'playing'
    return render_json(player)


@app.route('/player/volume/', methods=['GET', 'POST'])
def volume():
    return render_json( {'volume':player['volume']} )


def main():
    logging.basicConfig(level=logging.DEBUG, format='%(message)s')
    if len(sys.argv)>1 and sys.argv[1][:9] == '--listen=':
        (host,port) = sys.argv[1][9:].split(':')
        port = int(port)
    else:
        host = '127.0.0.1'
        port = 8080

    app.run(host=host, port=port, threaded=True)