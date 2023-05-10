/* DHTServer - ESP8266 Webserver with a DHT sensor as an input

   Based on ESP8266Webserver, DHTexample, and BlinkWithoutDelay (thank you)
https://github.com/IOT-MCU/ESP-01S-DHT11-v1.0
   Version 1.0  5/3/2014  Version 1.0   Mike Barela for Adafruit Industries
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_AHTX0.h>
#define DHTTYPE DHT22
#define SDA 0
#define SCL 2

// Replace with your network details
const char* ssid     = "";
const char* password = "";
const String host = "";
const String token = "";
const String entity_id = "esp-01";
//ESP-01 i2c pins (arduino defaults to 4/5)

Adafruit_AHTX0 aht;

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelaySend = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelaySend = 5000;
// Set timer to 5 minutes (300000)
// unsigned long timerDelaySend = 300000;

unsigned long previousMillis = 0;        // will store last temp was read
const long interval = 5000;              // interval at which to read sensor

float current_humidity;
float current_temperature;
float prev_humidity;
float prev_temperature;

float humidity_arr[12];
int current_humidity_index = 0;

float temperature_arr[12];
int current_temperature_index = 0;

 
void setup(void)
{
  // You can open the Arduino IDE Serial Monitor window to see what the code is doing
  Serial.begin(115200);  // Serial connection from ESP-01 via 3.3v console cable
  Serial.println("setup");

  //Serial.print("Wire.begin(");Serial.print(SCL);Serial.print(",");Serial.print(SDA);Serial.print(")");;
  Wire.begin(SCL,SDA);

  if (!aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  // Connect to WiFi network
  // WiFi.begin(ssid, password);
  // Serial.print("\n\r \n\rWorking to connect");

  // Wait for connection
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(WiFi.status());
  //   Serial.print(".");
  // }
  // Serial.println("");
  // Serial.println("DHT Weather Reading Server");
  // Serial.print("Connected to ");
  // Serial.println(ssid);
  // Serial.print("IP address: ");
  // Serial.println(WiFi.localIP());
}
 
void loop(void)
{
  Serial.println("loop");
  // Wait at least 5 seconds seconds between measurements.
  // if the difference between the current time and last time you read
  // the sensor is bigger than the interval you set, read the sensor
  // Works better than delay for things happening elsewhere also
  // unsigned long currentMillis = millis();
 
  // if(currentMillis - previousMillis >= interval) {
  //   // save the last time you read the sensor 
  //   previousMillis = currentMillis;
    
  // }

  //Serial.println("Reading");
  readSensor();
  SendAllData();

  // delay(1000);
  //1,000,000 = 1 second
  ESP.deepSleep(5000000);
}

void SendAllData(){
  WiFi.begin(ssid, password);
  Serial.print("\n\r \n\rWorking to connect");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(WiFi.status());
    Serial.print(".");
  }
  sendSensorDataTemperature();
  
  // WiFi.disconnect(true);
}

void readSensor() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
  Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity.relative_humidity) || isnan(temp.temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  current_humidity = humidity.relative_humidity;
  current_temperature = temp.temperature;
}

void setSensorData(float humidity, float temperature){
  if(sizeof(humidity_arr) <= current_humidity_index || sizeof(temperature_arr) <= current_temperature_index){
    return;
  }
  humidity_arr[current_humidity_index] = humidity;
  current_humidity_index++;

  temperature_arr[current_temperature_index] = temperature;
  current_temperature_index++;
}

void cleanSensorData(){
  // for(int i = 0; i < sizeof(humidity_arr); i++)
  // {
  //   humidity_arr[i] = 0;
  // }

  // for(int i = 0; i < sizeof(temperature_arr); i++)
  // {
  //   temperature_arr[i] = 0;
  // }
}

void sendSensorData(){
  WiFiClient client;
  HTTPClient http;
  
  http.begin(client, host+"/api/services/sensor.esp_3_temperature");
  http.addHeader("Authorization", "Bearer "+token);
  http.addHeader("Content-Type", "application/json");
  
  String httpRequestData = "{\"state\": \"" + String(current_temperature) + "\", \"attributes\": {\"unit_of_measurement\": \"°C\", \"device_class\": \"temperature\"}}";  

  int httpResponseCode = http.POST(httpRequestData);
  
  // Serial.print("HTTP Response code: ");
  // Serial.println(httpResponseCode);
    
  // Free resources
  http.end();
}

void sendSensorDataTemperature(){
  WiFiClient client;
  HTTPClient http;
  
  // Your Domain name with URL path or IP address with path
  http.begin(client, host+"/api/states/sensor.esp_3_temperature");

  // If you need Node-RED/server authentication, insert user and password below
  //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
  http.addHeader("Authorization", "Bearer "+token);

  // Specify content-type header
  http.addHeader("Content-Type", "application/json");
  // Data to send with HTTP POST
  String httpRequestData = "{\"state\": \"" + String(current_temperature) + "\", \"attributes\": {\"unit_of_measurement\": \"°C\", \"device_class\": \"temperature\"}}";           
  // Send HTTP POST request
  int httpResponseCode = http.POST(httpRequestData);
  
  // If you need an HTTP request with a content type: application/json, use the following:
  //http.addHeader("Content-Type", "application/json");
  //int httpResponseCode = http.POST("{\"api_key\":\"tPmAT5Ab3j7F9\",\"sensor\":\"BME280\",\"value1\":\"24.25\",\"value2\":\"49.54\",\"value3\":\"1005.14\"}");

  // If you need an HTTP request with a content type: text/plain
  //http.addHeader("Content-Type", "text/plain");
  //int httpResponseCode = http.POST("Hello, World!");
  
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
    
  // Free resources
  http.end();
}

void sendSensorDataHumidity(){
  WiFiClient client;
  HTTPClient http;
  
  // Your Domain name with URL path or IP address with path
  http.begin(client, host+"/api/states/sensor.esp_3_humidity");
  http.addHeader("Authorization", "Bearer "+token);
  http.addHeader("Content-Type", "application/json");
  String httpRequestData = "{\"state\": \"" + String(current_humidity) + "\", \"attributes\": {\"unit_of_measurement\": \"%\", \"device_class\": \"humidity\"}}";
  int httpResponseCode = http.POST(httpRequestData);
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  http.end();
}

//------------Light sleep----------

#define sleepTimeSeconds 250
#define sleepCountMax 3
int sleepCount = 0;

// void loop() { 
//     if (sleepCount == 0) { 
//         //Do Stuff 
//     }

//     lightSleep(); 
//     if (sleepCount == sleepCountMax) { 
//         sleepCount = 0; 
//     } else { 
//         sleepCount++; 
//     } 
// }

void lightSleep() { 
    Serial.print("Sleeping ");
    Serial.println(sleepCount);
    Serial.flush();
    WiFi.mode(WIFI_OFF);
    extern os_timer_t *timer_list; 
    timer_list = nullptr;
    wifi_fpm_set_sleep_type(LIGHT_SLEEP_T); 
    wifi_fpm_set_wakeup_cb(wakeupCallback); 
    wifi_fpm_open(); 
    wifi_fpm_do_sleep(sleepTimeSeconds * 1000 * 1000);
    delay((sleepTimeSeconds * 1000) + 1); 
}

void wakeupCallback() { 
    Serial.println("Wakeup"); 
    Serial.flush();
}