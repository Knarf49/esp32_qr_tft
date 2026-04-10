import socket
import struct
import time
import cv2
import numpy as np
import pyperclip
import pyautogui
from pyzbar.pyzbar import decode

HOST = "0.0.0.0"
PORT = 5000

def process_image(jpeg_bytes):
    np_arr = np.frombuffer(jpeg_bytes, dtype=np.uint8)
    img = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
    if img is None:
        return "ERROR: could not decode image"

    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    detected = decode(gray)

    if not detected:
        return "NONE"

    results = []
    for obj in detected:
        data = obj.data.decode("utf-8")
        kind = obj.type
        results.append(f"{kind}:{data}")

    return "|".join(results)


def extract_data(result):
    # Strip "QRCODE:" prefix, keep only actual data
    colonIdx = result.find(':')
    data = result[colonIdx + 1:] if colonIdx >= 0 else result
    # If multiple codes, take first
    pipeIdx = data.find('|')
    if pipeIdx >= 0:
        data = data[:pipeIdx]
    return data


def paste_to_focused_input(text):
    pyperclip.copy(text)               # set clipboard
    time.sleep(0.1)                    # small delay to ensure clipboard is set
    pyautogui.hotkey('ctrl', 'v')      # simulate Ctrl+V paste
    print(f"Pasted: {text}")


def recv_exact(conn, n):
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("Client disconnected")
        buf += chunk
    return buf


server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
server.bind((HOST, PORT))
server.listen(1)
print(f"Listening on {PORT}...")

conn, addr = server.accept()
conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
print(f"Connected: {addr}")

while True:
    try:
        raw_len = recv_exact(conn, 4)
        img_len = struct.unpack(">I", raw_len)[0]
        jpeg_bytes = recv_exact(conn, img_len)

        result = process_image(jpeg_bytes)
        print(f"Scan result: {result}")

        # Paste only if something found
        if result != "NONE" and not result.startswith("ERROR"):
            data = extract_data(result)
            paste_to_focused_input(data)

        # Send result back to ESP32
        encoded = result.encode("utf-8")
        conn.sendall(struct.pack(">I", len(encoded)) + encoded)

    except ConnectionError:
        print("Client disconnected")
        break

conn.close()
server.close()