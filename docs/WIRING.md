# Wiring Guide

## Per-Channel Components (MOSFET)

| Part | Purpose |
|------|---------|
| IRF520 / IRLZ44N / RFP30N06LE | N-channel MOSFET (logic-level gate) |
| 1kΩ resistor | Gate current limit |
| 10kΩ resistor | Gate pull-down (optional, keeps pump off during boot) |
| 1N4007 / 1N5408 | Flyback diode across pump terminals |

## Driver Comparison

| Method | Pros | Cons | Best For |
|--------|------|------|----------|
| **MOSFET** | PWM-capable, low heat, small | Needs basic soldering | Most peristaltic pumps |
| **Relay module** | No soldering, isolated | Click noise, no PWM, bulkier | On/off only, high current |
| **Pre-built board** | Clean, tested | Cost, availability | Production builds |

---

## 4-Channel MOSFET Wiring

```
                        +12V / +24V DC
                             │
             ┌───────────────┼───────────────────────┐
             │               │                       │
        PUMP 1 (+)    PUMP 2 (+)    ...         PUMP 4 (+)
             │               │                       │
           ┌─┴─┐           ┌─┴─┐                   ┌─┴─┐
           │ D │           │ D │                   │ D │
           └─┬─┘           └─┬─┘                   └─┬─┘
  1N4007  ┌──┤              │                        │
  (cathode) │              │                        │
           ├──┘             ├──┘                     ├──┘
           │   Drain        │   Drain                │   Drain
        ┌──┴──┐          ┌──┴──┐                  ┌──┴──┐
        │ IRF │          │ IRF │                  │ IRF │
        │ 520 │          │ 520 │     ...           │ 520 │
        └──┬──┘          └──┬──┘                  └──┬──┘
           │   Source       │   Source               │   Source
           ├──┐             ├──┐                     ├──┐
           │  │             │  │                     │  │
           │  └──GND────────┼──┼─────GND─────────────┼──┼──GND
           │                │                        │
      ┌────┴────┐      ┌────┴────┐              ┌────┴────┐
      │ 1kΩ     │      │ 1kΩ     │              │ 1kΩ     │
      └────┬────┘      └────┬────┘              └────┬────┘
           │ Gate           │ Gate                  │ Gate
           │                │                        │
      ┌─────┴─────┐   ┌─────┴─────┐            ┌─────┴─────┐
      │  ESP32    │   │  ESP32    │            │  ESP32    │
      │  GPIO 32  │   │  GPIO 33  │            │  GPIO 26  │
      └───────────┘   └───────────┘            └───────────┘


    ESP32 GND ──────────┴──────────┴──────────────┴─── GND rail

    Power supply (-) ───────────────────────────────── GND rail
```

### Pin Mapping (4-channel)

| Pump | GPIO | config.h Define |
|------|------|-----------------|
| Pump 1 | GPIO 32 | `PIN_PUMP_1` |
| Pump 2 | GPIO 33 | `PIN_PUMP_2` |
| Pump 3 | GPIO 25 | `PIN_PUMP_3` |
| Pump 4 | GPIO 26 | `PIN_PUMP_4` |

---

## 8-Channel MOSFET Wiring

Same circuit as above, expanded to 8 channels. Add pins in `config.h`:

| Pump | GPIO | Add to `defaultPins[]` in `web_server.cpp` |
|------|------|---------------------------------------------|
| Pump 1 | GPIO 32 | `32` |
| Pump 2 | GPIO 33 | `33` |
| Pump 3 | GPIO 25 | `25` |
| Pump 4 | GPIO 26 | `26` |
| Pump 5 | GPIO 27 | `27` |
| Pump 6 | GPIO 14 | `14` |
| Pump 7 | GPIO 12 | `12` |
| Pump 8 | GPIO 13 | `13` |

Also set `#define PUMP_COUNT 8` in `config.h`.

> **Note:** GPIO 12 is a strapping pin on ESP32 — it affects boot voltage. If using GPIO 12, add a 10kΩ pull-down resistor to GND and ensure it reads LOW at boot.

---

## Wiring Without MOSFET (Relay Module)

For on/off-only control, use an optocoupler relay module:

```
                     +5V ──── Relay VCC
                     GND ──── Relay GND

    ESP32 GPIO 32 ──┬─── Relay IN 1
                     │
                    1kΩ
                     │
                    GND

    ESP32 GPIO 33 ──┬─── Relay IN 2
                     │
                    1kΩ
                     │
                    GND

    ... repeat for each pump

    Relay COM ──── +12V/+24V power supply (+)

    Pump 1 (+) ──── Relay NO 1
    Pump 1 (-) ──── Power supply (-)

    Pump 2 (+) ──── Relay NO 2
    Pump 2 (-) ──── Power supply (-)
```

> **Important:** Many relay modules are active-LOW (input LOW = relay ON). Set `active` to match in the dashboard or invert logic in `pump.cpp` if needed.

---

## Power Supply

| Component | Voltage | Current |
|-----------|---------|---------|
| ESP32 | 5V (USB) or 3.3V (VIN) | ~200mA |
| Peristaltic pump (typical) | 12V / 24V | 100-500mA |
| Relay module (if used) | 5V | ~50mA per relay |

- Use a **single 12V or 24V supply** for all pumps
- Power the ESP32 separately (USB) or through a 5V regulator from the pump supply
- **DO NOT** power pumps from the ESP32 3.3V pin

## Quick Checklist

- [ ] Common GND between ESP32, MOSFETs/relays, and pump power supply
- [ ] Flyback diode across each pump (cathode to +) when using MOSFETs
- [ ] 1kΩ resistor between ESP32 GPIO and MOSFET gate
- [ ] Pump voltage matches power supply rating
- [ ] `PUMP_COUNT` in `config.h` matches wired channels
- [ ] GPIO pins in `config.h` and `defaultPins[]` match wiring
