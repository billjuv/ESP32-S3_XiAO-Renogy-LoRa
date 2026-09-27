#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>        // CHANGED: RadioLib replaces sandeepmistry LoRa (SX1262 needs RadioLib)
#include <ModbusMaster.h>
#include <esp_task_wdt.h>    // hardware watchdog

// ---- LoRa pins (Seeed XIAO ESP32S3 + Wio-SX1262 B2B Kit) ----
// Same pins as your XIAO/SCD41 and XIAO/SHT31 units.
// SPI uses the XIAO's default pins (SCK=7, MISO=8, MOSI=9), so no SPI.begin() needed.
SX1262 radio = new Module(41, 39, 42, 40); // NSS, DIO1, RST, BUSY

// ---- Renogy UART pins (XIAO D6 / D7) ----
// On the XIAO S3, "Serial" is the USB-C port, so the Renogy gets its own
// hardware UART (Serial1) mapped onto D6/D7.
#define RENOGY_TX   43   // D6 -> MAX3232 T1IN
#define RENOGY_RX   44   // D7 <- MAX3232 R1OUT
#define RENOGY_ADDR 0xFF

// ---- Device ID - change to match your controller ----
#define DEVICE_ID   "renogy_wonderer"
//#define DEVICE_ID "renogy_rover20"

// ---- How often to send (milliseconds) ----
#define SEND_INTERVAL 59000

// ---- Watchdog timeout (seconds) ----
// Must comfortably cover the worst-case time a single loop() pass could
// take (a Modbus read + retry + LoRa send), but be far shorter than
// SEND_INTERVAL so a genuinely stuck loop gets caught quickly.
#define WDT_TIMEOUT_S 20

ModbusMaster node;

// Attempts a Modbus read, retrying once on failure.
// Returns the ModbusMaster result code from the last attempt.
uint8_t readRenogyRegisters() {
  uint8_t result = node.readHoldingRegisters(0x0100, 17);
  if (result != node.ku8MBSuccess) {
    Serial.print("Modbus read failed (0x");
    Serial.print(result, HEX);
    Serial.println("), retrying once...");
    delay(250);
    result = node.readHoldingRegisters(0x0100, 17);
  }
  return result;
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // NEW: give the USB serial port time to connect so you see startup messages

  // CHANGED: Serial1 on D6/D7 instead of Serial2 on GPIO16/17
  Serial1.begin(9600, SERIAL_8N1, RENOGY_RX, RENOGY_TX);
  node.begin(RENOGY_ADDR, Serial1);

  Serial.println("Starting Renogy LoRa transmitter (XIAO S3)...");

  // CHANGED: RadioLib init with the same settings as your other XIAO units
  // (these must match the OpenMQTTGateway settings)
  int state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("LoRa init FAILED, code: %d\n", state);
    while (true);
  }
  radio.setFrequency(915.0);
  radio.setOutputPower(14);
  radio.setSpreadingFactor(7);
  radio.setBandwidth(125.0);
  radio.setCodingRate(5);
  radio.setPreambleLength(8);
  radio.setSyncWord(0x12);
  radio.setCRC(true);
  Serial.println("LoRa ready.");

  // Arm the hardware watchdog. If loop() ever fails to come back
  // around within WDT_TIMEOUT_S seconds (e.g. a Modbus/UART read that
  // never returns), the ESP32 reboots itself automatically.
  esp_task_wdt_init(WDT_TIMEOUT_S, true);  // true = reboot on timeout
  esp_task_wdt_add(NULL);                  // watch the current (loop) task
  Serial.println("Watchdog armed.");
}

void loop() {
  // Feed the watchdog every pass.
  esp_task_wdt_reset();

  static unsigned long lastSend = 0;

  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();

    uint8_t result = readRenogyRegisters();

    if (result == node.ku8MBSuccess) {

      int   soc        = node.getResponseBuffer(0x00);
      float battV      = node.getResponseBuffer(0x01) / 10.0;
      float battA      = node.getResponseBuffer(0x02) / 100.0;

      uint16_t tempRaw = node.getResponseBuffer(0x03);
      int battTemp     = tempRaw & 0x7F;
      if ((tempRaw & 0x0080) >> 7)  battTemp = -battTemp;
      int ctrlTemp     = (tempRaw & 0x7F00) >> 8;
      if ((tempRaw & 0x8000) >> 15) ctrlTemp = -ctrlTemp;

      float solarV     = node.getResponseBuffer(0x07) / 10.0;
      float solarA     = node.getResponseBuffer(0x08) / 100.0;
      int   solarW     = node.getResponseBuffer(0x09);

      float battVMin   = node.getResponseBuffer(0x0B) / 10.0;
      float battVMax   = node.getResponseBuffer(0x0C) / 10.0;
      int   solarWMax  = node.getResponseBuffer(0x0F);
      int   solarWMin  = node.getResponseBuffer(0x10);

      String payload = "{";
      payload += "\"value\":\""  + String(DEVICE_ID) + "\",";
      payload += "\"id\":\""     + String(DEVICE_ID) + "\",";
      payload += "\"batt_v\":"   + String(battV,   1) + ",";
      payload += "\"batt_a\":"   + String(battA,   2) + ",";
      payload += "\"soc\":"      + String(soc)         + ",";
      payload += "\"solar_v\":"  + String(solarV,  1) + ",";
      payload += "\"solar_a\":"  + String(solarA,  2) + ",";
      payload += "\"solar_w\":"  + String(solarW)      + ",";
      payload += "\"batt_t\":"   + String(battTemp)    + ",";
      payload += "\"ctrl_t\":"   + String(ctrlTemp)    + ",";
      payload += "\"solar_w_max\":" + String(solarWMax)   + ",";
      payload += "\"solar_w_min\":" + String(solarWMin)   + ",";
      payload += "\"batt_v_max\":" + String(battVMax, 1)  + ",";
      payload += "\"batt_v_min\":" + String(battVMin, 1);
      payload += "}";

      Serial.println("Sending: " + payload);

      // CHANGED: RadioLib transmit (blocking, returns when the packet is out)
      int txState = radio.transmit(payload);
      if (txState == RADIOLIB_ERR_NONE) {
        Serial.println("Sent.");
      } else {
        Serial.printf("LoRa send FAILED, code: %d\n", txState);
      }

    } else {
      Serial.print("Modbus error: 0x");
      Serial.println(result, HEX);
    }
  }
}