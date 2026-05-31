markdown_content = """# BẢN ĐỒ MAP CHÂN (PIN-TO-PIN) DỰ ÁN TAY CẦM GAME ESP32
Thao tác đấu nối giữa 1 mạch **ESP32 Type-C 38-pin** và 2 mạch **Arduino Joystick Shield (Funduino)** phục vụ game "Ngôi đền pha lê".

---

## 1. Sơ đồ mạch nguồn tổng (Nuôi ESP32)
*Nguồn cấp phát từ Pin 18650 -> Mạch sạc TP4056 -> Mạch tăng áp MT3608 (Vặn biến trở kích lên đúng 5V).*

| Đầu ra mạch tăng áp MT3608 | Chân trên mạch ESP32 | Chức năng |
| :--- | :--- | :--- |
| **VOUT+ (Cực Dương 5V)** | Chân 19 (**VIN / 5V**) | Cấp nguồn tổng nuôi hệ thống |
| **VOUT- (Cực Âm GND)** | Chân 18 (**GND**) | Tiếp mát chung hệ thống nguồn |

---

## 2. Sơ đồ đấu nối TAY CẦM 1 (Người chơi 1 - Nước)
*Đấu nối tại chỗ bên trong vỏ hộp chính (Sử dụng hàng chân BÊN TRÁI của ESP32).*

| Tín hiệu trên Joystick Shield 1 | Chân trên mặt Shield | Vị trí chân trên ESP32 | Chế độ khai báo trong Code |
| :--- | :--- | :--- | :--- |
| **Nguồn Dương (+)** | Chân `3.3V` (Hàng nguồn trái) | **Chân 1 (3.3V)** | Cấp nguồn cụm biến trở |
| **Nguồn Âm (-)** | Chân `GND` (Hàng nguồn trái) | **Chân 14 (GND)** | Tiếp mát chung cho Joy 1 |
| **Trục X (Trái/Phải)** | Chân `A0 / X-Axis` | **Chân 3 (GPIO36 / VP)** | `Analog Input` (ADC1) |
| **Trục Y (Lên/Xuống)** | Chân `A1 / Y-Axis` | **Chân 4 (GPIO39 / VN)** | `Analog Input` (ADC1) |
| **Nút A** | Chân `D2 / A Button` | **Chân 7 (GPIO32)** | `INPUT_PULLUP` |
| **Nút B** | Chân `D3 / B Button` | **Chân 8 (GPIO33)** | `INPUT_PULLUP` |
| **Nút C** | Chân `D4 / C Button` | **Chân 9 (GPIO25)** | `INPUT_PULLUP` |
| **Nút D** | Chân `D5 / D Button` | **Chân 10 (GPIO26)** | `INPUT_PULLUP` |
| **Nút E** | Chân `D6 / E Button` | **Chân 11 (GPIO27)** | `INPUT_PULLUP` |
| **Nút F** | Chân `D7 / F Button` | **Chân 15 (GPIO13)** | `INPUT_PULLUP` |

---

## 3. Sơ đồ đấu nối TAY CẦM 2 (Người chơi 2 - Lửa)
*Truyền dẫn tín hiệu từ Vỏ hộp phụ sang Vỏ hộp chính thông qua **Cáp mạng LAN 8 lõi**.*

| Tín hiệu trên Joystick Shield 2 | Chân trên mặt Shield 2 | Màu lõi dây LAN (Gợi ý) | Vị trí chân trên ESP32 (Hộp 1) | Chế độ trong Code |
| :--- | :--- | :--- | :--- | :--- |
| **Nguồn Âm (-)** | Chân `GND` | **Nâu** | **Chân 38 (GND)** | Tiếp mát chung cho Joy 2 |
| **Trục X (Trái/Phải)** | Chân `A0 / X-Axis` | **Cam** | **Chân 5 (GPIO34)** | `Analog Input` (ADC1) |
| **Trục Y (Lên/Xuống)** | Chân `A1 / Y-Axis` | **Sọc Cam** | **Chân 6 (GPIO35)** | `Analog Input` (ADC1) |
| **Nút A** | Chân `D2 / A Button` | **Xanh Lá** | **Chân 37 (GPIO23)** | `INPUT_PULLUP` |
| **Nút B** | Chân `D3 / B Button` | **Sọc Xanh Lá** | **Chân 36 (GPIO22)** | `INPUT_PULLUP` |
| **Nút C** | Chân `D4 / C Button` | **Xanh Dương** | **Chân 33 (GPIO21)** | `INPUT_PULLUP` |
| **Nút D** | Chân `D5 / D Button` | **Sọc Xanh Dương** | **Chân 31 (GPIO19)** | `INPUT_PULLUP` |
| **Nguồn Dương (+)** | Chân `3.3V` | **Sọc Nâu** | **Chân 1 (3.3V)** | Cấp nguồn ổn định (Tùy chọn) |

---

## ⚠️ 4. Các lưu ý kỹ thuật bắt buộc khi lắp ráp & Lập trình

1. **Gạt công tắc nguồn trên cả 2 Shield:** Trên cả hai bo mạch Joystick Shield, tìm công tắc gạt nhỏ mang nhãn `Voltage Select` và gạt dứt khoát về nấc **3.3V** trước khi cấp điện. Việc này đảm bảo các đường Analog không vượt quá ngưỡng chịu đựng của ESP32.
2. **Khai báo nút nhấn trong Code:** Tất cả các chân Digital nhận nút bấm (Nút A-F của Joy 1, và A-D của Joy 2) phải được cấu hình bằng lệnh: