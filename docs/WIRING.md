# Wiring Guide

## Driver Options

| Method | Pros | Cons | Best For |
|--------|------|------|----------|
| **8-CH MOSFET module** | Optocoupler isolated, PWM, clean | Bulkier than bare MOSFETs | **Recommended** for Pro-Simple |
| **Bare MOSFETs** | Cheapest, smallest | Soldering, no isolation | Budget builds |
| **Relay module** | No soldering, isolated | No PWM, click noise, large | On/off only |

---

## 8-Channel MOSFET Module (Recommended)

This wiring assumes the common **8-CH MOSFET module with optocoupler isolation** (13 x 9 x 8 cm). Optocouplers protect the ESP32 from voltage spikes and noise.

### Module Pin Layout

```
    IN1  IN2  IN3  IN4  IN5  IN6  IN7  IN8     ←  ESP32 GPIO input
    ┌────┬────┬────┬────┬────┬────┬────┬────┐
    │ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │ 7  │ 8  │   ←  MOSFET channels
    ├────┴────┴────┴────┴────┴────┴────┴────┤
    │          8-CH MOSFET MODULE           │
    └────┬────┬────┬────┬────┬────┬────┬────┘
    VCC  GND  VCC  OUT1 OUT2 OUT3 OUT4 OUT5 OUT6 OUT7 OUT8
     │    │    │    │    │    │    │    │    │
     │    │    │    │    │    │    │    │    │
     │    │    │    │    │    │    │    │    │
    +5V  GND  +5V  ─┘  ─┘  ─┘  ─┘  ─┘  ─┘  ─┘  ← Outputs to pumps
         │
        GND
```

> **Check your module** — some modules have VCC/GND on the input side AND a separate VCC/GND on the output/power side. Connect input VCC to ESP32 3.3V (or 5V if 5V version), and output VCC to the pump power supply.

### ESP32 → Module Wiring

```
    ESP32                    8-CH MOSFET Module
    ┌──────────────┐         ┌──────────────┐
    │ GPIO 32  ────┼────────→│ IN1          │
    │ GPIO 33  ────┼────────→│ IN2          │
    │ GPIO 25  ────┼────────→│ IN3          │
    │ GPIO 26  ────┼────────→│ IN4          │
    │ GPIO 27  ────┼────────→│ IN5          │
    │ GPIO 14  ────┼────────→│ IN6          │
    │ GPIO 12  ────┼────────→│ IN7          │
    │ GPIO 13  ────┼────────→│ IN8          │
    │              │         │              │
    │ GND      ────┼────────→│ GND          │
    └──────────────┘         └──────────────┘
```

### Module → Pump Wiring

```
    Module OUT1  ────────→  Pump 1 (-)
    Module OUT2  ────────→  Pump 2 (-)
    Module OUT3  ────────→  Pump 3 (-)
    Module OUT4  ────────→  Pump 4 (-)
    Module OUT5  ────────→  Pump 5 (-)
    Module OUT6  ────────→  Pump 6 (-)
    Module OUT7  ────────→  Pump 7 (-)
    Module OUT8  ────────→  Pump 8 (-)

    Module VCC  ────────→  +12V/+24V power supply (+)

    All Pump (+) terminals ──→  +12V/+24V power supply (+)
    Power supply (-)        ──→  Common GND with ESP32
```

### Pin Mapping (8-channel)

| Pump | GPIO | config.h Define | Module IN |
|------|------|-----------------|-----------|
| Pump 1 | GPIO 32 | `PIN_PUMP_1` | IN1 |
| Pump 2 | GPIO 33 | `PIN_PUMP_2` | IN2 |
| Pump 3 | GPIO 25 | `PIN_PUMP_3` | IN3 |
| Pump 4 | GPIO 26 | `PIN_PUMP_4` | IN4 |
| Pump 5 | GPIO 27 | — | IN5 |
| Pump 6 | GPIO 14 | — | IN6 |
| Pump 7 | GPIO 12 | — | IN7 |
| Pump 8 | GPIO 13 | — | IN8 |

Also set `#define PUMP_COUNT 8` in `config.h`.

> **GPIO 12 note:** This is a strapping pin on ESP32. If using GPIO 12, add a 10kΩ pull-down resistor to GND and ensure it reads LOW at boot.

### Module VCC Selection

| Module Version | Connect VCC to | Notes |
|----------------|----------------|-------|
| **3.3V** | ESP32 3.3V | Direct GPIO control, safest |
| **5V** | ESP32 5V (USB) or ext 5V | More noise margin |

Optocouplers on the input side mean the ESP32 GPIO drives the optocoupler LED, not the MOSFET directly. This isolates the ESP32 electrically from the pump power supply.

---

## Wiring Without MOSFET Module (Relay Module)

For on/off-only control, use an optocoupler relay module:

```
    ESP32 GPIO 32 ────────→  Relay IN 1
    ESP32 GPIO 33 ────────→  Relay IN 2
    ESP32 GND     ────────→  Relay GND
    ESP32 5V      ────────→  Relay VCC

    Relay COM 1  ────────→  +12V/+24V power supply (+)
    Pump 1 (+)   ────────→  Relay NO 1
    Pump 1 (-)   ────────→  Power supply (-)
```

> **Active-LOW:** Many relay modules turn ON when IN is LOW. Check your module's logic and adjust `pump.cpp` if needed.

---

## Power Supply

| Component | Voltage | Current |
|-----------|---------|---------|
| ESP32 | 5V (USB) or 3.3V (VIN) | ~200mA |
| Peristaltic pump (typical) | 12V / 24V | 100-500mA each |
| 8-CH MOSFET module | 5V input, pump voltage output | 0mA (signal only) |

- Use a **single 12V or 24V supply** for all pumps
- Power the ESP32 separately (USB) or through a 5V regulator from the pump supply
- **DO NOT** power pumps from the ESP32 3.3V pin
- For 8 pumps at 500mA each, a **24V 12A** supply provides comfortable headroom

## Quick Checklist

- [ ] Module VCC connected to correct voltage (3.3V or 5V version)
- [ ] Common GND between ESP32, MOSFET module, and pump power supply
- [ ] Pump (+) terminals all tied to power supply (+)
- [ ] Module OUT connected to pump (-) terminals
- [ ] Pump voltage matches power supply rating
- [ ] `PUMP_COUNT` in `config.h` matches wired channels
- [ ] GPIO pins in `config.h` and `defaultPins[]` match wiring
