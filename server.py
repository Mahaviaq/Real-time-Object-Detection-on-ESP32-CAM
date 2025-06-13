import subprocess
from flask import Flask, request, send_file
from ultralytics import YOLO
from gtts import gTTS
from PIL import Image
import tempfile
import cv2
import numpy as np
import io
import os


app = Flask(__name__)
model = YOLO("yolov8n.pt")

indoor_classes = [
    'person', 'chair', 'couch', 'potted plant', 'dining table', 'tv', 'laptop', 'book',
    'cell phone', 'keyboard', 'mouse', 'remote', 'microwave', 'oven', 'toaster', 'sink',
    'refrigerator', 'bed', 'clock', 'vase', 'scissors', 'teddy bear', 'hair drier',
    'toothbrush', 'lamp', 'bottle', 'cup', 'bowl', 'banana', 'apple', 'cabinet'
]

def estimate_distance(box, image_width):
    f = 615
    W = 30
    box_width_pixels = box[2] - box[0]
    return round((W * f) / box_width_pixels, 2) if box_width_pixels else 0.0

@app.route('/api/scan-qr', methods=['POST'])
def scan_qr():
    file = request.files['image']
    image_bytes = file.read()

    # Save image as 'incoming.png' (overwrite each time)
    with open("incoming.png", "wb") as f:
        f.write(image_bytes)

    # Decode for inference
    np_img = np.frombuffer(image_bytes, np.uint8)
    img = cv2.imdecode(np_img, cv2.IMREAD_COLOR)

    results = model(img, conf=0.25)

    names = results[0].names
    classes = results[0].boxes.cls.cpu().numpy()
    coords = results[0].boxes.xyxy.cpu().numpy()

    detected_info = []

    for cls_id, box in zip(classes, coords):
        label = names[int(cls_id)]
        if label in indoor_classes:
            distance = estimate_distance(box, img.shape[1])
            detected_info.append(f"{label} at {distance:.2f} cm")

    sentence = "I see " + ", ".join(detected_info) if detected_info else "No indoor objects detected"
    print(f"[INFO] Response Sentence: {sentence}")

    # Generate and save TTS output to 'response.mp3' (overwrite)
     # Step 1: Generate MP3 with gTTS
   # Generate MP3 with gTTS
    tts = gTTS(sentence)
    mp3_path = "response.mp3"
    wav_path = "response.wav"

    tts.save(mp3_path)

    # Convert MP3 to WAV using ffmpeg
    subprocess.run([
        "ffmpeg", "-y", "-i", mp3_path,
        "-ac", "1", "-ar", "16000", "-sample_fmt", "s16", wav_path
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    # Return WAV to ESP32
    return send_file(wav_path, mimetype='audio/wav')


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=7209)
