/**

*/


//DEVICE ESP32C3 Dev Module where ST7790 connected

#define ST7789_DISPLAY
#include <Arduino.h>
#include <SensirionI2cScd4x.h>
#include <Wire.h>
#include <WiFi.h>
#include <ThingSpeak.h>

#include "secret.h"

// ThingSpeak channel details
unsigned long myChannelNumber = CHANNEL_ID;
const char* myWriteAPIKey = API_KEY;

WiFiClient client;

SensirionI2cScd4x scd41_sensor;
#define SD41_I2C_SDA 7
#define SD41_I2C_SCL 9
static char errorMessage[64];
static int16_t error;

// macro definitions
// make sure that we use the proper definition of NO_ERROR
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

#ifdef ST7789_DISPLAY
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <SPI.h>
#define TFT_CS 10
#define TFT_RST 0  // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC 1
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
#endif

int i = 0;
uint16_t co2Concentration = 0;
float temperature = 0.0;
float relativeHumidity = 0.0;

void PrintUint64(uint64_t& value) {
  Serial.print("0x");
  Serial.print((uint32_t)(value >> 32), HEX);
  Serial.print((uint32_t)(value & 0xFFFFFFFF), HEX);
}

void initSCD41(void) {
  scd41_sensor.begin(Wire, SCD41_I2C_ADDR_62);

  uint64_t serialNumber = 0;
  delay(30);
  // Ensure sensor is in clean state
  error = scd41_sensor.wakeUp();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute wakeUp(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }
  error = scd41_sensor.stopPeriodicMeasurement();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }
  error = scd41_sensor.reinit();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute reinit(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }
  // Read out information about the sensor
  error = scd41_sensor.getSerialNumber(serialNumber);
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }
  Serial.print("serial number: ");
  PrintUint64(serialNumber);
  Serial.println();
  //
  // If temperature offset and/or sensor altitude compensation
  // is required, you should call the respective functions here.
  // Check out the header file for the function definitions.
  // Start periodic measurements (5sec interval)
  error = scd41_sensor.startPeriodicMeasurement();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute startPeriodicMeasurement(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }
  //
  // If low-power mode is required, switch to the low power
  // measurement function instead of the standard measurement
  // function above. Check out the header file for the definition.
  // For SCD41, you can also check out the single shot measurement example.
  //
}

void ConnectWifi() {
  WiFi.mode(WIFI_STA);
  Serial.println("Connecting to WiFi network: " + String(WIFI_SSID));
#ifdef ST7789_DISPLAY
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);

  tft.setCursor(0, 0);
  tft.println("Connecting to WiFi network: " + String(WIFI_SSID));
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
#ifdef ST7789_DISPLAY

    tft.print(".");
#endif
  }
  Serial.println("\nWiFi connected");

#ifdef ST7789_DISPLAY
  tft.print("\n Wifi Connected");
#endif
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  Serial0.begin(9600);  //For PM Sensor

#ifdef ST7789_DISPLAY
  tft.init(240, 320);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_BLUE);
  tft.setTextWrap(true);
  tft.setTextSize(2);
  tft.print("Starting Env Monitor..");

#endif

  Wire.begin(SD41_I2C_SDA, SD41_I2C_SCL);
  initSCD41();
  ConnectWifi();
  ThingSpeak.begin(client);  // Initialize ThingSpeak



}  // End of setup.

void loop() {
  bool dataReady = false;

  //
  // Wake the sensor up from sleep mode.
  //
  delay(10000);
  error = scd41_sensor.getDataReadyStatus(dataReady);
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute getDataReadyStatus(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }
  while (!dataReady) {
    delay(100);
    error = scd41_sensor.getDataReadyStatus(dataReady);
    if (error != NO_ERROR) {
      Serial.print("Error trying to execute getDataReadyStatus(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
    }
  }
  //
  // If ambient pressure compenstation during measurement
  // is required, you should call the respective functions here.
  // Check out the header file for the function definition.
  error = scd41_sensor.readMeasurement(co2Concentration, temperature, relativeHumidity);
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute readMeasurement(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }
  //
  // Print results in physical units.
  //
  Serial.print("CO2 concentration [ppm]: ");
  Serial.print(co2Concentration);
  Serial.println();
  Serial.print("Temperature [°C]: ");
  Serial.print(temperature);
  Serial.println();
  Serial.print("Relative Humidity [RH]: ");
  Serial.print(relativeHumidity);

  // set the fields with the values
  ThingSpeak.setField(1, co2Concentration);
  ThingSpeak.setField(2, temperature);
  ThingSpeak.setField(3, relativeHumidity);

  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200) {
    Serial.println("Channel update successful.");
  } else {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }

  display();
}  // End of loop


void display() {
  static int no_data_count = 0;

#ifdef ST7789_DISPLAY
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(0, 4);
  tft.setTextSize(3);
  tft.print("CO2[ppm]: ");
  tft.print(co2Concentration);
  //tft.setCursor(0, 40);
  tft.println(" ");
  tft.print(temperature);
  tft.print("C");
  tft.println(" ");
  tft.print("[RH]: ");
  tft.print(relativeHumidity);
#endif
}
