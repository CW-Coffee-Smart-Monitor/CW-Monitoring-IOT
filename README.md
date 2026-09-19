# CW-Monitoring-IOT

[![PlatformIO CI](https://github.com/FarrelAD/CW-Monitoring-IOT/actions/workflows/ci.yml/badge.svg)](https://github.com/FarrelAD/CW-Monitoring-IOT/actions/workflows/ci.yml)

Smart Coworking Space / Desk Monitoring System powered by ESP32, RFID (MFRC522), HC-SR04 Ultrasonic Distance Sensor, RGB LED indicators, and Real-Time TCP Socket / HTTP Webhooks.

---

## 🚀 Features

- **RFID Authentication**: Check-in and check-out tracking via RC522 RFID reader with remote validation.
- **Seat Occupancy Detection**: Ultrasonic HC-SR04 distance thresholding (`< 20 cm` considered occupied).
- **Auto-Checkout Timer**: Automatically clears seat reservation and checks out if the desk remains empty beyond the timeout duration (e.g., 15s for demo / 15m in production).
- **Real-Time Indicators**: Common Anode RGB LED displaying real-time status (Green for checked in, Cyan for reserved, Yellow for seated unauthenticated, Blue for available, Purple/Red for checkout & warnings).
- **Socket & Webhook Communication**: TCP Socket server connection with bidirectional commands and HTTP webhook reporting.
- **Automated Unit Testing**: Native C++ unit tests using the Unity test framework on PlatformIO without needing physical hardware.

---

## 🛠️ Hardware & Pinout (ESP32)

| Component | Pin Function | ESP32 GPIO | Notes |
| :--- | :--- | :--- | :--- |
| **MFRC522 RFID** | SS / SDA | GPIO 5 | SPI bus |
| | SCK | GPIO 18 | SPI bus |
| | MOSI | GPIO 23 | SPI bus |
| | MISO | GPIO 19 | SPI bus |
| | RST | GPIO 22 | Reset pin |
| **HC-SR04 Sensor** | TRIG | GPIO 26 | Ultrasonic trigger |
| | ECHO | GPIO 27 | Ultrasonic echo |
| **RGB LED** | RED | GPIO 2 | Common Anode (LOW = ON) |
| | GREEN | GPIO 4 | Common Anode (LOW = ON) |
| | BLUE | GPIO 21 | Common Anode (LOW = ON) |
| | Common / VCC | 3V3 | Power rail |

---

## 📁 Project Architecture (PlatformIO Standard)

```
CW-Monitoring-IOT/
├── include/
│   └── Config.h               # Central configuration: GPIO pins, WiFi credentials, server IPs, timeouts
├── lib/
│   ├── TableLogic/            # Core business logic: seat occupancy, RFID check-in/out, auto-checkout
│   ├── CommandHandler/        # TCP socket command parser & normalizer
│   ├── HardwareDrivers/       # Common Anode RGB LED & HC-SR04 ultrasonic distance sensor drivers
│   └── NetworkManager/        # WiFi connection, TCP Socket maintenance, HTTP REST validation & telemetry
├── src/
│   └── main.cpp               # Lightweight orchestrator (~220 lines): setup() and loop() event handling
├── test/
│   ├── test_table_logic/      # Native unit tests for TableLogic
│   └── test_command_handler/  # Native unit tests for CommandHandler
├── platformio.ini             # Dual-environment configuration (esp32dev & native)
└── README.md
```

### Running Tests

To run all automated test suites on your local computer:

```bash
# If PlatformIO is in your system PATH:
pio test -e native

# On Windows PowerShell using the PlatformIO virtualenv directly:
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" test -e native
```

To run a specific test suite:
```bash
pio test -e native -f test_table_logic
pio test -e native -f test_command_handler
```

### What is Covered by Tests:
1. **Occupancy Detection (`isOccupied`)**: Correct threshold handling (`< 20.0 cm`), distance filtering, and handling sensor error / negative values.
2. **Check-In Validation**:
   - Successful check-in when seat is occupied.
   - Rejection when RFID UID is disallowed by server.
   - Rejection when trying to check in to an empty seat (`NOT_OCCUPIED`).
3. **Check-Out Handling**:
   - Tapping the active card checks out and frees the desk.
   - Tapping another card while the desk is occupied is safely rejected (`TABLE_ALREADY_USED_BY_OTHER_UID`).
4. **Auto Check-Out Timer**:
   - Verifies transition through warning trigger when desk is vacated.
   - Ensures checkout occurs automatically once the timeout duration has elapsed.
5. **Command Parsing (`CommandHandler`)**:
   - Case-insensitive trimming and parsing of socket commands (`led-red`, `led-green`, `led-off`, `auto`, `reserve`, `cancel-reservation`).
   - Graceful fallback for unknown commands.

---

## 🔍 Code Quality, Formatting & Static Analysis

This repository enforces strict C++ diagnostics, automated formatting, and static analysis:

### 1. Code Formatting (`clang-format`)
Google C++ style with 4-space indent is configured in [`.clang-format`](.clang-format).
To format all files:
```bash
clang-format -i src/*.cpp include/*.h lib/*/*.h lib/*/*.cpp test/*/*.cpp
```

### 2. Static Analysis & Linting (`pio check`)
PlatformIO combines two complementary static analysis tools configured in `platformio.ini` and `.clang-tidy`:
- **`cppcheck`**: Scans for classic memory leaks, buffer overruns, uninitialized pointers, and undefined behavior.
- **`clang-tidy`**: Clang compiler AST analyzer checking modern C++ standards, bug-prone constructs, and readability rules.

To run both analyzers:
```bash
pio check -e esp32dev --skip-packages
```

### 3. Strict Compiler Diagnostics
Both `esp32dev` and `native` environments enforce strict compiler warnings (`-Wall -Wextra -Wunused`) to prevent hidden type-narrowing bugs or uninitialized variables.

### 4. Git Pre-Commit Hook
Install the Git hooks via `pre-commit` to automatically verify unit tests, syntax, and formatting before every commit:
```bash
pip install -e .[dev]
pre-commit install
```

---

## 🔨 Building & Uploading Firmware

### 1. Build the ESP32 Firmware
```bash
pio run -e esp32dev
```

### 2. Upload to ESP32
Connect your ESP32 board via USB, then run:
```bash
pio run -e esp32dev --target upload
```

### 3. Open Serial Monitor
```bash
pio device monitor -b 115200
```