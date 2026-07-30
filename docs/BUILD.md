# Pro-Simple Dosing Controller

ESP32-S3 based multi-channel dosing pump controller with a professional web dashboard.

## Hardware

| Component | Details |
|-----------|---------|
| MCU | ESP32-S3 |
| Pump outputs | 4 channels (configurable 1-8 in firmware) |
| Relay/Driver | MOSFET or relay per channel |
| Power | 24V DC (pumps) + 5V/3.3V (logic) |

### Pin Assignments (config.h)

| Pump | GPIO |
|------|------|
| Pump 1 | 32 |
| Pump 2 | 33 |
| Pump 3 | 25 |
| Pump 4 | 26 |

Extend `defaultPins[]` and `PIN_PUMP_X` defines for >4 pumps.

## Software Architecture

```
Pro-Simple.ino       -- main setup/loop
├── config.h         -- compile-time configuration
├── types.h          -- shared structs/enums
├── pump.h/.cpp      -- peristaltic pump GPIO control
├── dosing.h/.cpp    -- dose scheduling & execution
├── scheduler.h/.cpp -- time-based schedule checking
├── web_server.h/.cpp -- HTTP server + dashboard HTML
├── dashboard.h/.cpp -- JSON API serialization
├── storage.h/.cpp   -- EEPROM persistence
└── logger.h/.cpp    -- circular log buffer
```

## Configuration (config.h)

**WiFi credentials are hardcoded** — edit `config.h` before flashing, or use AP mode at `192.168.4.1` (password: `config123`) if the WiFi connection fails.

| Define | Default | Description |
|--------|---------|-------------|
| `WIFI_SSID` | "homewifi" | WiFi network name |
| `WIFI_PASS` | "12345678" | WiFi password |
| `WIFI_TIMEOUT_MS` | 15000 | AP fallback timeout |
| `PUMP_COUNT` | 4 | Number of pump channels |
| `PUMP_DEFAULT_RATE` | 100.0 | mL/min flow rate |
| `MAX_SCHEDULES` | 16 | Total schedule limit |
| `HTTP_PORT` | 80 | Web server port |
| `EEPROM_SIZE` | 4096 | Persistent storage |

## Web Dashboard

Served at `http://<esp32-ip>/`. Features:

- **KPI Cards** - uptime, total dispensed, total doses, alarms
- **Pump Overview** - table with enable toggle, inline name edit, GPIO pin, rate, dosed, status, dose/prime buttons
- **Pump Count** - modal to set 1-8 pumps (dynamically updates UI)
- **Schedules** - per-pump sections with max 4/pump, add modal with day-chip selector
- **Reservoirs** - level bars matching pump count, inline name edit
- **Recent Activity** - live log feed from ESP32
- **Daily Usage** - bar chart placeholder
- **Quick Actions** - Dose All, E-Stop, Prime All, Reboot, Sync NTP, Set Time
- **Clock** - live 12-hour client-side clock
- **Timezone Selector** - 17 timezones, default PST
- **Dark/Light Theme** - toggle with persistent state

## API Endpoints

| Route | Method | Description |
|-------|--------|-------------|
| `/` | GET | Dashboard HTML |
| `/api?path=status` | GET | `{uptime, totalDoses, totalVolume, freeHeap, rssi, wifiConnected, ip}` |
| `/api?path=pumps` | GET | `[{index, name, rate, active, totalDosed, runTimeSec, state, pin}]` |
| `/api?path=schedules` | GET | `[{index, pumpIndex, hour, minute, doseML, enabled, days}]` |
| `/api?path=logs` | GET | `["[T] [L] msg", ...]` |
| `/api/dose?pump=X&vol=Y` | GET | Start dose (returns `{ok}`) |
| `/api/dose?pump=X&vol=0&cancel=1` | GET | Cancel dose |
| `/api/pump?pump=X&name=Y` | GET | Rename pump |
| `/api/pump?pump=X&active=true/false` | GET | Toggle pump enable |
| `/api/pump?pump=X&rate=Y` | GET | Set pump rate |
| `/api/schedule?pump=X&hour=H&minute=M&vol=V&days=D` | GET | Add schedule |
| `/api/schedule/remove?index=X` | GET | Remove schedule |
| `/api/reset` | GET | Reset all totals |
| `/api/ntp` | GET | Trigger NTP sync |

State codes: `0=Idle, 1=Priming, 2=Dosing, 3=Complete, 4=Error`

Day bitmask: `Sun=1, Mon=2, Tue=4, Wed=8, Thu=16, Fri=32, Sat=64`

## Build & Flash

### PlatformIO (recommended)
```ini
[env:esp32-s3-dev]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
```

### Arduino IDE
1. Install ESP32 board package (Tools > Board > Boards Manager)
2. Select `ESP32S3 Dev Module`
3. Open `Pro-Simple.ino`
4. Edit `config.h` with your WiFi credentials
5. Upload

### Serial Monitor
Baud: 115200

## File Layout

```
Pro-Simple/
├── Pro-Simple.ino      -- main sketch
├── config.h            -- WiFi/pin/rate config
├── types.h             -- PumpConfig, Schedule, SystemStatus, DoseState
├── pump.h / pump.cpp   -- GPIO pump control
├── dosing.h / dosing.cpp -- dose lifecycle
├── scheduler.h / scheduler.cpp -- schedule checking
├── web_server.h / web_server.cpp -- HTTP + dashboard HTML
├── dashboard.h / dashboard.cpp -- JSON API
├── storage.h / storage.cpp -- EEPROM persistence
├── logger.h / logger.cpp -- circular log buffer
├── previews/           -- HTML design mockups
│   └── 11-corporate-dark.html -- final design reference
└── docs/
    └── BUILD.md        -- this file
```

## Notes

- Schedules are stored in EEPROM with CRC16 validation
- WiFi AP mode at `192.168.4.1` if station connection fails (password: `config123`)
- Dashboard uses system font stack (no external resources)
- Polling interval: 5 seconds (dashboard JS)
- Clock is client-side only; timezone selector is cosmetic
