#!/usr/bin/python3

# Tool used to generate the layout for the captain hat.

emptyX = -1
emptyY = -1

def printLayout(l, num=""):
  s = '{\n  '
  i = 0
  for (x, y) in l:
    if x == emptyX and y == emptyY:
      s+= 'EmptyPoint(), '
    else:
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

ox = 0
oy = 0
xDiff = 1
yDiff = 1
starts = [0, 17, 19, 34, 37, 50, 53, 64, 66, 80, 82, 89, 90, 100]

x = ox
y = oy
isCloud = False
l = []
cloudLengths = {}
for index in range(starts[-1]):
  if index in starts:
    isCloud = not isCloud
    if isCloud:
      cloudLengths[x] = y + 1
      y = oy
      x += xDiff
  elif isCloud:
    y += yDiff
  if isCloud:
    l.append((x, y))
  else:
    l.append((emptyX, emptyY))
cloudLengths[x] = y + 1

printLayout(l)

print('\n\n  static uint8_t CloudLength(uint8_t cloudNum) {')
print('    switch (cloudNum) {')
for cn in range(1, 1 + max(cloudLengths.keys())):
  print('      case {cn}: return {cl};'.format(cn=cn, cl=cloudLengths[cn]))
print('    }\n    return 0;\n  }')