# -*- coding: utf-8 -*-
#
# Controls TCL controller.

import binascii
import math
import socket
import time

from .tcl_layout import TclLayout

_MSG_INIT = bytearray([0xC5, 0x77, 0x88, 0x00, 0x00])
_MSG_INIT_DELAY = 0.1
_MSG_START_FRAME = bytearray([0xC5, 0x77, 0x88, 0x00, 0x00])
_MSG_START_FRAME_DELAY = 0.0005
_MSG_END_FRAME = bytearray([0xAA, 0x01, 0x8C, 0x01, 0x55])
_MSG_RESET = bytearray([0xC2, 0x77, 0x88, 0x00, 0x00])
_MSG_RESET_DELAY = 5
_MSG_REPLY = bytearray([0x55, 0x00, 0x00, 0x00, 0x00])

_FRAME_MSG_PREFIX = bytearray([0x88, 0x00, 0x68, 0x3F, 0x2B, 0xFD,
                               0x60, 0x8B, 0x95, 0xEF, 0x04, 0x69])
_FRAME_MSG_SUFFIX = bytearray([0x00, 0x00, 0x00, 0x00])
_FRAME_MSG_SIZE = 1024
_FRAME_MSG_DELAY = 0.0015
_FRAME_BLACK_COLOR = 0x2C

_STRAND_LENGTH = 512

_STRAND_BIT_MASK = bytearray([0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80])


class TclRenderer(object):
  """To use this class:
       renderer = TclRenderer(controller_id)
       renderer.set_layout('layout.dxf', width, height)
       # 'image_colors' has tuples with 3 RGB bytes for each pixel,
       # with 'height' number of sequential rows, each row having
       # length of 'width' pixels.
       renderer.send_frame(image_color)
  """

  def __init__(self, controller_id):
    self._controller_id = controller_id
    self._init_sent = False
    self.set_gamma(2.4)
    self._connect()

  def set_layout(self, layout_file, width, height):
    self._width = width
    self._height = height
    self._layout = TclLayout(layout_file, width - 1, height - 1)

  def get_layout_coords(self):
    return self._layout.get_all_coords()

  def set_gamma(self, gamma):
    # 1.0 is uncorrected gamma, which is perceived as "too bright"
    # in the middle. 2.4 is a good starting point. Changing this value
    # affects mid-range pixels - higher values produce dimmer pixels.
    self.set_gamma_ranges((0, 255, gamma), (0, 255, gamma), (0, 255, gamma))

  def set_gamma_ranges(self, r, g, b):
    if (r[0] < 0 or r[1] > 255 or r[2] <= 0 or
        g[0] < 0 or g[1] > 255 or g[2] <= 0 or
        b[0] < 0 or b[1] > 255 or b[2] <= 0):
      print 'Incorrect gamma values %s, %s, %s' % (r, g, b)
      return
    self._gamma_r = []
    self._gamma_g = []
    self._gamma_b = []
    for c in xrange(0, 256):
      d = float(c) / 255.0
      self._gamma_r.append(
          int(r[0] + math.floor((r[1] - r[0]) * math.pow(d, r[2]) + 0.5)))
      self._gamma_g.append(
          int(g[0] + math.floor((g[1] - g[0]) * math.pow(d, g[2]) + 0.5)))
      self._gamma_b.append(
          int(b[0] + math.floor((b[1] - b[0]) * math.pow(d, b[2]) + 0.5)))

  def _connect(self):
    self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    self._sock.setblocking(0)
    self._sock.bind(('', 0))

    target_ip = ('192.168.60.%s' % (49 + self._controller_id))
    self._sock.connect((target_ip, 5000))

  def _init_controller(self):
    # TODO(igorc): Re-init if there's still no reply data in several seconds.
    if self._init_sent:
      return
    self._init_sent = True
    # TODO(igorc): Look for a better place for reset.
    self._sock.send(_MSG_RESET)
    time.sleep(_MSG_RESET_DELAY)
    self._sock.send(_MSG_INIT)
    time.sleep(_MSG_INIT_DELAY)

  def send_frame(self, image_colors):
    self._init_controller()

    frame_data = self._convert_image_to_frame_data(image_colors)

    self._consume_reply_data()
    self._sock.send(_MSG_START_FRAME)
    time.sleep(_MSG_START_FRAME_DELAY)

    message_idx = 0
    frame_data_pos = 0
    while frame_data_pos < len(frame_data):
      frame_data_segment = frame_data[frame_data_pos :
                                      frame_data_pos + _FRAME_MSG_SIZE]
      frame_msg = bytearray(_FRAME_MSG_PREFIX)
      frame_msg.extend(frame_data_segment)
      frame_msg.extend(_FRAME_MSG_SUFFIX)
      frame_msg[1] = message_idx
      # print '3 = %s %s %s' % (frame_data_pos, message_idx, len(frame_msg))
      # if message_idx == 0:
      #   print 'msg %s' % binascii.hexlify(frame_msg[:64])
      frame_data_pos += _FRAME_MSG_SIZE
      message_idx += 1
      self._sock.send(frame_msg)
      time.sleep(_FRAME_MSG_DELAY)

    self._sock.send(_MSG_END_FRAME)
    self._consume_reply_data()

  def _consume_reply_data(self):
    has_reply_data = True
    while has_reply_data:
      try:
        reply_data = self._sock.recv(65536)
        # print 'Reply data = %s' % binascii.hexlify(reply_data)
      except socket.error:
        #TODO(igorc): Check error code
        has_reply_data = False

  def _convert_image_to_frame_data(self, image_colors):
    strands_colors = self._convert_image_to_strands(image_colors)
    #print 's', strands_colors
    return self._convert_strands_to_frame_data(strands_colors)

  def _convert_image_to_strands(self, image_colors):
    # TODO(igorc): Add gamut correction.
    expected_pixels = self._width * self._height
    if len(image_colors) != expected_pixels:
      raise Exception('Unexpected image size of %d', len(image_colors))
    strands_colors = []
    for strand_id in xrange(0, 8):
      current_colors = []
      strand_coords = self._layout.get_strand_coords(strand_id)
      if not strand_coords:
        continue
      for pos in xrange(0, min(len(strand_coords), _STRAND_LENGTH)):
        # color_idx = (strand_id * _STRAND_LENGTH + pos) % expected_pixels
        coord = strand_coords[pos]
        color_idx = coord[1] * self._width + coord[0]
        current_colors.append([
            self._gamma_r[image_colors[color_idx][0]],
            self._gamma_g[image_colors[color_idx][1]],
            self._gamma_b[image_colors[color_idx][2]]])
      strands_colors.append(current_colors)
    return strands_colors

  def _convert_strands_to_frame_data(self, strands_colors):
    result = bytearray()
    for led_id in xrange(0, _STRAND_LENGTH):
      result.extend(self._build_frame_color_seq(strands_colors, led_id, 2))
      result.extend(self._build_frame_color_seq(strands_colors, led_id, 1))
      result.extend(self._build_frame_color_seq(strands_colors, led_id, 0))
    for i in xrange(0, len(result)):
      result[i] = (result[i] + _FRAME_BLACK_COLOR) & 0xFF
    return result

  def _build_frame_color_seq(self, strands_colors, led_id, color_component):
    result = bytearray()
    color_bit_mask = 0x80
    while color_bit_mask > 0:
      current_byte = 0
      for strand_id in xrange(0, len(strands_colors)):
        strand_colors = strands_colors[strand_id]
        if led_id < len(strand_colors):
          color = strand_colors[led_id][color_component]
          if (color & color_bit_mask) != 0:
            current_byte |= _STRAND_BIT_MASK[strand_id]
      color_bit_mask /= 2
      result.append(current_byte)
    return result

