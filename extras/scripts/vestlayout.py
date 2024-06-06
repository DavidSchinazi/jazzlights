#!/usr/bin/python3

# Tool used to generate the layout for the orange vests.

w = 20
h = 18

s = "{\n  "
for i in range(w * h):
    x = 0.0 + i // h
    y = 0.0 + i % h
    if x % 2 == 1:
        y = h - y
    if i % (2 * h) == 0 and x not in [0, 6, 14] or i % (2 * h) == h:
        x -= 0.5
    s += "{{{x:4}, {y:4}}}, ".format(x=x, y=y)
    if i % 8 == 7:
        s += "\n  "
if w * h % 8 == 0:
    s = s[:-3]
s += "\n};"
print(s)
