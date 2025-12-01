# Auto Light Controller System
Sistem Auto Light Controller System adalah sistem pencahayaan cerdas berbasis ESP32-S3 + FreeRTOS yang mampu beroperasi dalam dua mode:
- AUTO — kecerahan diatur otomatis berdasarkan nilai sensor LDR (analog & digital)
- MANUAL — kecerahan dikendalikan secara manual menggunakan potensiometer

Sistem ini dilengkapi dengan OLED Display, LED RGB indikator, Buzzer, serta Emergency Stop menggunakan interrupt untuk keamanan sistem.

## 📌 Features
- **RTOS Multi-Tasking:** Menggunakan FreeRTOS dengan Queue, Semaphore, Mutex, dan Interrupt.
- **Dual-Core Processing:** Tugas dibagi ke core 0 dan core 1 untuk performa optimal.
- **Hybrid LDR Control:** Menggabungkan data analog & digital dari LDR untuk auto-brightness.
- **Manual Potentiometer Control:** Pengguna dapat mengatur kecerahan secara manual.
- **OLED System Monitoring**
- **RGB Mode Status Indicator**
- **Emergency Stop (Interrupt):** Menghentikan sistem secara instan dan mengaktifkan buzzer.

## 🧩 System Architecture
**RTOS Components Used**
| Komponen            | Fungsi                                                 |
| ------------------- | ------------------------------------------------------ |
| **Queue**           | Komunikasi antar-task (sensor → controller).           |
| **Semaphore**       | Akses eksklusif ke OLED (hindari flicker & data race). |
| **Mutex**           | Melindungi variabel shared seperti *brightnessShared*. |
| **Interrupt (ISR)** | Emergency stop dengan prioritas tinggi.                |


**Task Distribution**
| Core       | Task             | Fungsi                                            |
| ---------- | ---------------- | ------------------------------------------------- |
| **Core 0** | TaskSensor       | Membaca LDR (AO & DO) dan mengirim melalui Queue. |
|            | TaskInputUser    | Membaca potensiometer dan mengirim via Queue.     |
|            | TaskOLED         | Update display menggunakan semaphore.             |
|            | TaskAlarm        | Kontrol LED RGB & buzzer.                         |
| **Core 1** | TaskControl      | Proses logika mode AUTO/MANUAL & emergency stop.  |
|            | TaskLightControl | Software-PWM lampu berdasarkan brightnessShared.  |
|            | ISR_Emergency    | Interrupt tombol emergency.                       |

## 🛠️ Hardware Wiring Diagram
**ESP32-S3 Pin Configuration**
| Komponen            | Pin     | Keterangan                  |
| ------------------- | ------- | --------------------------- |
| **LDR Sensor (AO)** | GPIO 12 | Input analog                |
| **LDR Sensor (DO)** | GPIO 41 | Trigger intensitas cahaya   |
| **Potentiometer**   | GPIO 2  | Input manual control        |
| **Mode Button**     | GPIO 11 | Switch AUTO ↔ MANUAL        |
| **Emergency Stop**  | GPIO 3  | Interrupt FALLING           |
| **LED PWM Output**  | GPIO 4  | Lamp control (software PWM) |
| **OLED I2C SDA**    | GPIO 5  | Display                     |
| **OLED I2C SCL**    | GPIO 6  | Display                     |
| **RGB LED - R**     | GPIO 7  | Status indicator            |
| **RGB LED - G**     | GPIO 8  | Status indicator            |
| **RGB LED - B**     | GPIO 9  | Status indicator            |
| **Buzzer**          | GPIO 10 | Emergency notification      |

**Wiring Diagram**

<img width="755" height="723" alt="Cuplikan layar 2025-12-01 011739" src="https://github.com/user-attachments/assets/9e121361-1eb4-4e9c-89c3-63ece1578b7f" />

## 🧠 Program Implementation Overview
**1. SensorData Structure**
```
typedef struct {
  int analogValue;
  int digitalValue;
} SensorData;
```
Digunakan untuk mengirim data komprehensif pembacaan LDR melalui Queue.

**2. Global Variables**
- `brightnessShared` - nilai kecerahan terkini
- `systemMode` - AUTO (0) / MANUAL (1)
- `emergencyStop` - flag dari interrupt
- Queue: `queueSensor`, `queueInput`
- Mutex: `mutexShared`
- Semaphore: `semOLED`

## 3. Task Descriptions
**TaskSensor**
- Membaca LDR setiap 200 ms
- Mengirim data ke Core 1 via Queue

**TaskInput**
- Membaca potensiometer
- Mengirim nilai ke TaskControl

**TaskControl**
- Logika utama sistem:
    - AUTO → hybrid analog + digital LDR
    - MANUAL → potensiometer
- Melindungi variabel shared dengan Mutex

**TaskLightControl**
- Software PWM untuk LED utama
- Membaca `brightnessShared`

**TaskOLED**
- Update layar menggunakan semaphore

**TaskAlarm**
- RGB LED indikator mode
- Biru → Emergency
- Hijau → Auto
- Ungu → Manual

**ISR Emergency**
- Menghentikan semua proses normal
- Mengaktifkan buzzer + LED merah

## 📷 Demo Video
https://github.com/user-attachments/assets/50f24851-011c-4d32-8c7c-ebc2009f0899

