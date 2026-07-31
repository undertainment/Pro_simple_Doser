# Pro-Simple Dosing Controller

ESP32-based multi-channel peristaltic pump controller with a professional web dashboard and Neptune Apex Classic integration.

## Features

- **4 pump channels** (configurable 1-8) with individual GPIO pins
- **Manual dosing** — start/stop/cancel per pump, set volume in mL
- **Scheduled dosing** — up to 4 schedules per pump with day-of-week bitmask
- **Prime mode** — run pump for a fixed small volume
- **Reservoir tracking** — capacity (mL) and level (%) per pump, Refill All button
- **Calibration** — timed run, enter measured volume, new rate calculated and saved
- **Pump count** — dynamically switch between 1-8 pumps via dashboard
- **Neptune Apex Classic integration** — dual-unit support, auto-poll `/cgi-bin/status.xml` via Basic auth, per-probe visibility toggles
- **Web dashboard** — dark/light theme, live KPI cards, inline editing, activity log, timezone selector
- **WiFi AP fallback** — configures as access point if station connection fails
- **EEPROM persistence** — pump config, schedules, and Apex settings saved with CRC16 validation
- **Config backup/restore** — download/upload full settings as a JSON file from the dashboard Quick Actions card

## Requirements

- Arduino ESP32 core (`esp32:esp32`)
- **ArduinoJson** library (used by config import) — install via Library Manager or `arduino-cli lib install ArduinoJson`

## Hardware

| Component | Details |
|-----------|---------|
| MCU | ESP32 (any variant) |
| Pump outputs | 4 channels (up to 8 in firmware) |
| Relay/Driver | MOSFET or mechanical relay per channel |
| Power | 24V DC (pumps) + 5V/3.3V (logic) |

### Pin Assignments (config.h)

| Pump | GPIO |
|------|------|
| Pump 1 | 32 |
| Pump 2 | 33 |
| Pump 3 | 25 |
| Pump 4 | 26 |

## Software Architecture

```
Pro-Simple.ino        -- main setup/loop
├── config.h          -- compile-time configuration
├── types.h           -- structs (PumpConfig, Schedule, ApexConfig, ...)
├── pump.cpp/.h       -- peristaltic pump GPIO control
├── dosing.cpp/.h     -- dose lifecycle (Idle → Priming → Dosing → Complete)
├── scheduler.cpp/.h  -- time-based scheduled dosing
├── apex.cpp/.h       -- Neptune Apex Classic HTTP client (Basic auth, XML parse)
├── web_server.cpp/.h -- HTTP server + embedded dashboard HTML + JS
├── dashboard.cpp/.h  -- JSON serialization for all API responses
├── storage.cpp/.h    -- EEPROM save/load with CRC16
└── logger.cpp/.h     -- circular log buffer (in-memory)
```

## Setup

### WiFi Credentials

Copy the example and edit:

```
cp wifi_config.example.h wifi_config.h
```

Edit `wifi_config.h` with your SSID and password. This file is gitignored.

If WiFi connection fails, the ESP32 starts an access point:
- **SSID:** `Pro-Simple`
- **Password:** `config123`
- **IP:** `192.168.4.1`

### Neptune Apex Classic

Configured from the dashboard (no code changes needed):
1. Open dashboard → click **Config** on the Apex Classic card
2. Enter IP address, port (default 80), username, password
3. Check **Enable** and **Save**
4. Toggle individual probe visibility with the chip toggles

Default credentials: `admin` / serial number (printed on Apex base unit).

## Build & Flash

### PlatformIO (recommended)

```ini
[env:esp32-dev]
platform = espressif32
board = esp32-devkitc-1
framework = arduino
monitor_speed = 115200
```

```bash
pio run --target upload
pio device monitor
```

### Arduino IDE

1. Install ESP32 board package (Tools → Board → Boards Manager)
2. Select your ESP32 board variant
3. Open `Pro-Simple/Pro-Simple.ino`
4. Edit `wifi_config.h` with your WiFi credentials
5. Upload

## Configuration (config.h)

| Define | Default | Description |
|--------|---------|-------------|
| `WIFI_TIMEOUT_MS` | 15000 | AP fallback timeout |
| `PUMP_COUNT` | 4 | Number of pump channels |
| `PUMP_DEFAULT_RATE` | 100.0 | Default flow rate (mL/min) |
| `PUMP_MIN_DOSE_ML` | 1.0 | Minimum dose volume |
| `PUMP_MAX_DOSE_ML` | 9999.0 | Maximum dose volume |
| `MAX_SCHEDULES` | 16 | Total schedules across all pumps |
| `APEX_UNIT_COUNT` | 2 | Number of Apex Classic units |
| `APEX_POLL_INTERVAL_MS` | 3600000 | Apex poll interval (1 hour) |
| `APEX_TIMEOUT_MS` | 10000 | Apex HTTP timeout |
| `EEPROM_SIZE` | 8192 | EEPROM allocation |

## API Endpoints

| Route | Description |
|-------|-------------|
| `GET /` | Dashboard HTML |
| `GET /api?path=status` | System status JSON |
| `GET /api?path=pumps` | Pump array JSON |
| `GET /api?path=schedules` | Schedule array JSON |
| `GET /api?path=logs` | Log entries JSON |
| `GET /api?path=apex` | Apex probe data JSON |
| `GET /api/dose?pump=X&vol=Y` | Start dose |
| `GET /api/pump?pump=X&name=Y` | Update pump config |
| `GET /api/schedule?pump=X&hour=H&minute=M&vol=V` | Add schedule |
| `GET /api/schedule/remove?index=X` | Remove schedule |
| `GET /api/reset` | Reset all totals |
| `GET /api/refill` | Reset all reservoirs to 100% |
| `GET /api/apex?unit=0&ip=...&enabled=true&probeMask=15` | Save Apex config |
| `GET /api/config/export` | Download full config as JSON backup |
| `POST /api/config/import` | Restore config from uploaded JSON backup |
| `GET /api/ntp` | Trigger NTP resync |
| `GET /api/timezone?min=-480` | Set timezone offset (minutes) |

State codes: `0=Idle, 1=Priming, 2=Dosing, 3=Complete, 4=Error`

## File Layout

```
Pro-Simple/
├── Pro-Simple.ino           -- main sketch
├── config.h                 -- compile-time configuration
├── types.h                  -- structs and enums
├── pump.cpp / pump.h
├── dosing.cpp / dosing.h
├── scheduler.cpp / scheduler.h
├── apex.cpp / apex.h        -- Neptune Apex Classic client
├── web_server.cpp / web_server.h
├── dashboard.cpp / dashboard.h
├── storage.cpp / storage.h
├── logger.cpp / logger.h
├── wifi_config.h            -- gitignored, real WiFi credentials
├── wifi_config.example.h    -- template for cloning
├── docs/BUILD.md
└── README.md
```

## Notes

- Dashboard is embedded in `web_server.cpp` as a raw string literal — no external files needed
- Polling interval: 5 seconds (frontend JS)
- Clock is client-side only; the timezone selector persists to the device (`/api/timezone`) and drives the NTP-based scheduler so doses fire at local time
- Time is synced via NTP (`pool.ntp.org`) on boot and on demand via `/api/ntp`; there is no manual time setting
- Apex data is cached on the ESP32 and served from memory; probes are filtered by `probeMask` before sending to dashboard
