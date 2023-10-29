#!/usr/bin/python3

# Tool used to generate the layout for the staff.

def printLayout(l, num=""):
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
  s += 'static_assert(JL_LENGTH(pixelMap{num}) == {lednum}, "bad size");'.format(num=num, lednum=len(l))
  print(s)

l = []
xShaft = 2
oyShaft = 1
yDiffShaft = 1
x = xShaft
y = oyShaft
for i in range(36):
  l.append((x, y))
  y += yDiffShaft

printLayout(l)

w = 4
xDiff = 1
yDiff = -1
ox = 0
oy = 0

l2 = []
xUp = True
x = ox
y = oy
for i in range(33):
  l2.append((x, y))
  if xUp:
    x += xDiff
    if x >= ox + w * xDiff:
      xUp = False
  else:
    x -= xDiff
    if x <= ox:
      xUp = True
      y += yDiff

printLayout(l2, 2)

