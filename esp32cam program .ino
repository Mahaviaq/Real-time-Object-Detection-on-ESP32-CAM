#include <WiFiProvisioner.h>
#include <WiFi.h>
#include <esp32cam.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <AudioFileSourceICYStream.h>

#include <SPIFFS.h>


#include "AudioFileSourceSPIFFS.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

AudioGeneratorWAV* wav;
AudioFileSourceSPIFFS* file;
AudioOutputI2S* i2sOutput;

bool audioPlaying = false;

const char* postUri = "/api/scan-qr";
const int port = 7209;
const char* server = "192.168.92.120";
// ------ Config ------
const char* ssid = "Smart voice system";
const char* password = "44332211@";



#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WiFiClient espClient;

bool wifiConnected = false;
esp32cam::Resolution initialResolution;
WiFiProvisioner provisioner;

void ButtonMonitorTask(void* param) {
  Serial.println("📌 ButtoMonitorTask started.");
  while (true) {
    int irValue = digitalRead(2);

    if (wifiConnected && irValue == HIGH && !audioPlaying) {
      Serial.println("Sending image...");
      displayFitText("Sending image...");
      digitalWrite(4, HIGH);
      delay(10);
      sendImage();
      digitalWrite(4, LOW);
      vTaskDelay(pdMS_TO_TICKS(5000));
    } else {
      delay(10);
      // No vehicle detected, check again in 3 seconds
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
}

void displayFitText(String message) {
  int16_t x1, y1;
  uint16_t w, h;
  uint8_t textSize = 2;  // Start with larger size

  // Try to find a text size that fits the screen width
  while (textSize > 0) {
    display.setTextSize(textSize);
    display.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);
    if (w <= SCREEN_WIDTH) {
      break;
    }
    textSize--;  // Reduce size if too wide
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);  // Centered
  display.print(message);
  display.display();

  vTaskDelay(pdMS_TO_TICKS(3000));  // Show for 1 second
}


void drawCenteredText(const String& line1, const String& line2 = "", int textSize = 1, int delayTime = 1000) {
  display.clearDisplay();
  display.setTextSize(textSize);
  display.setTextColor(WHITE);

  int16_t x1, y1;
  uint16_t w, h;

  if (line1 != "") {
    display.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 10);
    display.println(line1);
  }

  if (line2 != "") {
    display.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 30);
    display.println(line2);
  }

  display.display();
  vTaskDelay(pdMS_TO_TICKS(delayTime));
}

// Final countdown screen
void showCountdown() {
  drawCenteredText("Starting", "System...", 1, 600);

  for (int i = 3; i >= 1; i--) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);

    String countText = String(i);
    int16_t x1, y1;
    uint16_t w, h;

    display.getTextBounds(countText, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, (64 - h) / 2);
    display.println(countText);
    display.display();
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  drawCenteredText("System", "Started!", 1, 1000);
}

void displayScrollingText(String message) {
  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(2);  // Large font
  display.setTextColor(SSD1306_WHITE);
  display.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);

  int scrollX = SCREEN_WIDTH;

  while (scrollX > -w) {
    display.clearDisplay();
    display.setCursor(scrollX, (SCREEN_HEIGHT - h) / 2);  // Vertically center
    display.print(message);
    display.display();
    scrollX -= 2;  // Scroll speed
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

bool resetCamera() {
  Serial.println("🔄 Resetting camera...");
  using namespace esp32cam;

  Camera.end();   // Deinitialize and free resources
  delay(100);

  // Reinitialize
  Config cfg;
  cfg.setPins(pins::AiThinker);
  cfg.setResolution(Resolution::find(1024, 768));
  cfg.setBufferCount(2);
  cfg.setJpeg(80);

  bool ok = Camera.begin(cfg);
  if (ok) {
    Serial.println("✅ Camera reinitialized successfully.");
  } else {
    Serial.println("❌ Camera reinit failed.");
  }
  return ok;
}

// ------ Camera Init ------
bool initCamera() {
  Serial.println("📷 Initializing camera...");
  using namespace esp32cam;
  Config cfg;
  cfg.setPins(pins::AiThinker);
  cfg.setResolution(Resolution::find(1024, 768));
  cfg.setBufferCount(2);
  cfg.setJpeg(80);

  bool ok = Camera.begin(cfg);
  if (ok) {
    Serial.println("✅ Camera initialized successfully.");
  } else {
    Serial.println("❌ Camera initialization failed.");
  }
  return ok;
}

// ------ Send Image ------
void playWAVFromSPIFFS() {
  audioPlaying = true;
  
  Serial.println("✅ Initializing I2S");
  digitalWrite(4, LOW);
  Serial.printf("📦 Free heap before: %d\n", ESP.getFreeHeap());
   
   AudioFileSourceSPIFFS file("/response.wav");
   AudioOutputI2S i2sOutput;
   AudioGeneratorWAV wav;
   file.seek(0, SeekSet);
  i2sOutput.SetPinout(12, 13, 0); // LRCK = 13, BCLK = 12, DIN = 0
  i2sOutput.SetGain(0.6);

  Serial.println("✅ Playing WAV");
  delay(10);

  if (!wav.begin(&file, &i2sOutput)) {
    Serial.println("❌ Failed to start WAV playback");
    audioPlaying = false;
    return;
  }

  while (wav.isRunning()) {
    if (!wav.loop()) {
      wav.stop();
      break;
    }
    vTaskDelay(1);
  }

  Serial.println("✅ WAV playback done");

  //SPIFFS.remove("/response.wav");
  delay(20);
  //Serial.println("🗑 Deleted /response.wav");

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Ready");
  display.display();
  delay(10);
  audioPlaying = false;

}



void sendImage() {
  if (!WiFi.isConnected()) return;

  delay(10);

  vTaskDelay(pdMS_TO_TICKS(100));
  // initCamera();
  digitalWrite(4, LOW);
  auto frame = esp32cam::capture();
  vTaskDelay(pdMS_TO_TICKS(1000));
  if (!frame) {
  Serial.println("❌ Failed to capture image, will reset camera");
  displayFitText("Capture failed");
  if (resetCamera()) {
    Serial.println("🧹 Retrying capture...");
    frame = esp32cam::capture();  // Try again
    if (!frame) {
      Serial.println("❌ Retry also failed.");
      displayFitText("Camera failed restart");
      return;
    }
  } else {
    Serial.println("❌ Reset failed, aborting.");
    displayFitText("Camera failed restart");
    return;
  }
}
  vTaskDelay(pdMS_TO_TICKS(1000));
  Serial.print("Captured image size: ");
  Serial.println(frame->size());
  delay(10);
  WiFiClient client;
  client.setTimeout(10000);  // ⏱ Set 10s timeout to prevent premature stop
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String contentType = "multipart/form-data; boundary=" + boundary;

  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"image\"; filename=\"image.jpg\"\r\n";
  bodyStart += "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";

  int contentLength = bodyStart.length() + frame->size() + bodyEnd.length();

  Serial.println("Connecting to server...");
  if (!client.connect(server, port)) {
    Serial.println("Connection to server failed");
    displayFitText("Connection to server failed : Rebooting");
    delay(3000);
    ESP.restart();
    return;
  }

  // Send HTTP headers
  client.print(String("POST ") + postUri + " HTTP/1.1\r\n");
  client.print(String("Host: ") + server + "\r\n");
  client.print("Content-Type: " + contentType + "\r\n");
  client.print("Content-Length: " + String(contentLength) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  // Send body
  client.print(bodyStart);
  client.write(frame->data(), frame->size());
  delay(10);
  client.print(bodyEnd);
  displayFitText("Image Send");
  // --- Skip HTTP response headers ---
  Serial.println("⏳ Waiting for response headers...");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;  // End of headers
    Serial.println("[Header] " + line);
  }

  // --- Prepare SPIFFS file to store MP3 ---
  File audioFile = SPIFFS.open("/response.wav", FILE_WRITE);
  if (!audioFile) {
    Serial.println("❌ Failed to open /response.wav for writing");
    client.stop();
    return;
  }

  Serial.println("💾 Receiving MP3 file from server...");
  displayFitText("Downloading Audio");
  const size_t bufferSize = 512;
  uint8_t buffer[bufferSize];
  size_t totalBytes = 0;

  // --- Read MP3 data from response ---
  while (client.connected() || client.available()) {
    size_t len = client.read(buffer, bufferSize);
    if (len > 0) {
      audioFile.write(buffer, len);
      totalBytes += len;
    }
  }

  audioFile.close();
  client.stop();
  frame.release();
  frame = nullptr;

  Serial.printf("✅ MP3 file downloaded: %d bytes (%.2f KB)\n", totalBytes, totalBytes / 1024.0);

  // --- Confirm file size directly from SPIFFS ---
  File checkFile = SPIFFS.open("/response.wav", FILE_READ);
  if (checkFile) {
    Serial.printf("🔍 SPIFFS file size: %d bytes\n", checkFile.size());
    checkFile.close();
  }

  displayFitText("Audio ready");
  vTaskDelay(pdMS_TO_TICKS(1000));
  delay(10);
  playWAVFromSPIFFS();  // ✅ Play the downloaded MP3 file
}
void setup() {

  Serial.begin(115200);
  Serial.println("🚀 Booting ESP32-CAM...");
  delay(1000);

  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS Mount Failed");
    displayFitText("SPIFFS failed!");
    while (true)
      ;
  }
  /*
out = new AudioOutputI2S(1, AudioOutputI2S::EXTERNAL_I2S);
//out = new AudioOutputI2S();
out->SetPinout(13, 12, 16);  // LRCK = 13, BCLK = 12, DIN = 27
out->SetGain(0.6);
mp3 = new AudioGeneratorMP3();

if (!out->begin()) {
  Serial.println("❌ Failed to start I2S output");
  delay(3000);
}*/
  // Start I2C for OLED
  Wire.begin(14, 15);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true)
      ;
  }
  delay(100);
  display.clearDisplay();
  display.display();
  displayFitText("initCamera...");
  // Init Camera
  if (!initCamera()) {
    Serial.println("⚠ Restarting due to camera init failure...");
    delay(1000);
    ESP.restart();
  }
  displayFitText("WiFi Connecting ..");
  /*
  // Configure WiFiProvisioner
   provisioner.getConfig().AP_NAME = "Smart Vision System";
  provisioner.getConfig().SHOW_INPUT_FIELD = true;
  provisioner.getConfig().SHOW_RESET_FIELD = false;
  provisioner.getConfig().INPUT_LENGTH = 15;
  provisioner.getConfig().INPUT_TEXT ="Server Address";

  provisioner.onSuccess([](const char *ssid, const char *password, const char *input) {
    Serial.printf("Connected to SSID: %s\n", ssid);
    if (input) {
      server= input;
     
    }
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
  });

  provisioner.startProvisioning();
  */

  // WiFi
  Serial.printf("📶 Connecting to WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }
  wifiConnected = WiFi.status() == WL_CONNECTED;
  if (wifiConnected) {
    displayFitText("WiFi connected!");
    Serial.print("🌐 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("WiFi connection failed! ");
    displayFitText("WiFi connection failed!");
  }

  // IR and Flash Pin
  pinMode(4, OUTPUT);
  pinMode(2, INPUT);
  digitalWrite(4, LOW);

  displayFitText("Provisioning...");
  showCountdown();
  xTaskCreatePinnedToCore(ButtonMonitorTask, "IR Task", 4096, NULL, 1, NULL, 0);
}

// ------ Tasks ------


void loop() {
  //provisioner.handle();  // ⬅ Required to keep provisioning active

  if (ESP.getFreeHeap() < 15000) {
    displayFitText("Heap critically low! Restarting...");
    Serial.println("⚠ Heap critically low! Restarting...");
    delay(5000);
      ESP.restart();
  }
  delay(100);  // Prevent WDT reset
  vTaskDelay(pdMS_TO_TICKS(1000));
}