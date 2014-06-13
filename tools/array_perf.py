#!/usr/bin/python
#

import array
import sys
import time

def create_normal_array(size):
  arr = []
  for i in range(0, size):
    arr.append(i)
  return arr

def create_byte_array(size):
  arr = array.array('B')
  for i in range(0, size):
    arr.append(i & 0xFF)
  return arr

def write_array(arr):
  for i in range(0, len(arr)):
    arr[i] = i & 0xFF

def read_array(arr):
  sum = 0
  for i in range(0, len(arr)):
    sum += arr[i]
  if sum == -1:
    print 'Sum is -1, please ignore this fact'

def get_time():
  return int(round(time.time() * 1000))

def get_rate(duration, size):
  return '%sMB/s' % (size / 1000 / duration)

def test_arr(name, create_func, size):
  prev_time = get_time()
  arr = create_func(size)
  create_duration = get_time() - prev_time

  prev_time = get_time()
  write_array(arr)
  write_duration = get_time() - prev_time

  prev_time = get_time()
  read_array(arr)
  read_duration = get_time() - prev_time

  print 'Stats for %s: count=%s, size=%s, create=%s, write=%s, read=%s' % (
      name, len(arr), sys.getsizeof(arr),
      get_rate(create_duration, size),
      get_rate(write_duration, size),
      get_rate(read_duration, size))


test_arr('Regular', create_normal_array, 10 * 1000 * 1000)
test_arr('ByteArray', create_byte_array, 10 * 1000 * 1000)

