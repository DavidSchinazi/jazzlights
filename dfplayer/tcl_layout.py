# -*- coding: utf-8 -*-
#
# Parses layour DXF file format.

import dxfgrabber
import sys


class _Line(object):

  def __init__(self, start, end):
    self._start = start
    self._end = end

  def _get_other_end(self, c):
    if c == self._start:
      return self._end
    if c == self._end:
      return self._start
    raise Exception('Unknown end of line')


class Strand(object):

  def __init__(self, id):
    self._id = id
    self._coords = []

  def get_id(self):
    return self._id

  def get_coords(self):
    return list(self._coords)


class TclLayout(object):

  def __init__(self, file_path, max_x, max_y):
    self._strands = {}
    self._max_x = max_x
    self._max_y = max_y

    dxf = dxfgrabber.readfile(file_path)
    self._parse(dxf)

    self._normalize()

    #for strand in self._strands.values():
    #  print 'Strand #%s, %s leds = %s' % (
    #      strand._id, len(strand._coords), strand._coords)

  def get_strands(self):
    return list(self._strands.values())

  def get_strand_coords(self, id):
    if id >= len(self._strands):
      return None
    return self._strands[id].get_coords()

  def get_all_coords(self):
    result = []
    for s in self._strands.values():
      result += s._coords
    return result

  def _parse(self, dxf):
    anchors = {}
    circles = set()
    dots = {}
    for e in dxf.entities:
      type = e.dxftype
      if e.dxftype == 'TEXT':
        if (len(e.text) != 2 or e.text[0] != 'p' or
            e.text[1] < '1' or e.text[1] > '8'):
          raise Exception('Unsupported anchor text "%s"' % e.text)
        id = int(e.text[1]) - 1
        if id in anchors:
          raise Exception('More than one id of %s', id)
        anchors[id] = self._get_coord(e.insert)
      elif e.dxftype == 'CIRCLE':
        center = self._get_coord(e.center)
        if center in circles:
          raise Exception('More than one circle at %s', [center])
        circles.add(center)
      elif e.dxftype == 'LINE':
        start = self._get_coord(e.start)
        end = self._get_coord(e.end)
        if start == end:
          raise Exception('Line of zero length: %s' % [start])
        line = _Line(start, end)
        self._add_dot(dots, start, line)
        self._add_dot(dots, end, line)
      else:
        raise Exception('Unsupported DXF entity type %s', e.dxftype)

    for id, id_coord in anchors.items():
      strand = Strand(id)
      self._strands[id] = strand
      prev_coord = id_coord
      while True:
        if prev_coord not in dots:
          raise Exception('No lines found to originate from %s' % [prev_coord])
        if len(dots[prev_coord]) == 0:
          raise Exception('All lines were consumed for %s' % [prev_coord])
        if len(dots[prev_coord]) > 1:
          raise Exception('More than one line starts at %s' % [prev_coord])
        in_line = dots[prev_coord][0]
        coord = in_line._get_other_end(prev_coord)
        dots[prev_coord] = []
        if coord not in circles:
          raise Exception('No circle found for %s', [coord])
        circles.remove(coord)
        strand._coords.append(coord)
        dots[coord].remove(in_line)
        if len(dots[coord]) == 0:
          break
        prev_coord = coord

    if len(circles):
      raise Exception('Some circles remain unconsumed: %s', circles)

    for coord, lines in dots.items():
      if len(lines) != 0:
        raise Exception('Some dots remain unconsumed: %s', [coord])

  def _get_coord(self, c):
    if len(c) == 3 and c[2] != 0:
      raise Exception('Non-zero Z coordinate in %s' % [c])
    return (float(c[0]), float(c[1]))

  def _add_dot(self, dots, coord, line):
    if coord not in dots:
      dots[coord] = []
    dots[coord].append(line)
    if len(dots[coord]) > 2:
      raise Exception('More than one line connected at one dot %s', [coord])

  def _normalize(self):
    # Make all coordinates be in range of [0-max_xy].
    min_x = sys.float_info.max
    min_y = sys.float_info.max
    max_x = sys.float_info.min
    max_y = sys.float_info.min
    for s in self._strands.values():
      for c in s._coords:
        x, y = c[0], c[1]
        if x < min_x:
          min_x = x
        if x > max_x:
          max_x = x
        if y < min_y:
          min_y = y
        if y > max_y:
          max_y = y
    width = max_x - min_x
    height = max_y - min_y
    for s in self._strands.values():
      new_coords = []
      for c in s._coords:
        x = (c[0] - min_x) / width * self._max_x
        y = (c[1] - min_y) / height * self._max_y
        y = self._max_y - y
        new_coords.append((int(round(x)), int(round(y))))
      s._coords = new_coords

