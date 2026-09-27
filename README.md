# ESP32-Renogy-LoRa

A standalone LoRa transmitter that reads data from a Renogy solar charge controller via RS232 Modbus and transmits it wirelessly to an OpenMQTTGateway (OMG) LoRa gateway. Built for remote monitoring where WiFi is unavailable or impractical.

Tested with:
- Renogy Wonderer 10A PWM (RNG-CTRL-WND10)
- Renogy Rover 20A MPPT

Should work with any Renogy charge controller that has an RS232 RJ12 port.

> Note: This project covers RS232 controllers only. Renogy also makes controllers with RS485 ports, which use a different converter (MAX485) and require DE/RE pin toggling in the firmware. The Modbus registers and LoRa transmission code should be largely the same — community contributions for RS485 support are welcome.

---

## How It Works

The ESP32 polls the charge controller every 60 seconds via Modbus RTU over RS232 (through a MAX3232 level converter), builds a JSON payload, and transmits it via LoRa. The OMG gateway receives the packet and publishes it to MQTT, where it can be picked up by Node-RED, InfluxDB, and Grafana.

```
Renogy controller → RJ12/RS232 → MAX3232 → ESP32 → RFM95W → LoRa RF → OMG gateway → MQTT
```

---

## Hardware

- ESP32 DevKit1
- Adafruit RFM95W LoRa transceiver (915MHz)
- MAX3232 TTL/RS232 converter board — [(Part I used)](https://www.amazon.com/dp/B091TN2ZPY?ref=ppx_yo2ov_dt_b_fed_asin_title)
- MP1584EN DC-DC buck converter — Adjusted to step Renogy RJ12 voltage down to 5V to power the ESP32 [(Part I used)](https://www.amazon.com/dp/B01MQGMOKI?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_2&th=1)
- RJ12 6-wire cable — [(One like this cut in two)](https://www.amazon.com/dp/B0F9YVVG77?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1)

Related:
- OMG LoRa Gateway — [(LILYGO LoRa32 915MHz ESP32 Development Board)](https://www.amazon.com/LILYGO-LoRa32-433Mhz-Development-Paxcounter/dp/B09SHRWVNB/ref=sr_1_1?dib=eyJ2IjoiMSJ9.AtA4SX5sQMRx9EvaArXKy60QZlmmxk9hImRj-x_Qk8rvBFpeO9muThruuULU-846Vx-3iCq7dWMWCl_yu2j2Khe3-p4Iab3rjYUMJ7dX-M3n50LaZSgEfWAGOmWnz7vc_I-ep0rsEZm0i6qEFLWm9ylQfEWX7wuf_1JmnFbK5WCITDXlXins-bcn0Slu2RqrZP-2AnFuwnji3k1hDXQRNW7JcEHHeEA6zz5iRrNZ62s.xW7aiOYN-sqfYSBU11yUh3eUZPZGGErpU2juy_AMUO0&dib_tag=se&keywords=ttgo%2Besp32%2Blora&qid=1775087725&sr=8-1&th=1)

---

## Wiring

### RJ12 to MAX3232 (RS232 side)
Pins counted right-to-left with contacts facing you.

| RJ12 Pin | Signal | MAX3232 RS232 side |
|----------|--------|--------------------|
| Pin 1 | Controller TX | RXD |
| Pin 2 | Controller RX | TXD |
| Pin 3 | GND | GND |
| Pin 4 | GND | GND |
| Pin 5 | PWR | +11–15V * |
| Pin 6 | PWR | +11–15V * |

\* ~11V on Wonderer 10A, ~15V on Rover 20A

> ⚠️ Never connect RS232 lines directly to the ESP32 — the voltage levels will damage it. Always use the MAX3232 converter.

> ⚠️ Never plug a USB cable from your computer into the ESP32 for programming while the board is also powered from the controller's RJ12 port. This could damage your computer.

### MAX3232 (TTL side) to ESP32

| MAX3232 TTL | ESP32 GPIO |
|-------------|------------|
| TX | GPIO 16 (RX2) |
| RX | GPIO 17 (TX2) |
| VCC | 3.3V |
| GND | GND |

### RFM95W to ESP32

| RFM95W | ESP32 GPIO |
|--------|------------|
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| SCK | GPIO 18 |
| CS | GPIO 5 |
| RST | GPIO 14 |
| G0 (DIO0) | GPIO 2 |
| VIN | 3.3V |
| GND | GND |

> Note: The pin labeled **G0** on the Adafruit RFM95W breakout is DIO0. EN can be left unconnected.

---

## Power

Both controllers have been tested using their RJ12 RS232 port (pins 4–6) to power the ESP32 via a buck converter:

| Controller | RJ12 Voltage | Notes |
|------------|--------------|-------|
| Wonderer 10A | ~11.3V | Step down to 5V with buck converter |
| Rover 20A | ~15.1V | Step down to 5V with buck converter |

> ⚠️ Do NOT connect RJ12 pins 4–6 directly to the ESP32 — the voltage will damage it. Always use a buck converter to step down to 5V first.

---

## Modbus Settings

Both controllers tested at:
- **Baud rate:** 9600
- **Slave address:** 0xFF

> Note: The default documented slave address for Renogy controllers is 0x01, but both units tested here responded to 0xFF. If you get no response, use the included scan utility to find your controller's address.

---

## LoRa Settings

Matched to an existing OpenMQTTGateway LoRa gateway:

| Setting | Value |
|---------|-------|
| Frequency | 915 MHz |
| Spreading Factor | SF7 |
| Bandwidth | 125 kHz |
| Sync Word | 0x12 |

---

## MQTT Output

The transmitter publishes to your OMG gateway, which forwards to MQTT. OMG looks for the `"value"` field in the payload to create a dedicated subtopic automatically (the name is set in `main.cpp` — change as desired):

**Topic:**
```
OMGhome/OMG_ESP32_LORA/LORAtoMQTT/renogy_wonderer
```

**Payload example:**
```json
{
  "value": "renogy_wonderer",
  "id": "renogy_wonderer",
  "batt_v": 13.2,
  "batt_a": 0.09,
  "soc": 100,
  "solar_v": 13.6,
  "solar_a": 0.07,
  "solar_w": 1,
  "batt_t": 0,
  "ctrl_t": 25,
  "solar_w_max": 51,
  "solar_w_min": 0,
  "batt_v_max": 14.0,
  "batt_v_min": 13.1,
  "rssi": -79,
  "snr": 9.75,
  "pferror": 226,
  "packetSize": 220
}
```

> Note: `batt_t` will always read 0 on the Wonderer 10A as it has no external battery temperature sensor connection. The Rover 20A returned a value even without a temperature probe attached.

---

## PlatformIO Setup

**platformio.ini:**
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    sandeepmistry/LoRa @ ^0.8.0
    4-20ma/ModbusMaster @ ^2.0.1
```

Set the device ID in `main.cpp` to match your controller:
```cpp
// For Wonderer 10A:
#define DEVICE_ID "renogy_wonderer"

// For Rover 20A:
#define DEVICE_ID "renogy_rover20"
```

---

## Scan Utility

If your controller doesn't respond, use the included scan utility (`scan.cpp`) to find the correct baud rate and Modbus address. It tries the most common combinations first before falling back to a full 0x00–0xFF sweep.

---

## Node-RED / InfluxDB / Grafana

> 🔧 To be documented — Node-RED flow to subscribe to the MQTT topic, parse the payload, and write fields to InfluxDB for display in a Grafana dashboard.

---

## Home Assistant Integration

Since the data arrives via MQTT as clean JSON, it can be added to Home Assistant using MQTT sensors in `configuration.yaml`. Add one entry per field:

```yaml
mqtt:
  sensor:
    - name: "Renogy Battery Voltage"
      state_topic: "OMGhome/OMG_ESP32_LORA/LORAtoMQTT/renogy_wonderer"
      value_template: "{{ value_json.batt_v }}"
      unit_of_measurement: "V"

    - name: "Renogy SOC"
      state_topic: "OMGhome/OMG_ESP32_LORA/LORAtoMQTT/renogy_wonderer"
      value_template: "{{ value_json.soc }}"
      unit_of_measurement: "%"
```

Repeat for each field (`solar_v`, `solar_w`, `ctrl_t`, `solar_w_max`, `batt_v_max`, etc.), adjusting the topic for the Rover if needed. Restart Home Assistant after saving.

> Note: Your MQTT broker must be configured in Home Assistant first. See the [HA MQTT integration docs](https://www.home-assistant.io/integrations/mqtt/) for details.

---

## Notes

- The Wonderer 10A responds to Modbus at 2400 baud in some configurations and 9600 in others — if 9600 fails, try 2400.
- Some sources report that the Wonderer 10A cannot supply enough power for an ESP32 from its RJ12 port. The unit purchased in 2026 worked fine.
- Signal range tested at -79 RSSI at the far end of a residential yard using a small coil antenna oriented horizontally. A vertical antenna will improve this.
- The 60-second poll interval is conservative and well within LoRa duty cycle limits.

---

## Related Projects

- [wrybread/ESP32ArduinoRenogy](https://github.com/wrybread/ESP32ArduinoRenogy) — inspiration for the Modbus register approach
- [OpenMQTTGateway](https://github.com/1technophile/OpenMQTTGateway) — the LoRa gateway firmware
