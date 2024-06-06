#!/usr/bin/python3

# Tool used to generate the layout for the captain hat.

emptyX = -1337
emptyY = -1337


def printLayout(points, name):
    s = "constexpr Point {name}PixelMap".format(name=name) + "[] = {\n  "
    i = 0
    for x, y in points:
        if x == emptyX and y == emptyY:
            s += "EmptyPoint(), "
        else:
            s += "{{{x:.2f}, {y:.2f}}}, ".format(x=x, y=y)
        if i % 8 == 7:
            s += "\n  "
        i += 1
    if i % 8 == 0:
        s = s[:-3]
    s += "\n};"
    s += "\n\n"
    s += 'static_assert(JL_LENGTH({name}PixelMap) == {lednum}, "bad size");\n'.format(
        name=name, lednum=len(points)
    )
    s += "PixelMap {name}Pixels(JL_LENGTH({name}PixelMap), {name}PixelMap);\n\n".format(
        name=name
    )
    print(s)


ox = 0
oy = 0
xDiff = 1
yDiff = 1
starts = [0, 17, 19, 34, 37, 50, 53, 64, 66, 80, 82, 89, 90, 100]

x = ox
y = oy
isCloud = False
points = []
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
        points.append((x, y))
    else:
        points.append((emptyX, emptyY))
cloudLengths[x] = y + 1

printLayout(points, "cloud")

ceilings = [(42, 46), (38, 42), (10, 32)]
x = 0
for ceil in ceilings:
    x += 1
    y = 0
    points = []
    for _ in range(ceil[0]):
        y -= 1
        points.append((x, y))
    for _ in range(ceil[0], ceil[1]):
        points.append((emptyX, emptyY))
    printLayout(points, "ceiling{x}".format(x=x))


print("\n\n  static uint8_t CloudLength(uint8_t cloudNum) {")
print("    switch (cloudNum) {")
for cn in range(1, 1 + max(cloudLengths.keys())):
    print("      case {cn}: return {cl};".format(cn=cn, cl=cloudLengths[cn]))
print("    }\n    return 0;\n  }")
