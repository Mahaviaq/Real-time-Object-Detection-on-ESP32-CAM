# 🎯 Object Detection on ESP32-CAM using Python and YOLO

## 📌 Overview
This project demonstrates a lightweight real-time object detection system using Python and OpenCV on a PC, combined with an ESP32-CAM IoT device. The ESP32-CAM captures images and sends them to the host system, where Python and a YOLO model detect and label objects.

## 🔧 Technologies Used
- 🐍 Python 3
- 📷 OpenCV (cv2)
- 🧠 YOLOv8 (or YOLOv5)
- 🤖 ESP32-CAM (Arduino IDE for flashing)
- 📡 Serial and Wi-Fi communication (we can use both for image transfer)

## 💡 How It Works
1. The ESP32-CAM captures an image either on boot or on-demand.
2. The image is transferred to the computer (via serial or Wi-Fi).
3. Python script processes the image using OpenCV and YOLO.
4. Detected objects are labeled and displayed or stored.

## 📦 Project Goals
- Enable low-cost IoT vision systems using ESP32-CAM.
- Perform object detection without cloud dependency.
- Explore the edge-AI potential of embedded + PC-based hybrid solutions.

## 🚀 Future Scope
- Automate alerts or actions based on detected objects.
- Integrate with a small display or speaker for real-time feedback.
- Expand to motion detection or face recognition.
