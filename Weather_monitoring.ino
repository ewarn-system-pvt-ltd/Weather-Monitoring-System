#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Wi-Fi Credentials ---
const char* ssid = "SIDDHI";
const char* password = "Koelnagara@8";

// --- MQTT Broker Details ---
const char* mqtt_server = "94.136.188.174";
const int mqtt_port = 1883;
const char* mqtt_user = "mqttadmin";
const char* mqtt_password = "isdr@430";

WiFiClient espClient;
PubSubClient client(espClient);

// --- DHT11 Setup ---
#define DHTPIN 27
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- BMP180 Setup ---
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

// --- OLED Display Setup ---
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32/128x64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Timer for non-blocking delays
unsigned long lastMsg = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Attempt to connect
    String clientId = "ESP32-WeatherMonitoring";
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("Connected to MQTT Broker!");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" - Trying again in 5 seconds");
      
      // Show MQTT error on OLED
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("MQTT Failed!");
      display.print("Retrying...");
      display.display();
      
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize OLED first so we can use it for boot messages
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Initialize Wi-Fi and MQTT
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  
  // Initialize DHT11
  dht.begin();
  
  // Initialize BMP180
  Serial.println("Initializing BMP180...");
  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP180 sensor, check wiring!");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("BMP180 Error!");
    display.display();
    while (1) {} 
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 

  unsigned long now = millis();
  
  if (now - lastMsg > 2000) {
    lastMsg = now;

    // Variables to hold sensor data
    float humidity = dht.readHumidity();
    float dht_temperature = dht.readTemperature();
    float pressure = 0.0;
    float altitude = 0.0;
    float bmp_temperature = 0.0;

    Serial.println("\n--- Weather Monitoring Data ---");

    // 1. Read DHT11
    if (isnan(humidity) || isnan(dht_temperature)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      Serial.print("DHT11 - Hum: "); Serial.print(humidity); Serial.print(" %\t Temp: "); Serial.print(dht_temperature); Serial.println(" C");
      client.publish("weather/humidity", String(humidity).c_str());
      client.publish("weather/temperature_dht", String(dht_temperature).c_str());
    }

    // 2. Read BMP180
    sensors_event_t event;
    bmp.getEvent(&event);

    if (event.pressure) {
      pressure = event.pressure;
      float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;
      altitude = bmp.pressureToAltitude(seaLevelPressure, event.pressure);
      bmp.getTemperature(&bmp_temperature);

      Serial.print("BMP180 - Pres: "); Serial.print(pressure); Serial.print(" hPa\t Alt: "); Serial.print(altitude); Serial.print(" m\t Temp: "); Serial.print(bmp_temperature); Serial.println(" C");
      client.publish("weather/pressure", String(pressure).c_str());
      client.publish("weather/altitude", String(altitude).c_str());
      client.publish("weather/temperature_bmp", String(bmp_temperature).c_str());
    } else {
      Serial.println("BMP180 Sensor error!");
    }

    // 3. Update OLED Display
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(" WEATHER STATION ");
    display.println("-----------------");
    
    // Display Temp (Using BMP180 for temp as it's usually more accurate than DHT11)
    display.print("Temp: "); 
    display.print(bmp_temperature); 
    display.println(" C");
    
    // Display Humidity
    display.print("Hum:  "); 
    display.print(humidity); 
    display.println(" %");

    // Display Pressure
    display.print("Pres: "); 
    display.print(pressure); 
    display.println(" hPa");

    // Display Altitude
    display.print("Alt:  "); 
    display.print(altitude); 
    display.println(" m");

    // Display Network Status
    display.println("-----------------");
    display.print("MQTT: ");
    display.println(client.connected() ? "Connected" : "Offline");
    
    display.display();
  }
}