# 🌱 AgroMate — AI Farming Robot

[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://python.org)
[![FastAPI](https://img.shields.io/badge/FastAPI-0.100+-009688?logo=fastapi&logoColor=white)](https://fastapi.tiangolo.com)
[![ESP32](https://img.shields.io/badge/ESP32--S3-PlatformIO-E7352C?logo=espressif&logoColor=white)](https://platformio.org)
[![Android](https://img.shields.io/badge/Android-Jetpack%20Compose-3DDC84?logo=android&logoColor=white)](https://developer.android.com)
[![BLE](https://img.shields.io/badge/BLE-5.0-0082FC?logo=bluetooth&logoColor=white)](https://www.bluetooth.com)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

> **An intelligent AI-powered farming robot system** that identifies plants, detects diseases, provides agricultural advice, and speaks multiple languages — all powered by a laptop AI server communicating with an ESP32 robot via Bluetooth Low Energy.

---

## 🏗️ System Architecture

```
┌─────────────────────┐         BLE          ┌─────────────────────┐
│   🤖 ESP32 Robot    │◄────────────────────►│  💻 Laptop AI Server│
│                     │                       │                     │
│  • TFT Display      │    Commands/Data      │  • FastAPI Backend  │
│  • AI Face & Eyes   │◄────────────────────►│  • Plant AI Engine  │
│  • Microphone       │                       │  • Disease Detection│
│  • Speaker          │    Images/Audio       │  • Voice Processing │
│  • Camera           │────────────────────►│  • AI Chatbot       │
│  • LEDs & Buttons   │                       │  • Database         │
│  • Battery Monitor  │    AI Responses       │  • Weather Service  │
│  • Servo Motors     │◄────────────────────│  • Dataset Manager  │
└─────────────────────┘                       └────────┬────────────┘
                                                       │
                                              HTTP/WebSocket
                                                       │
                                              ┌────────▼────────────┐
                                              │  📱 Android App     │
                                              │                     │
                                              │  • Dashboard        │
                                              │  • AI Chat          │
                                              │  • Scan History     │
                                              │  • Robot Manager    │
                                              │  • Analytics        │
                                              └─────────────────────┘
```

## 📁 Project Structure

```
AgroMate/
├── backend/          # Python AI Server (FastAPI + BLE + AI)
├── firmware/         # ESP32 Firmware (PlatformIO + Arduino)
├── android/          # Android App (Kotlin + Jetpack Compose)
├── docs/             # Documentation
├── datasets/         # Downloaded datasets (gitignored)
└── models/           # Trained ML models (gitignored)
```

## 🚀 Quick Start

### Prerequisites

- **Python 3.11+** with pip
- **PlatformIO** (VS Code extension or CLI)
- **Android Studio** Hedgehog+ with Kotlin plugin
- **Bluetooth 5.0+** adapter on laptop

### 1. Backend (Laptop AI Server)

```bash
cd backend
python -m venv venv
venv\Scripts\activate        # Windows
# source venv/bin/activate   # Linux/macOS
pip install -r requirements.txt
cp .env.example .env
# Edit .env with your API keys
python main.py
```

The server starts at `http://localhost:8000` with interactive docs at `/docs`.

### 2. Firmware (ESP32)

```bash
cd firmware
pio run                      # Compile
pio run -t upload            # Flash to ESP32
pio device monitor           # Serial monitor
```

### 3. Android App

Open `android/` in Android Studio and run on device/emulator.

## 🌟 Features

### 🧠 AI Capabilities
- 🌿 **Plant Identification** — 100+ crop species recognition
- 🦠 **Disease Detection** — Multi-class disease classification
- 🐛 **Pest Detection** — Object detection for common pests
- 🧪 **Nutrient Analysis** — Leaf color deficiency detection
- 🌾 **Crop Advisory** — Season & location-aware recommendations
- 🌤️ **Weather Analysis** — Agricultural weather forecasting

### 🗣️ Voice Assistant
- 🎤 Speech recognition (English, Hindi, Gujarati)
- 🔊 Natural text-to-speech with Edge-TTS
- 💬 Context-aware agricultural chatbot

### 🤖 Robot Features
- 👀 Expressive animated AI face with eye tracking
- 📸 Camera-based plant scanning
- 🔋 Battery management with sleep/wake
- 💡 RGB LED status indicators

### 📱 Mobile App
- Real-time robot status dashboard
- AI chat interface
- Scan history & disease reports
- Analytics dashboard

## 📡 BLE Communication

Custom binary protocol over Bluetooth Low Energy with:
- Chunked image/audio transfer
- Automatic reconnection
- Multi-robot support
- < 5s image transfer latency

## 🗂️ Data Sources

Automated dataset collection from PlantVillage, PlantDoc, Kaggle, Trefle API, Plant.id, Pl@ntNet, OpenWeatherMap, FAO, and government agriculture portals.

## 📄 License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for details.

## 🤝 Contributing

Contributions welcome! Please read our contributing guidelines before submitting PRs.

---

<p align="center">
  <b>Built with 💚 for sustainable agriculture</b>
</p>
