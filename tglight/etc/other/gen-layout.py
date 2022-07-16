#!/usr/bin/python3

import sys

out = sys.stdout
# out = open('/Volumes/PPRJ/Projects/tglight/etc/tglight.toml', 'w')

FEET_IN_METER = 3.28084

def gen_caboose(left=0, top=0, flipx=False):
    zigzag1 = [
        (0,0),
        (1,1),
        (0,2),
        (1,3),
        (3,3),
        (2,2),
        (3,1),
        (2,0),
    ]

    scoord = [
    ]

    for i in range(0,8):
        for c in zigzag1:
            scoord.append(((8+i*4)+c[0], 2+c[1]))

    scoord.append( (40,2) )
    scoord.append( (42,2) )
    scoord.append( (41,3) )
    scoord.append( (40,4) )
    scoord.append( (42,4) )
    scoord.append( (41,5) )

    for i in reversed(range(15,33,2)):
        scoord.append( (i, 1) )

    for i in reversed(range(2,15,2)):
        scoord.append( (i, 0) )
        scoord.append( (i-1, 1) )

    scoord.append( (0,0) )
    scoord.append( (0,1) )

    for i in reversed(range(0,7,2)):
        scoord.append( (i,2) )

    scoord.append( (0,3) )
    scoord.append( (1,3) )
    scoord.append( (3,3) )
    scoord.append( (5,3) )
    scoord.append( (7,3) )

    for i in reversed(range(0,7,2)):
        scoord.append( (i,4) )

    scoord.append( (0,5) )
    scoord.append( (1,5) )
    scoord.append( (3,5) )
    scoord.append( (5,5) )
    scoord.append( (7,5) )

    for i in reversed(range(0,7,2)):
        scoord.append( (i,6) )

    scoord.append( (0,7) )
    scoord.append( (1,7) )
    scoord.append( (3,7) )
    scoord.append( (5,7) )
    scoord.append( (7,7) )

    for i in reversed(range(0,7,2)):
        scoord.append( (i,8) )

    scoord.append( (0,9) )
    scoord.append( (1,9) )
    scoord.append( (3,9) )
    scoord.append( (5,9) )
    scoord.append( (7,9) )

    for i in range(0,9):
        for c in zigzag1:
            scoord.append(((8+i*4)+c[0], 6+c[1]))

    scoord.append( (41,7) )
    scoord.append( (42,8) )
    scoord.append( (41,9) )

    INCH_IN_M = 0.0254
    HOR_SPAN=INCH_IN_M*250.0/43
    VER_SPAN=INCH_IN_M*128/10

    print('# Caboose outside wall', file=out)
    print('[[strand]]', file=out)
    print('renderers = [{type="pixelpusher", addr="10.1.64.103", controller=10103, group=10001, port=7331, strip=2}]', file=out)
    print('layout = {type="pixelmap", coords=[', file=out, end='')
    for (cx, cy) in scoord:
        print("   %3.2f, %3.2f, %3.2f," % (left + (-1 if flipx else 1)*cx*HOR_SPAN, cy*VER_SPAN, 0), file=out, end='')
    print(']}\n', file=out)

    print('# Caboose inside wall', file=out)
    print('[[strand]]', file=out)
    print('renderers = [{type="pixelpusher", addr="10.1.64.103", controller=10103, group=10001, port=7331, strip=3}]', file=out)
    print('layout = {type="pixelmap", coords=[', file=out, end='')
    for (cx, cy) in scoord:
        print("   %3.2f, %3.2f, %3.2f," % (left + (-1 if flipx else 1)*cx*HOR_SPAN, cy*VER_SPAN, 0), file=out, end='')
    print(']}\n', file=out)


def gen_dorsal_fins(left=0, top=0):
    dorsal_poles = [
        ('D1', 35, 16),
        ('D2', 35, 16),
        ('D3', 35, 16),
        ('D4', 45, 16),
        ('D5', 45, 20),
        ('D6', 45, 20),
        ('D7', 45, 20),
        ('D8', 45, 20),
        ('D9', 45, 20),
        ('D10', 45, 20),
        ('D11', 42, 20),
        ('D12', 43, 20),
    ]

    x = left
    for stripn, slc in enumerate([dorsal_poles[i:i + 3] for i in range(0, len(dorsal_poles), 3)]):
        print("# Caboose dorsal fins", ' + '.join([v[0] for v in slc]), file=out)
        print("[[strand]]", file=out)
        print('renderers = [{type="pixelpusher", addr="10.1.64.103", controller=10103, group=10001, port=7331, strip=%d}]' % (4+stripn), file=out)
        print('layout = {type="pixelmap", coords=[', file=out, end='')
        for (n, leds, feet) in slc:
            leds += 1
            x += 0.2
            for ny in range(leds):
                print("   %3.2f, %3.2f, %3.2f," % (x, top + -1.0*ny*feet/(FEET_IN_METER*leds), 0), file=out, end='')

        print(']}\n', file=out)


def gen_tail_fins(left=0, top=0):
    left_poles = [
        ('1L', 61, 25),
        ('2L', 50, 20),
        ('3L', 40, 16),
        ('4L', 38, 14.4),
        ('5L', 35, 13.4),
        ('6L', 37, 14.3),
        ('7L', 41, 16),
        ('8L', 50, 20),
    ]

    right_poles = [
        ('1R', 61, 25),
        ('2R', 50, 20),
        ('3R', 45, 16),
        ('4R', 38, 16),
        ('5R', 35, 14.4),
        ('6R', 37, 14.2),
        ('7R', 41, 16),
        ('8R', 50, 20),
    ]

    y = top
    print("# Caboose left (inside) tail fins", file=out)
    print("[[strand]]", file=out)
    print('renderers = [{type="pixelpusher", addr="10.1.64.103", controller=10103, group=10001, port=7331, strip=0}]', file=out)
    print('layout = {type="pixelmap", coords=[', file=out, end='')
    for (name, leds, feet) in left_poles:
        leds = leds+1
        y += 0.3
        for n in range(leds):
            print("   %3.2f, %3.2f, %3.2f," % (left - n*feet/(FEET_IN_METER*leds), y, 0), file=out, end='')
    print(']}\n', file=out)

    y = top
    print("# Caboose right (outside) tail fins", file=out)
    print("[[strand]]", file=out)
    print('renderers = [{type="pixelpusher", addr="10.1.64.103", controller=10103, group=10001, port=7331, strip=1}]', file=out)
    print('layout = {type="pixelmap", coords=[', file=out, end='')
    for (name, leds, feet) in left_poles:
        leds = leds+1
        y += 0.3
        for n in range(leds):
            print("   %3.2f, %3.2f, %3.2f," % (left - n*feet/(FEET_IN_METER*leds), y, 0), file=out, end='')
    print(']}\n', file=out)


def gen_robot_head(left=0, top=0):
    print("# Robot head", file=out)
    print("[[strand]]", file=out)
    print('renderers = [{type="pixelpusher", addr="10.1.64.102", controller=10102, group=10001, port=7331, strip=1}]', file=out)
    print('layout = {type="matrix-zigzag", size=[16,4], origin=[%3.2f, %3.2f], resolution=3}' % (left, top), file=out)
    print('', file=out)


def gen_robot_outside(left=0, top=0):
    robo_triplets = [0, 0, 0, 0, 0, 0, 0, 0.83, 0, 0, 0.83, 0, 0, 1.66, 0, 0, 1.66, 0, 0, 2.49, 0, 0, 2.49, 0, 0, 3.32, 0, 0, 3.32, 0, 0, 4.15, 0, 0, 4.15, 0, 0, 4.98, 0, 0, 4.98, 0, 1, 0.67, 0, 1, 0.67, 0, 1, 1.5, 0, 1, 1.5, 0, 1, 2.33, 0, 1, 2.33, 0, 1, 3.16, 0, 1, 3.16, 0, 1, 3.99, 0, 1, 3.99, 0, 1, 4.82, 0, 1, 4.82, 0, 1, 5.65, 0, 1, 5.65, 0, 2, 0, 0, 2, 0, 0, 2, 0.83, 0, 2, 0.83, 0, 2, 1.66, 0, 2, 1.66, 0, 2, 2.49, 0, 2, 2.49, 0, 2, 3.32, 0, 2, 3.32, 0, 2, 4.15, 0, 2, 4.15, 0, 2, 4.98, 0, 2, 4.98, 0, 3, 0.67, 0, 3, 0.67, 0, 3, 1.5, 0, 3, 1.5, 0, 3, 2.33, 0, 3, 2.33, 0, 3, 3.16, 0, 3, 3.16, 0, 3, 3.99, 0, 3, 3.99, 0, 3, 4.82, 0, 3, 4.82, 0, 3, 5.65, 0, 3, 5.65, 0, 4, 0, 0, 4, 0, 0, 4, 0.83, 0, 4, 0.83, 0, 4, 1.66, 0, 4, 1.66, 0, 4, 2.49, 0, 4, 2.49, 0, 4, 3.32, 0, 4, 3.32, 0, 4, 4.15, 0, 4, 4.15, 0, 4, 4.98, 0, 4, 4.98, 0, 5, 0.67, 0, 5, 0.67, 0, 5, 1.5, 0, 5, 1.5, 0, 5, 2.33, 0, 5, 2.33, 0, 5, 3.16, 0, 5, 3.16, 0, 5, 3.99, 0, 5, 3.99, 0, 5, 4.82, 0, 5, 4.82, 0, 5, 5.65, 0, 5, 5.65, 0, 6, 0, 0, 6, 0, 0, 6, 0.83, 0, 6, 0.83, 0, 6, 1.66, 0, 6, 1.66, 0, 6, 2.49, 0, 6, 2.49, 0, 6, 3.32, 0, 6, 3.32, 0, 6, 4.15, 0, 6, 4.15, 0, 6, 4.98, 0, 6, 4.98, 0, 7, 0.67, 0, 7, 0.67, 0, 7, 1.5, 0, 7, 1.5, 0, 7, 2.33, 0, 7, 2.33, 0, 7, 3.16, 0, 7, 3.16, 0, 7, 3.99, 0, 7, 3.99, 0, 7, 4.82, 0, 7, 4.82, 0, 7, 5.65, 0, 7, 5.65, 0, 8, 0, 0, 8, 0, 0, 8, 0.83, 0, 8, 0.83, 0, 8, 1.66, 0, 8, 1.66, 0, 8, 2.49, 0, 8, 2.49, 0, 8, 3.32, 0, 8, 3.32, 0, 8, 4.15, 0, 8, 4.15, 0, 8, 4.98, 0, 8, 4.98, 0, 9, 0.67, 0, 9, 0.67, 0, 9, 1.5, 0, 9, 1.5, 0, 9, 2.33, 0, 9, 2.33, 0, 9, 3.16, 0, 9, 3.16, 0, 9, 3.99, 0, 9, 3.99, 0, 9, 4.82, 0, 9, 4.82, 0, 9, 5.65, 0, 9, 5.65, 0, 10, 0, 0, 10, 0, 0, 10, 0.83, 0, 10, 0.83, 0, 10, 1.66, 0, 10, 1.66, 0, 10, 2.49, 0, 10, 2.49, 0, 10, 3.32, 0, 10, 3.32, 0, 10, 4.15, 0, 10, 4.15, 0, 10, 4.98, 0, 10, 4.98, 0, 11, 0.67, 0, 11, 0.67, 0, 11, 1.5, 0, 11, 1.5, 0, 11, 2.33, 0, 11, 2.33, 0, 11, 3.16, 0, 11, 3.16, 0, 11, 3.99, 0, 11, 3.99, 0, 11, 4.82, 0, 11, 4.82, 0, 11, 5.65, 0, 11, 5.65, 0, 12, 0, 0, 12, 0, 0, 12, 0.83, 0, 12, 0.83, 0, 12, 1.66, 0, 12, 1.66, 0, 12, 2.49, 0, 12, 2.49, 0, 12, 3.32, 0, 12, 3.32, 0, 12, 4.15, 0, 12, 4.15, 0, 12, 4.98, 0, 12, 4.98, 0, 13, 0.67, 0, 13, 0.67, 0, 13, 1.5, 0, 13, 1.5, 0, 13, 2.33, 0, 13, 2.33, 0, 13, 3.16, 0, 13, 3.16, 0, 13, 3.99, 0, 13, 3.99, 0, 13, 4.82, 0, 13, 4.82, 0, 13, 5.65, 0, 13, 5.65, 0, 14, 0, 0, 14, 0, 0, 14, 0.83, 0, 14, 0.83, 0, 14, 1.66, 0, 14, 1.66, 0, 14, 2.49, 0, 14, 2.49, 0, 14, 3.32, 0, 14, 3.32, 0, 14, 4.15, 0, 14, 4.15, 0, 14, 4.98, 0, 14, 4.98, 0, 15, 0.67, 0, 15, 0.67, 0, 15, 1.5, 0, 15, 1.5, 0, 15, 2.33, 0, 15, 2.33, 0, 15, 3.16, 0, 15, 3.16, 0, 15, 3.99, 0, 15, 3.99, 0, 15, 4.82, 0, 15, 4.82, 0, 15, 5.65, 0, 15, 5.65, 0, 16, 0, 0, 16, 0, 0, 16, 0.83, 0, 16, 0.83, 0, 16, 1.66, 0, 16, 1.66, 0, 16, 2.49, 0, 16, 2.49, 0, 16, 3.32, 0, 16, 3.32, 0, 16, 4.15, 0, 16, 4.15, 0, 16, 4.98, 0, 16, 4.98, 0, 17, 0.67, 0, 17, 0.67, 0, 17, 1.5, 0, 17, 1.5, 0, 17, 2.33, 0, 17, 2.33, 0, 17, 3.16, 0, 17, 3.16, 0, 17, 3.99, 0, 17, 3.99, 0, 17, 4.82, 0, 17, 4.82, 0, 17, 5.65, 0, 17, 5.65, 0, 18, 0, 0, 18, 0, 0, 18, 0.83, 0, 18, 0.83, 0, 18, 1.66, 0, 18, 1.66, 0, 18, 2.49, 0, 18, 2.49, 0, 18, 3.32, 0, 18, 3.32, 0, 18, 4.15, 0, 18, 4.15, 0, 18, 4.98, 0, 18, 4.98, 0, 19, 0.67, 0, 19, 0.67, 0, 19, 1.5, 0, 19, 1.5, 0, 19, 2.33, 0, 19, 2.33, 0, 19, 3.16, 0, 19, 3.16, 0, 19, 3.99, 0, 19, 3.99, 0, 19, 4.82, 0, 19, 4.82, 0, 19, 5.65, 0, 19, 5.65, 0, 20, 0, 0, 20, 0, 0, 20, 0.83, 0, 20, 0.83, 0, 20, 1.66, 0, 20, 1.66, 0, 20, 2.49, 0, 20, 2.49, 0, 20, 3.32, 0, 20, 3.32, 0, 20, 4.15, 0, 20, 4.15, 0, 20, 4.98, 0, 20, 4.98, 0, 21, 0.67, 0, 21, 0.67, 0, 21, 1.5, 0, 21, 1.5, 0, 21, 2.33, 0, 21, 2.33, 0, 21, 3.16, 0, 21, 3.16, 0, 21, 3.99, 0, 21, 3.99, 0, 21, 4.82, 0, 21, 4.82, 0, 21, 5.65, 0, 21, 5.65, 0, ]

    print(len(robo_triplets)/3)

    print('# Robot outside wall', file=out)
    print('[[strand]]', file=out)
    print('renderers = [{type="pixelpusher", addr="10.1.64.102", controller=10102, group=10001, port=7331, strip=0}]', file=out)
    print('layout = {type="pixelmap", coords=[', file=out, end='')
    for i in range(0, len(robo_triplets), 3):
        print("   %3.2f, %3.2f, %3.2f," % (left + robo_triplets[i]/FEET_IN_METER, top + robo_triplets[i+1]/3.28084, robo_triplets[i+2]/3.28084), file=out, end='')
        #print("   %3.2f, %3.2f, %3.2f," % (left + robo_triplets[i]/FEET_IN_METER, top + robo_triplets[i+1]/3.28084, robo_triplets[i+2]/3.28084), file=out, end='')
    print(']}\n', file=out)


def gen_robot_inside(left=0, top=0):
    print('# Robot inside wall (net #1)', file=out)
    print("[[strand]]", file=out)
    print('renderers = [{type="pixelpusher", addr="10.1.64.102", controller=10102, group=10001, port=7331, strip=2}]', file=out)
    print('layout = {type="matrix-zigzag", size=[20,20], origin=[%3.2f, %3.2f], resolution=7}' % (left, top), file=out)
    print('', file=out)

    print('# Robot inside wall (net #2)', file=out)
    print("[[strand]]", file=out)
    print('renderers = [{type="pixelpusher", addr="10.1.64.102", controller=10102, group=10001, port=7331, strip=3}]', file=out)
    print('layout = {type="matrix-zigzag", size=[20,20], origin=[%3.2f, %3.2f], resolution=7}' % (left + 3.0, top), file=out)
    print('', file=out)


print('ledr=0.07\n', file=out)

gen_tail_fins(left=0, top=0.5)
gen_caboose(left=7, top=0, flipx=True)
gen_dorsal_fins(left=4.5, top=-0.5)

print('# ---- \n', file=out)
gen_robot_outside(left=7.5, top=0)
gen_robot_head(left=14.3, top=-0.7)
gen_robot_inside(left=7.5, top=0)
