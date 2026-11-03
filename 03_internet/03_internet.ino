/*
 * Bai thuc hanh — kenh Internet (MQTT)
 * Nen: otto_motion (TFT + 4 servo, cung pin — khong thao day)
 *
 * ESP32 join Wi-Fi CO INTERNET (xem wifi_secrets.h).
 * Broker: broker.hivemq.com:1883
 *   subscribe  otto/hale/cmd     <- publish: happy | forward | left | ...
 *   publish    otto/hale/status  <- JSON ok/face
 *
 * Demo dung yeu cau: phone dung mang KHAC (4G) publish len topic cmd.
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <Grobot_Animations.h>
#include <Otto.h>
#include "wifi_secrets.h"

static const char *MQTT_HOST = "broker.hivemq.com";
static const uint16_t MQTT_PORT = 1883;
static const char *TOPIC_CMD = "otto/hale/cmd";
static const char *TOPIC_STATUS = "otto/hale/status";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Otto Otto;
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);
GrobotEyes eyes(0x97E0, TFT_BLACK);

#define LeftLeg 14
#define RightLeg 13
#define LeftFoot 26
#define RightFoot 16
#define BuzzerPin 15
#define FOOT_H 30

const MoodData MOOD_LISTEN = {16, 0, -28, 30, 46};
const MoodData MOOD_THINK  = {10, 0, 18, 14, 42};
const MoodData MOOD_SPEAK  = {0, 22, 0, 38, 52};
const MoodData MOOD_SQUINT = {28, 8, 12, 16, 40};
const MoodData MOOD_ANNOY_L = {6, 0, 40, 18, 42};
const MoodData MOOD_ANNOY_R = {6, 0, 18, 18, 42};

struct Face {
  const char *key;   // PROTOCOL emotion or motion alias
  const char *label; // TFT label
  MoodData left;
  MoodData right;
  int lookX;
  int lookY;
  uint16_t color;
  uint8_t blush;
  uint8_t mouth;
};

// PROTOCOL led.state + alias motion → mat TFT
static const Face faces[] = {
  {"idle",      "IDLE",   MOOD_NEUTRAL, MOOD_NEUTRAL, 0, 0,    0x4C9F, 0, 0},
  {"listening", "LISTEN", MOOD_LISTEN,  MOOD_LISTEN,  0, -16,  0xFE60, 1, 0},
  {"thinking",  "THINK",  MOOD_THINK,   MOOD_SQUINT,  16, -10, 0xA11F, 0, 4},
  {"think",     "THINK",  MOOD_THINK,   MOOD_SQUINT,  16, -10, 0xA11F, 0, 4},
  {"speaking",  "SPEAK",  MOOD_SPEAK,   MOOD_SPEAK,   0, 4,    0x07E0, 1, 3},
  {"nod",       "SPEAK",  MOOD_SPEAK,   MOOD_SPEAK,   0, 4,    0x07E0, 1, 3},
  {"happy",     "HAPPY",  MOOD_HAPPY,   MOOD_HAPPY,   0, 6,    0xFEA0, 2, 1},
  {"alert",     "ALERT",  MOOD_ANGRY,   MOOD_ANGRY,   0, 0,    0xF800, 1, 4},
  {"shake",     "ANNOY",  MOOD_ANNOY_L, MOOD_ANNOY_R, 8, 0,    0xFD20, 1, 4},
  {"turn_left", "IDLE",   MOOD_NEUTRAL, MOOD_NEUTRAL, -12, 0,  0x4C9F, 0, 0},
  {"turn_right","IDLE",   MOOD_NEUTRAL, MOOD_NEUTRAL, 12, 0,   0x4C9F, 0, 0},
  {"step_forward","LISTEN", MOOD_LISTEN, MOOD_LISTEN, 0, -8,   0xFE60, 1, 0},
};
static const int FACE_COUNT = sizeof(faces) / sizeof(faces[0]);

static const Face *currentFace = &faces[0];
static uint32_t lastLook = 0;

static const Face *findFace(const char *key) {
  if (!key) return nullptr;
  for (int i = 0; i < FACE_COUNT; i++) {
    if (strcmp(faces[i].key, key) == 0) return &faces[i];
  }
  return nullptr;
}

static void showLabel(const char *label) {
  tft.fillRect(0, 200, 240, 40, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(8, 208);
  tft.print(label);
}

static void applyFace(const Face *f) {
  if (!f) return;
  currentFace = f;
  eyes.setStyle(f->color, f->blush, f->mouth);
  eyes.setEmotion(f->left, f->right);
  eyes.lookAt(f->lookX, f->lookY);
  showLabel(f->label);
  eyes.renderEmotions(canvas);
  Serial.printf("face=%s\n", f->label);
}

// Plain-text + JSON motion names share one face key.
static const char *faceKeyFor(const char *key) {
  if (strcmp(key, "forward") == 0 || strcmp(key, "tien") == 0 ||
      strcmp(key, "backward") == 0 || strcmp(key, "lui") == 0 ||
      strcmp(key, "step_back") == 0) {
    return "step_forward";
  }
  if (strcmp(key, "left") == 0 || strcmp(key, "trai") == 0) return "turn_left";
  if (strcmp(key, "right") == 0 || strcmp(key, "phai") == 0) return "turn_right";
  return key;
}

static const char *emotionToMotion(const char *emotion) {
  if (!emotion || !emotion[0]) return nullptr;
  if (strcmp(emotion, "idle") == 0) return "idle";
  if (strcmp(emotion, "listening") == 0) return "idle";
  if (strcmp(emotion, "thinking") == 0) return "think";
  if (strcmp(emotion, "speaking") == 0) return "nod";
  if (strcmp(emotion, "happy") == 0) return "happy";
  if (strcmp(emotion, "alert") == 0) return "alert";
  return nullptr;
}

static bool runMotion(const char *name) {
  if (strcmp(name, "idle") == 0) {
    Otto.home();
    return true;
  }
  if (strcmp(name, "nod") == 0) {
    Otto.bend(1, 1200, LEFT);
    Otto.home();
    return true;
  }
  if (strcmp(name, "shake") == 0) {
    Otto.shakeLeg(1, 1200, RIGHT);
    Otto.home();
    return true;
  }
  if (strcmp(name, "think") == 0) {
    Otto.jitter(3, 600, 20);
    Otto.home();
    return true;
  }
  if (strcmp(name, "happy") == 0) {
    Otto.swing(2, 800, FOOT_H);
    Otto.tiptoeSwing(1, 500, FOOT_H);
    Otto.home();
    return true;
  }
  if (strcmp(name, "alert") == 0) {
    Otto.swing(2, 600, FOOT_H);
    Otto.shakeLeg(1, 1000, LEFT);
    Otto.home();
    return true;
  }
  if (strcmp(name, "turn_left") == 0) {
    Otto.turn(3, 1000, LEFT);
    Otto.home();
    return true;
  }
  if (strcmp(name, "turn_right") == 0) {
    Otto.turn(3, 1000, RIGHT);
    Otto.home();
    return true;
  }
  // 4 servos: walk/turn set hip amp 30 and foot amp 40 (Otto.cpp).
  if (strcmp(name, "forward") == 0 || strcmp(name, "tien") == 0 ||
      strcmp(name, "step_forward") == 0) {
    Otto.walk(3, 1000, FORWARD);
    Otto.home();
    return true;
  }
  if (strcmp(name, "backward") == 0 || strcmp(name, "lui") == 0 ||
      strcmp(name, "step_back") == 0) {
    Otto.walk(3, 1000, BACKWARD);
    Otto.home();
    return true;
  }
  if (strcmp(name, "left") == 0 || strcmp(name, "trai") == 0) {
    Otto.turn(3, 1000, LEFT);
    Otto.home();
    return true;
  }
  if (strcmp(name, "right") == 0 || strcmp(name, "phai") == 0) {
    Otto.turn(3, 1000, RIGHT);
    Otto.home();
    return true;
  }
  return false;
}

// Mat truoc, servo sau — cung key (emotion/motion)
static bool act(const char *key) {
  const Face *f = findFace(faceKeyFor(key));
  if (f) applyFace(f);

  const char *motion = key;
  // led.state listening/speaking/thinking → motion khac ten
  const char *mapped = emotionToMotion(key);
  if (mapped) motion = mapped;

  return runMotion(motion);
}

static const char *faceLabel() {
  return currentFace ? currentFace->label : "IDLE";
}

static void publishStatus(bool ok, const char *name, const char *err) {
  JsonDocument doc;
  doc["v"] = 1;
  doc["from"] = "esp32";
  doc["ok"] = ok;
  doc["face"] = faceLabel();
  if (ok && name) doc["name"] = name;
  if (!ok && err) doc["error"] = err;
  char buf[192];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  Serial.println(buf);
  if (mqtt.connected() && n > 0) {
    mqtt.publish(TOPIC_STATUS, buf, false);
  }
}

static void sendAck(const char *id, bool ok, const char *name, const char *err) {
  JsonDocument doc;
  doc["v"] = 1;
  if (id && id[0]) doc["id"] = id;
  doc["from"] = "esp32";
  doc["ok"] = ok;
  if (ok && name) doc["name"] = name;
  if (!ok && err) doc["error"] = err;
  serializeJson(doc, Serial);
  Serial.println();
  publishStatus(ok, name, err);
}

static void handleMqttPayload(char *payload) {
  while (*payload == ' ' || *payload == '\t') payload++;
  size_t n = strlen(payload);
  while (n > 0 && (payload[n - 1] == ' ' || payload[n - 1] == '\t' ||
                   payload[n - 1] == '\r' || payload[n - 1] == '\n')) {
    payload[--n] = '\0';
  }
  if (n == 0) return;

  for (size_t i = 0; i < n; i++) {
    if (payload[i] >= 'A' && payload[i] <= 'Z') payload[i] = payload[i] - 'A' + 'a';
  }

  if (strcmp(payload, "status") == 0) {
    publishStatus(true, "status", nullptr);
    return;
  }
  if (act(payload)) {
    publishStatus(true, payload, nullptr);
  } else {
    publishStatus(false, nullptr, "bad_args");
  }
}

static void onMqttMessage(char *topic, byte *message, unsigned int length) {
  if (strcmp(topic, TOPIC_CMD) != 0) return;
  if (length >= 64) length = 63;
  char payload[64];
  memcpy(payload, message, length);
  payload[length] = '\0';
  Serial.printf("MQTT cmd: %s\n", payload);
  handleMqttPayload(payload);
}

static void connectWifi() {
  if (strcmp(WIFI_SSID, "CHANGE_ME") == 0 || WIFI_SSID[0] == '\0') {
    Serial.println("WIFI: dien SSID/PASS trong wifi_secrets.h roi nap lai");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("WiFi connecting to %s", WIFI_SSID);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(400);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi OK IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi FAIL");
  }
}

static void ensureMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) return;
  String cid = String("otto-") + String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.printf("MQTT connect %s ...\n", MQTT_HOST);
  if (mqtt.connect(cid.c_str())) {
    mqtt.subscribe(TOPIC_CMD);
    Serial.printf("MQTT OK sub %s\n", TOPIC_CMD);
    publishStatus(true, "online", nullptr);
  } else {
    Serial.printf("MQTT fail rc=%d\n", mqtt.state());
  }
}

static void handleLine(char *line) {
  while (*line == ' ' || *line == '\t') line++;

  if (line[0] != '{') {
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\t')) line[--n] = '\0';
    if (n == 0) return;
    if (strcmp(line, "status") == 0) {
      publishStatus(true, "status", nullptr);
      return;
    }
    if (act(line)) {
      sendAck("s", true, line, nullptr);
    } else {
      sendAck("s", false, nullptr, "bad_args");
    }
    return;
  }

  JsonDocument req;
  if (deserializeJson(req, line)) {
    sendAck("", false, nullptr, "bad_args");
    return;
  }

  const char *id = req["id"] | "";
  const char *cmd = req["cmd"] | "";
  if (!id[0] || !cmd[0]) {
    sendAck(id, false, nullptr, "bad_args");
    return;
  }

  if (strcmp(cmd, "ping") == 0) {
    JsonDocument doc;
    doc["v"] = 1;
    doc["id"] = id;
    doc["from"] = "esp32";
    doc["ok"] = true;
    serializeJson(doc, Serial);
    Serial.println();
    return;
  }

  if (strcmp(cmd, "motion") == 0) {
    const char *name = req["name"] | "";
    if (!name[0] || !act(name)) {
      sendAck(id, false, nullptr, "bad_args");
      return;
    }
    sendAck(id, true, name, nullptr);
    return;
  }

  if (strcmp(cmd, "led") == 0) {
    const char *state = req["state"] | "";
    if (!state[0] || !act(state)) {
      sendAck(id, false, nullptr, "bad_args");
      return;
    }
    JsonDocument doc;
    doc["v"] = 1;
    doc["id"] = id;
    doc["from"] = "esp32";
    doc["ok"] = true;
    doc["state"] = state;
    doc["name"] = emotionToMotion(state);
    serializeJson(doc, Serial);
    Serial.println();
    return;
  }

  sendAck(id, false, nullptr, "unknown_cmd");
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("=== 03 MQTT Otto Internet ===");

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  canvas.createSprite(240, 120);

  Otto.init(LeftLeg, RightLeg, LeftFoot, RightFoot, false, BuzzerPin);
  Otto.setTrims(0, 14, 0, 0);
  Otto.home();

  applyFace(findFace("idle"));

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(256);
  connectWifi();
  ensureMqtt();

  Serial.printf("ready — pub to %s  (happy/forward/...)\n", TOPIC_CMD);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t lastWifi;
    if (millis() - lastWifi > 5000) {
      lastWifi = millis();
      connectWifi();
    }
  } else {
    if (!mqtt.connected()) {
      static uint32_t lastMqtt;
      if (millis() - lastMqtt > 3000) {
        lastMqtt = millis();
        ensureMqtt();
      }
    } else {
      mqtt.loop();
    }
  }

  eyes.renderEmotions(canvas);

  if (millis() - lastLook > 900 && currentFace) {
    eyes.lookAt(currentFace->lookX + random(-10, 11),
                currentFace->lookY + random(-8, 9));
    lastLook = millis();
  }

  static char buf[256];
  static size_t len = 0;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (len == 0) continue;
      buf[len] = '\0';
      handleLine(buf);
      len = 0;
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    } else {
      len = 0;
      sendAck("", false, nullptr, "bad_args");
    }
  }
}
