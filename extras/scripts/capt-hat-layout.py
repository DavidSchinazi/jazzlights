#!/usr/bin/python3

# Tool used to generate the layout for the captain hat.

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

ox = 0
xDiff = 1
yHat = 0
numled = 48

x = ox
y = yHat
l = []
for i in range(numled):
  l.append((x, y))
  if i < numled / 2 - 1 or i >= numled - 2:
    x += xDiff
  else:
    x -= xDiff

printLayout(l)
