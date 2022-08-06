#!/usr/bin/python3

# Tool used to generate the layout for the gecko foot on the stage.

w = 7
h = 7
ox = 10.5
oy = 0.0
resolution = 8

l = []
for i in range(w * h):
  x = 0.0 + i % w
  y = 0.0 + i // w
  if y % 2 == 0:
    x = w - x - 1.0
  x = ox + x / resolution
  y = oy + y / resolution
  l.append((x, y))
  l.append((x, y))

# Add four corners of robot bounding box.
# These values come from robot/tglight.toml
minX = -5.95
maxX = 19.7
minY = -6.45
maxY = 2.93
l.append((minX, minY))
l.append((minX, maxY))
l.append((maxX, minY))
l.append((maxX, maxY))

s = '{\n  '
i = 0
for (x, y) in l:
  s += '{{{x:.2f}, {y:.2f}}}, '.format(x=x,y=y)
  if i % 8 == 7:
    s += '\n  '
  i += 1
if i % 8 == 0:
  s = s[:-3]
s += '\n};'
s += '\n\n'
s += '#define LEDNUM {}'.format(len(l))
print(s)
