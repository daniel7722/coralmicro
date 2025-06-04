#!/usr/bin/python3
# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import collections
import contextlib
import enum
import io
import json
import queue
import socket
import struct
import threading
import time
import tkinter as tk

from PIL import ImageTk, Image

MSG_TYPE_SETUP = 0
MSG_TYPE_IMAGE_DATA = 1


def recvall(sock, size):
  data = bytearray()
  while size != 0:
    chunk = sock.recv(size)
    if not chunk:
      return b''
    data += chunk
    size -= len(chunk)
  return bytes(data)


Message = collections.namedtuple('Message', ('type', 'body'))


def recv_msg(sock):
  header = recvall(sock, size=5)
  if not header:
    return None

  msg_size, msg_type = struct.unpack('<IB', header)
  msg_body = recvall(sock, msg_size)
  if not msg_body:
    return None

  return Message(msg_type, msg_body)


class Fps:
  def __init__(self, avg_size):
    self._avg_size = avg_size
    self._acc = []
    self._prev = time.monotonic()

  def update(self):
    curr = time.monotonic()
    self._acc.append(curr - self._prev)
    if len(self._acc) > self._avg_size:
      self._acc.pop(0)
    fps = len(self._acc) / sum(self._acc)
    self._prev = curr
    return fps


@contextlib.contextmanager
def Fetcher(read_msg, write_data):
  def run():
    try:
      while not done.is_set():
        msg = read_msg()
        if msg is None:
          break

        if msg.type == MSG_TYPE_IMAGE_DATA:
          if not write_data(Image.open(io.BytesIO(msg.body))):
            break

    except Exception as e:
      pass

  done = threading.Event()
  thread = threading.Thread(target=run)
  thread.start()
  try:
    yield
  finally:
    done.set()
    thread.join()


class PoseCanvas(tk.Frame):
  def _update_image(self, width, height):
    w, h = self._image.size
    sx = width / w
    sy = height / h
    size = int(sx * w), int(sy * h)

    image = self._image
    if self.flip_x:
      image = image.transpose(Image.FLIP_LEFT_RIGHT)
    if self.flip_y:
      image = image.transpose(Image.FLIP_TOP_BOTTOM)
    self._tk_image = ImageTk.PhotoImage(image=image.resize(size))
    self._canvas.itemconfigure(self._image_id, image=self._tk_image)

  def _configure(self, event):
    r = min(event.width / self._width, event.height / self._height)
    w, h = self._width * r, self._height * r
    x = (event.width - w) / 2
    y = (event.height - h) / 2
    self._canvas.place(x=x, y=y, width=w, height=h)

  def _canvas_configure(self, event):
    sx = event.width / self._canvas_width
    sy = event.height / self._canvas_height
    self._canvas_width = event.width
    self._canvas_height = event.height
    self._canvas.scale('all', 0, 0, sx, sy)
    self._update_image(event.width, event.height)

  def __init__(self, parent, width, height):
    super().__init__(parent, bg='black')

    self.flip_x = False
    self.flip_y = False

    self._image = Image.new('RGB', (width, height), color='black')
    self._tk_image = ImageTk.PhotoImage(image=self._image)

    self._width = width
    self._height = height
    self._canvas_width = width
    self._canvas_height = height

    self._canvas = tk.Canvas(self, bd=0, highlightthickness=0, bg='black')
    self._image_id = self._canvas.create_image(0, 0, anchor='nw',
                                               image=self._tk_image)
    self._canvas.place(x=0, y=0, width=width, height=height)
    self._canvas.bind('<Configure>', self._canvas_configure)
    self.bind('<Configure>', self._configure)

  def update_image(self, image): # called when TYPE_IMAGE_DATA is sent
    self._image = image
    self._update_image(self._canvas_width, self._canvas_height)



def create_tk(width, height, fullscreen=False, do_stop=lambda: None):
  root = tk.Tk()
  root.attributes('-fullscreen', fullscreen)
  root.title("Dev Board Micro Cat Dog Blinker")
  root.geometry(f'{width}x{height}')
  root.columnconfigure(0, weight=1)
  root.rowconfigure(0, weight=1)

  c = PoseCanvas(root, width, height)
  c.grid(column=0, row=0, sticky='nwes')

  fps = Fps(avg_size=25)

  data_queue = queue.Queue()
  done = threading.Event()

  def update_async(data):
    try:
      if done.is_set():
        return False

      data_queue.put(data)
      root.event_generate('<<NewData>>', when='tail')
      return True
    except Exception as e:
      return False

  def on_new_data(event):
    try:
      root.winfo_exists()
    except Exception:
      return

    data = data_queue.get_nowait()
    if isinstance(data, Image.Image):
      c.update_image(data)
      print('FPS: %.2f' % fps.update())

  root.bind('<<NewData>>', on_new_data)

  def close_window():
    do_stop()
    done.set()
    root.destroy()

  def on_key(event):
    ch = event.char.lower()

    if ch == 'f':
      root.attributes('-fullscreen', not root.attributes('-fullscreen'))
    elif ch == 'x':
      c.flip_x = not c.flip_x
    elif ch == 'y':
      c.flip_y = not c.flip_y
    elif ch == 'q':
      close_window()
  root.bind('<Key>', on_key)

  def on_esc(self):
    if root.attributes('-fullscreen'):
      root.attributes('-fullscreen', False)
  root.bind('<Escape>', on_esc)

  root.protocol('WM_DELETE_WINDOW', close_window)

  return root, update_async


@contextlib.contextmanager
def SocketClient(host, port):
  sock = socket.socket()
  print(f'Client connecting to {host}:{port}...')
  sock.connect((host, port))
  print('Client connected.')

  try:
    yield sock
  finally:
    sock.send(bytes([0]))
    sock.shutdown(socket.SHUT_RDWR)
    sock.close()
    print('Client closed.')


def main():
  parser = argparse.ArgumentParser(description='Cat Dog Blinker UI',
                                   formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('--host', '--device_ip_address',
                      type=str, default='10.10.10.1')
  parser.add_argument('--port', type=int, default=27000)
  parser.add_argument('--flip_x', '--mirror', action='store_true')
  parser.add_argument('--flip_y', action='store_true')
  parser.add_argument('--fullscreen', action='store_true')
  args = parser.parse_args()

  with SocketClient(args.host, args.port) as sock:
    msg = recv_msg(sock)
    if msg is None:
      return
    assert(msg.type == MSG_TYPE_SETUP)
    width, height = struct.unpack('<ii', msg.body)

    root, update = create_tk(width, height, args.fullscreen)
    with Fetcher(lambda: recv_msg(sock), update):
      root.mainloop()


if __name__ == '__main__':
  main()
