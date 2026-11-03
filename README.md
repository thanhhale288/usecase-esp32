# Use case ESP32 — Bluetooth / LAN / Internet

Bài thực hành: điều khiển robot **Otto (TFT + 4 servo)** qua 3 kênh, cùng phần cứng.

| Thư mục | Kênh | Cách dùng |
|---------|------|-----------|
| `01_bluetooth/` | Bluetooth Classic | App **Serial Bluetooth Terminal** → thiết bị `ESP32_OTTO` |
| `02_lan/` | SoftAP + WebServer | Wi‑Fi `ESP32_OTTO_LAN` → http://192.168.4.1/ |
| `03_internet/` | MQTT | HiveMQ public broker → topic `otto/hale/cmd` |

## Lệnh chung

`happy` · `idle` · `alert` · `forward` · `backward` · `left` · `right` · `status`

## Pin (không tháo dây Otto)

| Thiết bị | GPIO |
|----------|------|
| TFT ST7789 | CS22 DC21 MOSI23 SCK18 RST27 |
| Servo CT/CP/BT/BP | 14 / 13 / 26 / 16 |
| Nguồn servo | 5V ngoài + GND chung |

## Nạp firmware

Board: **ESP32 Dev Module**

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 01_bluetooth
arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn esp32:esp32:esp32 \
  --upload-property upload.speed=115200 01_bluetooth
```

Thư viện: `ArduinoJson`, `TFT_eSPI` (User_Setup Otto), `Grobot_Animations`, `OttoDIYLib`, `ESP32Servo`, `PubSubClient` (cho MQTT).

### MQTT — cấu hình Wi‑Fi

```bash
cp 03_internet/wifi_secrets.h.example 03_internet/wifi_secrets.h
# điền WIFI_SSID / WIFI_PASS (mạng có Internet)
```

**Không commit** `wifi_secrets.h`.

Broker: `broker.hivemq.com:1883`  
- Publish lệnh: `otto/hale/cmd`  
- Subscribe trạng thái: `otto/hale/status`

## Bảo mật

Không đưa mật khẩu Wi‑Fi nhà / API key vào repo hoặc báo cáo công khai.
