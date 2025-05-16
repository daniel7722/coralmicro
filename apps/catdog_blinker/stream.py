# viewer.py
import socket
import struct
import io
from PIL import Image, ImageTk
import tkinter as tk

HOST = '10.10.10.1'  # Coral Dev Board Micro default IP over USB
PORT = 27000

MSG_TYPE_SETUP = 0
MSG_TYPE_IMAGE_DATA = 1

def recv_all(sock, size):
    buf = b''
    while len(buf) < size:
        data = sock.recv(size - len(buf))
        if not data:
            return None
        buf += data
    return buf

def recv_msg(sock):
    header = recv_all(sock, 5)
    if not header:
        return None, None
    msg_size, msg_type = struct.unpack('<IB', header)
    body = recv_all(sock, msg_size)
    return msg_type, body

class ImageApp:
    def __init__(self, root, width, height):
        self.canvas = tk.Canvas(root, width=width, height=height)
        self.canvas.pack()
        self.image_on_canvas = None

    def update_image(self, image):
        self.tk_image = ImageTk.PhotoImage(image)
        if self.image_on_canvas is None:
            self.image_on_canvas = self.canvas.create_image(0, 0, anchor='nw', image=self.tk_image)
        else:
            self.canvas.itemconfig(self.image_on_canvas, image=self.tk_image)

def main():
    with socket.socket() as sock:
        print(f"🔌 Connecting to {HOST}:{PORT}")
        sock.connect((HOST, PORT))
        print("✅ Connected")

        msg_type, body = recv_msg(sock)
        if msg_type != MSG_TYPE_SETUP:
            print("Expected setup message, got", msg_type)
            return

        width, height = struct.unpack('<HH', body)
        print(f"📷 Image size: {width}x{height}")

        root = tk.Tk()
        app = ImageApp(root, width, height)
        root.title("Coral Dev Board Camera Viewer")

        def poll():
            msg_type, body = recv_msg(sock)
            if msg_type == MSG_TYPE_IMAGE_DATA and body:
                image = Image.open(io.BytesIO(body))
                app.update_image(image)
            root.after(50, poll)

        poll()
        root.mainloop()

if __name__ == "__main__":
    main()
