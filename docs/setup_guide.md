# AgroMate System Setup & Configuration Guide

This guide walks you through setting up the Laptop AI Server, ESP32 Firmware, and the companion Android app.

---

## 1. Laptop AI Server Setup (Python)

### Prerequisites
- Python 3.11 or higher installed on your computer.
- Bluetooth capability enabled on the laptop.

### Installation Steps
1. Navigate to the backend directory:
   ```bash
   cd backend
   ```
2. Create a virtual environment:
   ```bash
   python -m venv venv
   source venv/bin/activate  # On Windows: venv\Scripts\activate
   ```
3. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```
4. Copy the environment variables template and configure it:
   ```bash
   cp .env.example .env
   ```
   Open `.env` and set your preferred keys (e.g. OpenAI keys, weather API tokens).
5. Start the FastAPI server:
   ```bash
   python main.py
   ```

The server will initialize SQLite database migrations, start the background BLE manager, and serve the API routes on `http://localhost:8000`.

---

## 2. ESP32 Firmware Setup (PlatformIO)

### Prerequisites
- VS Code installed with the **PlatformIO IDE** extension.
- ESP32-S3 board connected via USB.

### Build and Flash
1. Open the `firmware/` directory in VS Code. PlatformIO will automatically read `platformio.ini` and download libraries (NimBLE, TFT_eSPI, etc.).
2. Configure your display driver settings in `.pio/libdeps/esp32-s3-devkitc-1/TFT_eSPI/User_Setup.h` matching the pin layout defined in `src/config.h`.
3. Press **Build** (checkmark icon in status bar) to compile.
4. Press **Upload** (arrow icon in status bar) to flash the firmware onto your ESP32 board.
5. Open the Serial Monitor at `115200` baud rate to check diagnostic logs.

Once flashed, the ESP32 will start advertising as `AgroMate-Robot` and wait for the laptop server to pair.

---

## 3. Companion Android App Setup (Gradle)

### Prerequisites
- Android Studio Hedgehog (or later) installed.
- Android device with Bluetooth & Location enabled.

### Build and Run
1. Open Android Studio and choose **Open Project**.
2. Select the `android/` directory. Android Studio will download Gradle tools and sync dependencies.
3. Open `app/src/main/java/com/agromate/app/di/AppModule.kt` and change the default Retrofit Server URL to match your laptop's local IP address.
4. Connect your Android device via USB and enable USB Debugging.
5. Click the **Run** button (green play icon) in the toolbar to compile and install the application.
