# -*- coding: utf-8 -*-
# Licensed under The MIT License
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


class _StrandBuilder(object):

  def __init__(self, min_x, max_x, min_y, max_y):
    self._min_x = min_x
    self._max_x = max_x
    self._range_x = max_x - min_x
    self._min_y = min_y
    self._max_y = max_y
    self._range_y = max_y - min_y
    self._enable_horizontal_mirror = False
    self._coords = []

  def enable_horizontal_mirror(self):
    self._enable_horizontal_mirror = True

  # Note that x/y here are [0, 1] that will be mapped to [min, max] range.
  def add_custom_coords(self, count, start_x, end_x, start_y, end_y):
    if self._enable_horizontal_mirror:
      start_x = 1.0 - start_x
      end_x = 1.0 - end_x
    start_x = float(start_x)
    end_x = float(end_x)
    start_y = float(start_y)
    end_y = float(end_y)
    step_x = (end_x - start_x) / count
    step_y = (end_y - start_y) / count
    for i in xrange(count):
      x = (start_x + step_x * i) * self._range_x + self._min_x
      y = (start_y + step_y * i) * self._range_y + self._min_y
      if x < self._min_x or x > self._max_x:
        raise Exception('x out of range: %s (%s / %s)' % (
            x, self._min_x, self._max_x))
      if y < self._min_y or y > self._max_y:
        raise Exception('y out of range: %s (%s / %s)' % (
            y, self._min_y, self._max_y))
      self._coords.append((int(x), int(y)))

  def add_horizontal(self, count, start_x, end_x, y):
    self.add_custom_coords(count, start_x, end_x, y, y)

  def get_coords(self):
    return list(self._coords)


class TclLayout(object):

  def __init__(self, file_path, max_x, max_y):
    self._strands = {}
    self._file_path = file_path
    self._max_x = max_x
    self._max_y = max_y

    dxf = dxfgrabber.readfile(file_path)
    self._parse(dxf)

    self._normalize()

    self._customize()

    self._print_info()

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

  def _print_info(self):
    print 'Layout "%s", dst_width=%s, dst_height=%s' % (
        self._file_path, self._max_x + 1, self._max_y + 1)
    for s in self._strands.values():
      min_x, max_x, min_y, max_y = self._find_min_max(s._coords)
      print '  Strand P%s, led_count=%s, x=(%s-%s), y=(%s-%s)' % (
          s._id + 1, len(s._coords), min_x, max_x, min_y, max_y)

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
    min_x, max_x, min_y, max_y = self._find_min_max(self._get_all_coords())
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

  def _get_all_coords(self):
    result = []
    for s in self._strands.values():
      result += s._coords
    return result

  def _find_min_max(self, coords):
    min_x = sys.float_info.max
    min_y = sys.float_info.max
    max_x = sys.float_info.min
    max_y = sys.float_info.min
    for c in coords:
      x, y = c[0], c[1]
      if x < min_x:
        min_x = x
      if x > max_x:
        max_x = x
      if y < min_y:
        min_y = y
      if y > max_y:
        max_y = y
    return (min_x, max_x, min_y, max_y)

  # The place of terrible hacks, because I have no time to mod the DXF.
  def _customize(self):
    if self._file_path != 'dfplayer/layout1.dxf':
      return
    self._make_new_tail(2, 6, True)
    self._make_new_tail(3, 7, False)

  def _make_new_tail(self, old_strand_id, new_strand_id, is_mirrored):
    if new_strand_id in self._strands:
      raise Exception('Strand already exists')

    old_cutoff_coord_id = 202
    old_coords = self._strands[old_strand_id]._coords
    min_x, max_x, min_y, max_y = self._find_min_max(
        old_coords[old_cutoff_coord_id:])
    del old_coords[old_cutoff_coord_id:]

    builder = _StrandBuilder(min_x, max_x, min_y, max_y)
    if is_mirrored:
      builder.enable_horizontal_mirror()
    y_step = 1.0 / 7.0
    width = 20.0  # In feet.
    builder.add_horizontal(61, 0, 20.0 / width, 0)
    builder.add_horizontal(50, 0, 20.0 / width, y_step)
    builder.add_horizontal(42, 0, 16.0 / width, y_step * 2)
    builder.add_horizontal(38, 0, 14.0 / width, y_step * 3)
    builder.add_horizontal(35, 0, 13.0 / width, y_step * 4)
    builder.add_horizontal(37, 0, 14.0 / width, y_step * 5)
    builder.add_horizontal(41, 0, 16.0 / width, y_step * 6)
    builder.add_horizontal(50, 0, 20.0 / width, y_step * 7)
    self._strands[new_strand_id] = Strand(new_strand_id)
    self._strands[new_strand_id]._coords = builder.get_coords()
