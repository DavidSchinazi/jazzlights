#!/usr/bin/python3

# Used to tweak the robot layout in a rush.


xDiff = 0.3
yDiff = 0.25
robotWallMaxX = 14.5
startUp = True

strands = [
  {
    'comment': 'Robot outside wall',
    'maxX': robotWallMaxX,
    'maxY': 1.72,
    'minY': 0.0,
    'numX': 24,
    'numY': 7,
    'startMidWay': False,
    'specialStart': True,
    'controller': 10102,
    'strip': 0,
    # x = [7.5 .. 13.9] with diff 0.3 
    # on lower y =  [0.2 .. 1.72] with diff 0.25
    # on higher y = [0.0 .. 1.52] with diff 0.25
  },
  {
    'comment': 'Robot head',
    'maxX': robotWallMaxX + 5.2,
    'maxY': 1.38,
    'minY': 1.0,
    'numX': 17,
    'numY': 2,
    'startMidWay': True,
    'specialStart': False,
    'controller': 10102,
    'strip': 1,
    # x = [14.3 .. 16.59] with diff 0.15
    # on lower y =  [1.0  .. 1.14] with diff 0.14
    # on higher y = [1.07 .. 1.21] with diff 0.14
  },
  {
    'comment': 'Caboose outside wall new 12V WS2801',
    'maxX': 6.6,
    'maxY': 1.72,
    'minY': 0.0,
    'numX': 10,
    'numY': 6,
    'startMidWay': True,
    'specialStart': False,
    'controller': 10106,
    'strip': 0,
  },
]

for strand in strands:
  maxX = strand['maxX']
  maxY = strand['maxY']
  minY = strand['minY']
  numX = strand['numX']
  numY = strand['numY']
  startMidWay = strand['startMidWay']

  l = []
  x = maxX
  if startMidWay:
    minY += yDiff/2
    maxY -= yDiff/2
  y = maxY if startUp else minY

  for xi in range(numX):
    thisMinY = minY
    thisMaxY = maxY
    up = (xi % 2 == (0 if startUp else 1))
    maxyi = numY
    if strand['specialStart']:
      if xi <= 2:
        maxyi = 3
      if xi == 0:
        thisMinY = 1.26
    yMult = -1.0 if up else 1.0
    for yi in range(maxyi):
      l.append((x, y))
      y += yDiff * yMult
    if up:
      y = thisMinY
    else:
      y = thisMaxY
    x -= xDiff

  s = '# {name} ({num} LEDs)\n'.format(name=strand['comment'], num=len(l)*2)
  s += '[[strand]]\n'
  s += 'renderers = [{'
  s += 'type="pixelpusher", addr="10.1.64.102", controller={c}, group=10001, port=7331, strip={s}'.format(c=strand['controller'], s=strand['strip'])
  s += '}]\n'
  s += 'layout = {type="pixelmap", coords=['
  for c in l:
    # print("({:.2f}, {:.2f})".format(c[0], c[1]))
    for i in range(2):
      s += "{:.2f}, {:.2f}, 0.0,   ".format(c[0], c[1])
  s = s[:-4]
  s += ']}\n'
  print(s)
