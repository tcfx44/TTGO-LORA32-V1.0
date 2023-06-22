#include <SPI.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <LoRa.h>
#include <pins_arduino.h>
#include "images.h"

//#define LORA_BAND    433
#define LORA_BAND    868
//#define LORA_BAND    915

// How long between transmissions
const int TRANSMISSION_PERIOD_MS = 1000;

// Global variable declarations
SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL);

volatile int counter = 0;
TaskHandle_t pktDisplayTask;

// Forward method declarations
void pktDisplay(void *parm);
void displayLoraData();
void showLogo();

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println();
  Serial.println("LoRa P2P Transmitter");

  pinMode(LED_BUILTIN, OUTPUT);

  // Create a new task on the second core to blink the LED when packets
  // are sent.  After creation, give it a bit of time to startup
  xTaskCreatePinnedToCore(
    pktDisplay,
    "pktDisplayTask",
    4096,
    NULL,
    1,
    &pktDisplayTask,
    1);
  delay(500);

  // // Configure OLED by setting the OLED Reset HIGH, LOW, and then back HIGH
  // pinMode(OLED_RST, OUTPUT);
  // digitalWrite(OLED_RST, HIGH);
  // delay(100);
  // digitalWrite(OLED_RST, LOW);
  // delay(100);
  // digitalWrite(OLED_RST, HIGH);

  display.init();
  display.flipScreenVertically();

  // LoRa image
  showLogo();
  delay(2000);

  // Indicate which function this device is running
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  int centerWidth = display.getWidth() / 2;
  int centerHeight = display.getHeight() / 2;
  display.drawString(centerWidth, 0, "LoRa");
  display.drawString(centerWidth, centerHeight, "P2P Transmitter");
  display.display();
  delay(2000);

  // Configure the LoRA radio
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
  if (!LoRa.begin(LORA_BAND * 1E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  // See https://github.com/sandeepmistry/arduino-LoRa/blob/master/API.md
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125 * 1000);
  LoRa.setCodingRate4(4);
  LoRa.setPreambleLength(8);
  LoRa.setSyncWord(0x34);
  LoRa.enableCrc();
  Serial.println("LoRa init");
}

void loop() {
  static unsigned long lastTimeSent = 0;

  // Send a packet once/per second
  if (millis() - lastTimeSent >= TRANSMISSION_PERIOD_MS) {
    lastTimeSent = millis();

    // send packet
    counter++;
    LoRa.beginPacket();
    LoRa.print("hello ");
    LoRa.print(counter);
    LoRa.endPacket();

    Serial.println(String(counter, DEC));

    // Wakeup the display task. We could do directly instead of in
    // a separate task since loop is called by the standard thread
    // context, but to synchronize both the transmitter and receiver
    // code and to ensure that we send a packet once/sec, we use
    // a secondary task for all visual transmission indications.
    vTaskNotifyGiveFromISR(pktDisplayTask, NULL);
  }
}

// Use the second core to blink the LED when we receive a packet
void pktDisplay(void *parm) {
  static const int BLINK_TIME_MS = 250;

  while (1) {
    // Wait for a notification
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Show the data on the screen
    displayLoraData();

    // toggle the led to give a visual indication the packet was sent
    digitalWrite(LED_BUILTIN, HIGH);
    vTaskDelay(BLINK_TIME_MS/portTICK_PERIOD_MS);
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void displayLoraData() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  display.drawString(0, 0, "Sending packet: ");
  display.drawString(90, 0, String(counter, DEC));
  display.display();
}

void showLogo() {
  uint8_t x_off = (display.getWidth() - logo_width) / 2;
  uint8_t y_off = (display.getHeight() - logo_height) / 2;

  display.clear();
  display.drawXbm(x_off, y_off, logo_width, logo_height, logo_bits);
  display.display();
}
