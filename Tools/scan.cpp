// Renogy Modbus scan utility - XIAO ESP32-S3 version
// Finds the baud rate and Modbus address your controller answers on.
// Tries the most common addresses first, then a full 0x01-0xFF sweep.

#include <Arduino.h>
#include <ModbusMaster.h>

// ---- Renogy UART pins (same as main.cpp) ----
#define RENOGY_TX   43   // D6 -> MAX3232 T1IN
#define RENOGY_RX   44   // D7 <- MAX3232 R1OUT

ModbusMaster node;

// Try one address at the current baud. Returns true if the controller answers.
bool tryAddress(uint8_t addr) {
  node.begin(addr, Serial1);
  uint8_t result = node.readHoldingRegisters(0x0100, 1);
  delay(100);
  return (result == node.ku8MBSuccess);
}

void reportFound(long baud, uint8_t addr) {
  Serial.println();
  Serial.print("*** FOUND IT! Baud: ");
  Serial.print(baud);
  Serial.print("  Address: 0x");
  Serial.println(addr, HEX);
  Serial.println("Scan stopped. Put these values in main.cpp.");
}

void setup() {
  Serial.begin(115200);
  delay(3000);  // give the USB Serial Monitor time to connect

  Serial.println("Renogy Modbus scan starting...");

  long bauds[] = {9600, 2400, 4800};
  uint8_t commonAddrs[] = {0xFF, 0x01, 0x60, 0x0B};

  // ---- Pass 1: common addresses at each baud (quick, ~30 seconds) ----
  Serial.println("Pass 1: common addresses");
  for (long baud : bauds) {
    Serial1.end();
    Serial1.begin(baud, SERIAL_8N1, RENOGY_RX, RENOGY_TX);
    delay(100);
    Serial.print("  Baud ");
    Serial.println(baud);

    for (uint8_t addr : commonAddrs) {
      if (tryAddress(addr)) {
        reportFound(baud, addr);
        return;
      }
    }
  }

  // ---- Pass 2: full sweep (slow - roughly 9 minutes per baud rate) ----
  Serial.println("Pass 2: full sweep 0x01-0xFF (this takes a while)");
  for (long baud : bauds) {
    Serial1.end();
    Serial1.begin(baud, SERIAL_8N1, RENOGY_RX, RENOGY_TX);
    delay(100);
    Serial.print("  Baud ");
    Serial.println(baud);

    for (int addr = 0x01; addr <= 0xFF; addr++) {
      if (tryAddress(addr)) {
        reportFound(baud, addr);
        return;
      }
      if (addr % 16 == 0) {           // progress dot every 16 addresses
        Serial.print(".");
      }
    }
    Serial.println();
  }

  Serial.println("Scan complete - no response found. Check wiring (try swapping TX/RX).");
}

void loop() {}
