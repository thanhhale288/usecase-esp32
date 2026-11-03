# Hướng dẫn 4-trong-1 — tái dùng dây Jarvis (ít thay đổi nhất)

**Không tháo dây.** Giữ nguyên ESP32 + TFT + 4 servo (+ cảm biến nếu đang cắm).  
Chỉ **đổi firmware / thêm kênh nhận lệnh** — cùng `act()` đã chạy (`happy`, `forward`, …).

Báo cáo đặt tên kiểu *“Hệ thống điều khiển robot ESP32 (TFT + servo) qua BT / LAN / Internet”* — **không** nộp như đồ án Jarvis.

---

## 1. Giữ nguyên phần cứng

Pin map đã chốt (`esp32/README.md`):

| Thiết bị | GPIO |
|----------|------|
| TFT ST7789 | 22, 21, 23, 18, 27 |
| CT / CP / BT / BP | 14 / 13 / 26 / 16 |
| Nguồn servo | 5V ngoài + GND chung |

Sơ đồ đấu nối nộp bài = copy bảng này + ảnh bàn mạch hiện tại.

---

## 2. Công thức 4-trong-1

| Lớp | Việc |
|-----|------|
| Đầu ra | TFT face + 4 servo — **code sẵn** `esp32/otto_motion` |
| Lệnh chung | `happy` · `idle` · `forward` · `backward` · `left` · `right` · `status` |
| Kênh 1 BT | Cùng lệnh qua `BluetoothSerial` |
| Kênh 2 LAN | WebServer nút / URL gọi cùng lệnh |
| Kênh 3 Internet | MQTT topic nhận cùng lệnh |

**Thay đổi tối thiểu:** copy `otto_motion` → 3 biến thể chỉ khác *cách đọc lệnh vào* `handleLine` / `act`. Không đổi pin, không đổi wiring.

```
[Phone/PC] --BT / Wi‑Fi / MQTT--> [ESP32] --> act() --> TFT + servo
```

---

## 3. Map 4 chủ đề

| Chủ đề | Lý thuyết | Demo trên phần cứng đang nối |
|--------|-----------|------------------------------|
| 1. ESP32 | GPIO, PWM servo, SPI TFT, Arduino | Pin map + `setup`/`loop` otto |
| 2. Bluetooth | Classic, lệnh text | Gửi `happy` qua Serial BT Terminal |
| 3. LAN | Hotspot, HTTP, WebServer | Nút Happy / Forward trên trình duyệt |
| 4. Internet | MQTT, khác mạng | Publish `forward` từ phone LTE/wifi khác |

---

## 4. Cấu trúc thư mục (trong worktree này)

```
ESP32_ThucHanh/
  HUONG_DAN.md          ← file này
  README.md
  01_bluetooth/         ← fork otto_motion + BluetoothSerial
  02_lan/               ← fork otto_motion + WiFi WebServer
  03_internet/          ← fork otto_motion + MQTT
```

Nền: copy từ `../esp32/otto_motion/otto_motion.ino` (đã có gait + emotion).  
`01_bluetooth` kiểu LED GPIO2 cũ = **không dùng** cho hướng này (có thể xóa / bỏ qua).

---

## 5. Làm theo thứ tự

### A. Bluetooth (~40 phút)

1. Copy `esp32/otto_motion` → `ESP32_ThucHanh/01_bluetooth`
2. Thêm `BluetoothSerial`; trong `loop` đọc `SerialBT` giống đọc `Serial` → `handleLine`
3. Upload (vẫn TFT User_Setup Otto)
4. App **Serial Bluetooth Terminal** → tên vd. `ESP32_OTTO`
5. Gửi `happy` / `forward` — mắt + chân chạy → chụp/video

### B. LAN (~45 phút)

1. Copy otto_motion → `02_lan`
2. `WiFi.begin` Hotspot laptop (SSID/pass **không** commit công khai)
3. `WebServer(80)`: trang HTML nút Happy / Forward / Idle / Left / Right
4. Handler gọi `act("happy")` tương ứng
5. Điện thoại cùng Hotspot mở `http://IP/` → demo + chụp

### C. Internet MQTT (~45 phút)

1. Copy → `03_internet` + PubSubClient
2. Broker test: `broker.hivemq.com:1883`
3. Subscribe `esp32/<ten>/cmd` — payload = `happy` / `forward` / …
4. Publish status lên `esp32/<ten>/status`
5. Phone **khác mạng** publish `happy` → demo + chụp

### D. Báo cáo

- Sơ đồ: 3 kênh vào cùng ESP32/TFT/servo  
- Đấu nối: bảng pin Jarvis (đổi tên “robot thực hành”)  
- So sánh BT vs LAN vs MQTT  
- Ảnh 3 demo; che mật khẩu Wi‑Fi  

---

## 6. Checklist “ít đụng dây”

- [ ] Không rút servo / TFT / level shifter  
- [ ] Vẫn nguồn 5V ngoài + GND chung  
- [ ] Chỉ unplug USB khi nạp firmware nếu cần  
- [ ] 3 sketch cùng pin `#define` như otto_motion  
- [ ] Cùng bộ lệnh Serial đã quen  

---

## 7. Nạp (port máy bạn — thường 2120)

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 ESP32_ThucHanh/01_bluetooth
arduino-cli upload -p /dev/cu.usbserial-2120 --fqbn esp32:esp32:esp32 \
  --upload-property upload.speed=115200 ESP32_ThucHanh/01_bluetooth
```
