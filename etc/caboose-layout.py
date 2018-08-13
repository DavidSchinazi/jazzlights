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



print scoord

INCH_IN_M = 0.0254
HOR_SPAN=INCH_IN_M*250.0/43
VER_SPAN=INCH_IN_M*128/10

for (cx, cy) in scoord:
    print "   %3.2f, %3.2f, %3.2f," % (cx*HOR_SPAN, cy*VER_SPAN, 0)
