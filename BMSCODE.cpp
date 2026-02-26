// --- Libraries --- 
// Used for I2C communication (OLED display) 
#include <Wire.h> 
// Used for SPI communication (OLED display via 
SPI) 
#include <SPI.h> 
// OLED graphics library 
#include <Adafruit_GFX.h> 
// OLED screen driver (SSD1306) 
#include <Adafruit_SSD1306.h> 
// DHT sensor library for temperature & humidity 
#include <DHT.h> 
// WiFi support for ESP32 
#include <WiFi.h> 
// HTTP support for sending data to ThingSpeak 
#include <HTTPClient.h> 
// SoftwareSerial library to communicate with 
SIM800L GSM module 
#include <SoftwareSerial.h> 
// --- OLED Display Configuration --- 
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_MOSI 23 
#define OLED_CLK 18 
#define OLED_DC 19 
#define OLED_CS -1 
#define OLED_RESET 5 
// --- Sensor and Actuator Pin Configuration --- 
#define DHT_PIN 4 
#define MQ2_PIN 35 
#define VOLTAGE_SENSOR_PIN 34 
#define CURRENT_SENSOR_PIN 36 
#define BUZZER_PIN 32 
#define FAN_RELAY_PIN 25
#define PELTIER_RELAY_PIN 26 
#define LOAD_RELAY_PIN 27 
// --- GSM Module Pin Configuration --- 
#define SIM800_TX 21  // Connect to SIM800L RX 
#define SIM800_RX 22  // Connect to SIM800L TX 
// --- DHT Sensor Configuration --- 
#define DHTTYPE DHT11 
DHT dht(DHT_PIN, DHTTYPE); 
// --- GSM Serial Setup --- 
SoftwareSerial sim800(SIM800_RX, SIM800_TX); // 
RX, TX 
// --- Voltage Divider Resistor Values --- 
const float R1 = 29500.0; 
const float R2 = 10000.0;
// --- Constants --- 
const float V_REF = 3.3; 
const int ADC_RESOLUTION = 4095; 
const float V_MAX = 12.8; 
const float V_MIN = 10.0; 
const float DESIGN_VOLTAGE = 12.8; 
const float ACS712_SENSITIVITY = 0.185; 
float zeroCurrentOffset = 2048.0; 
// --- WiFi and ThingSpeak Configuration --- 
const char* ssid = "ssid"; 
const char* password = "123456789"; 
const char* apiKey = "KWQZOLBVCKRIS3LP"; 
const char* server = 
"http://api.thingspeak.com/update"; 
// --- OLED Initialization --- 
Adafruit_SSD1306 display(SCREEN_WIDTH, 
SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, 
OLED_DC, OLED_RESET, OLED_CS); 
// --- Kalman Filter Variables for SOC & SOH --- 
float soc = 100.0, soh = 100.0; 
float soc_estimate = 100.0, soh_estimate = 100.0; 
float soc_error = 1.0, soh_error = 1.0; 
float soc_measurement_error = 2.0, 
soh_measurement_error = 2.0; 
float kalman_gain_soc = 0.0, kalman_gain_soh = 0.0; 
void sendSMS(String message) { 
sim800.println("AT+CMGF=1"); // Set SMS text 
mode 
delay(1000); 
sim800.println("AT+CMGS=\"+916305451180\""); // 
Replace with your phone number 
delay(1000); 
sim800.println(message); // Message content 
delay(100); 
sim800.write(26); // CTRL+Z to send SMS 
delay(5000); 
} 
 
void setup() { 
    Serial.begin(115200); 
    sim800.begin(9600); // Start SIM800L serial 
communication 
 
    // --- Set Pin Modes --- 
    pinMode(FAN_RELAY_PIN, OUTPUT); 
    pinMode(PELTIER_RELAY_PIN, OUTPUT); 
    pinMode(LOAD_RELAY_PIN, OUTPUT); 
    pinMode(BUZZER_PIN, OUTPUT); 
 
    digitalWrite(FAN_RELAY_PIN, LOW); 
    digitalWrite(PELTIER_RELAY_PIN, LOW); 
    digitalWrite(LOAD_RELAY_PIN, HIGH); 
    digitalWrite(BUZZER_PIN, LOW); 
 
    // --- OLED Setup ---
     if (!display.begin(SSD1306_SWITCHCAPVCC)) 
{ 
        Serial.println(F("SSD1306 allocation failed")); 
        while (1); 
    } 
 
    display.clearDisplay(); 
    display.setTextSize(1); 
    display.setTextColor(WHITE); 
 
    dht.begin(); 
 
    // --- Calibrate Current Sensor Offset --- 
    float sum = 0; 
    for (int i = 0; i < 100; i++) { 
        sum += 
analogRead(CURRENT_SENSOR_PIN); 
        delay(10); 
    }
    zeroCurrentOffset = sum / 100.0; 
 
    // --- WiFi Connection --- 
    WiFi.begin(ssid, password); 
    Serial.print("Connecting to WiFi..."); 
    while (WiFi.status() != WL_CONNECTED) { 
        delay(1000); 
        Serial.print("."); 
    } 
    Serial.println("\nConnected to WiFi!"); 
} 
 
void loop() { 
    float temperature = dht.readTemperature(); 
    float humidity = dht.readHumidity(); 
    int smokeLevel = analogRead(MQ2_PIN) / 40.95; 
 
    int sensorValue = 
analogRead(VOLTAGE_SENSOR_PIN);
float vout = (sensorValue * V_REF) / 
ADC_RESOLUTION; 
    float batteryVoltage = vout * ((R1 + R2) / R2); 
 
    // --- Kalman Filter for SOC --- 
    float soc_measured = ((batteryVoltage - V_MIN) / 
(V_MAX - V_MIN)) * 100.0; 
    soc_measured = constrain(soc_measured, 0.0, 
100.0); 
    kalman_gain_soc = soc_error / (soc_error + 
soc_measurement_error); 
    soc_estimate = soc_estimate + kalman_gain_soc * 
(soc_measured - soc_estimate); 
    soc_error = (1 - kalman_gain_soc) * soc_error; 
    soc = soc_estimate; 
 
    // --- Kalman Filter for SOH --- 
    float soh_measured = (batteryVoltage / 
DESIGN_VOLTAGE) * 100.0; 
    soh_measured = constrain(soh_measured, 90.0, 
100.0); 
    kalman_gain_soh = soh_error / (soh_error + 
soh_measurement_error);
  soh_estimate = soh_estimate + kalman_gain_soh * 
(soh_measured - soh_estimate); 
    soh_error = (1 - kalman_gain_soh) * soh_error; 
    soh = soh_estimate; 
 
    // --- Current Sensor Reading --- 
    float currentRaw = 
analogRead(CURRENT_SENSOR_PIN); 
    float currentVoltage = abs(((currentRaw - 
zeroCurrentOffset) * V_REF) / 
ADC_RESOLUTION); 
    float current = currentVoltage / 
ACS712_SENSITIVITY; 
 
    // --- Control Logic & Alerts --- 
    bool alertTriggered = false; 
    String alertMsg = ""; 
 
    if (temperature > 35) { 
        digitalWrite(FAN_RELAY_PIN, HIGH); 
        alertTriggered = true; 
 alertMsg += "High Temp: " + 
String(temperature) + "C\n"; 
    } else { 
        digitalWrite(FAN_RELAY_PIN, LOW); 
    } 
 
    if (humidity > 60) { 
        digitalWrite(PELTIER_RELAY_PIN, HIGH); 
        alertTriggered = true; 
        alertMsg += "High Humidity: " + 
String(humidity) + "%\n"; 
    } else { 
        digitalWrite(PELTIER_RELAY_PIN, LOW); 
    } 
 
    if (smokeLevel > 70) { 
        digitalWrite(LOAD_RELAY_PIN, LOW); 
        alertTriggered = true; 
        alertMsg += "Smoke Alert: " + 
String(smokeLevel) + "%\n";
 } 
 else { 
        digitalWrite(LOAD_RELAY_PIN, HIGH); 
    } 
 
    if (alertTriggered) { 
        digitalWrite(BUZZER_PIN, HIGH); 
        delay(2000); 
        digitalWrite(BUZZER_PIN, LOW); 
        delay(500); 
        sendSMS(alertMsg); 
    } 
 
    // --- Serial Monitor Output --- 
    Serial.print("Temp: "); Serial.print(temperature); 
Serial.print("°C  "); 
    Serial.print("Humidity: "); Serial.print(humidity); 
Serial.print("%  "); 
    Serial.print("Smoke: "); Serial.print(smokeLevel); 
Serial.print("%  "); 
    Serial.print("Voltage: "); 
Serial.print(batteryVoltage, 2); Serial.print("V  ");
  Serial.print("SOC: "); Serial.print(soc, 1); 
Serial.print("%  "); 
    Serial.print("SOH: "); Serial.print(soh, 1); 
Serial.print("%  "); 
    Serial.print("Current: "); Serial.print(current, 2); 
Serial.println("A"); 
 
    // --- OLED Display Output --- 
    display.clearDisplay(); 
    display.setCursor(0, 0); 
    display.print(F("Temp: ")); 
display.print(temperature, 1); display.println(F(" C")); 
    display.print(F("Humidity: ")); 
display.print(humidity, 1); display.println(F(" %")); 
    display.print(F("Smoke: ")); 
display.print(smokeLevel, 1); display.println(F(" 
%")); 
    display.print(F("Voltage: ")); 
display.print(batteryVoltage, 2); display.println(F(" 
V")); 
    display.print(F("SOC: ")); display.print(soc, 1); 
display.println(F(" %")); 
    display.print(F("SOH: ")); display.print(soh, 1); 
display.println(F(" %")); 
 display.print(F("Current: ")); display.print(current, 
2); display.println(F(" A")); 
    display.display(); 
 
    // --- ThingSpeak Upload --- 
    if (WiFi.status() == WL_CONNECTED) { 
        HTTPClient http; 
        String url = String(server) + "?api_key=" + 
apiKey + 
                     "&field1=" + String(temperature) + 
                     "&field2=" + String(humidity) + 
                     "&field3=" + String(smokeLevel) + 
                     "&field4=" + String(batteryVoltage, 2) + 
                     "&field5=" + String(soc, 1) + 
                     "&field6=" + String(soh, 1) + 
                     "&field7=" + String(current, 2); 
 
        http.begin(url); 
        int httpCode = http.GET();
            if (httpCode > 0) Serial.println("Data sent to 
ThingSpeak!"); 
        else Serial.println("ThingSpeak upload error."); 
        http.end(); 
    } else { 
        Serial.println("WiFi Disconnected!"); 
    } 
 
    delay(2000); 
}