import time
import cv2
import numpy as np
import pyperclip
import pyautogui
from pyzbar.pyzbar import decode


def process_image(img):
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
    colonIdx = result.find(':')
    data = result[colonIdx + 1:] if colonIdx >= 0 else result
    pipeIdx = data.find('|')
    if pipeIdx >= 0:
        data = data[:pipeIdx]
    return data


def paste_to_focused_input(text):
    pyperclip.copy(text)
    time.sleep(0.1)
    pyautogui.hotkey('ctrl', 'v')
    print(f"Pasted: {text}")


CAMERA_INDEX = 1        # Change if your webcam isn't device 0
SCAN_COOLDOWN = 2.0     # Seconds to wait before scanning the same code again

cap = cv2.VideoCapture(CAMERA_INDEX)
if not cap.isOpened():
    raise RuntimeError(f"Could not open webcam at index {CAMERA_INDEX}")

print("Webcam started. Press 'q' to quit.")

last_pasted = None
last_paste_time = 0.0

while True:
    ret, frame = cap.read()
    if not ret:
        print("Failed to grab frame")
        break

    result = process_image(frame)

    if result != "NONE" and not result.startswith("ERROR"):
        data = extract_data(result)
        now = time.time()

        # Avoid spamming the same code repeatedly
        if data != last_pasted or (now - last_paste_time) > SCAN_COOLDOWN:
            paste_to_focused_input(data)
            last_pasted = data
            last_paste_time = now

    cv2.imshow("QR Scanner", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()