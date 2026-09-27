#include <Arduino.h>
#include <ModbusMaster.h>

#define RXD2 16
#define TXD2 17

ModbusMaster node;

void setup() {
  Serial.begin(115200);
  Serial.println("Scanning all addresses...");

  long bauds[] = {9600, 2400, 4800};
  uint8_t addrs[] = {0xFF, 0x01, 0x00, 0x60, 0x0B};

  for (long baud : bauds) {
    Serial.print("Trying baud: ");
    Serial.println(baud);
    Serial2.begin(baud, SERIAL_8N1, RXD2, TXD2);
    delay(100);
    
    for (int addr = 0; addr <= 255; addr++) {
      node.begin(addr, Serial2);
      uint8_t result = node.readHoldingRegisters(0x0100, 1);
      if (result == node.ku8MBSuccess) {
        Serial.print("*** FOUND IT! Baud: ");
        Serial.print(baud);
        Serial.print(" Address: 0x");
        Serial.println(addr, HEX);
      }
      delay(100);
    }
  }
  Serial.println("Scan complete.");
}

void loop() {}