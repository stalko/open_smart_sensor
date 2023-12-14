#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_AHTX0.h>
#include <AHT20.h>
#define DHTTYPE DHT22
#define SDA 0
#define SCL 2

//----------------------Config-------------------------
// Wifi connection
#define WIFI_SSID ""
#define WIFI_AUTH ""

// data from DHCP: IP address, network mask, Gateway, DNS
#define DATA_IPADDR IPAddress(192,168,178,228)  //Test
//#define DATA_IPADDR IPAddress(192,168,178,203)  //Kid's
//#define DATA_IPADDR IPAddress(192,168,178,229)  //Test 2
#define DATA_IPMASK IPAddress(255,255,255,0)
#define DATA_GATEWY IPAddress(192, 168, 178, 1)
#define DATA_DNS1   IPAddress(213,46,228,196)
#define DATA_DNS2   IPAddress(62,179,104,196)

// data from your Wifi connect: Channel, BSSID
#define DATA_WIFI_CH 4
#define DATA_WIFI_BSSID {0x1C, 0xED, 0x6F, 0x62, 0x49, 0x5B}
#define WIFI_CONNECTION_TIMEOUT 3000 // 3sec 

#define LED_BUILDIN 2
#define analogPin A0 /* ESP8266 Analog Pin ADC0 = A0 */

// data for Home assistant
const String host = "http://127.0.0.1:8123";
const String token = "";
const String entity_id = "esp_12_v3";  //Test
//const String entity_id = "esp_02";  //Kid's
//const String entity_id = "esp_03";  //Test 3
//-----------------------------------------------------------

Adafruit_AHTX0 aht;
AHT20 aht20;
float current_humidity;
float current_temperature;
float current_voltage;
int heat_up_sensor = 6;

 
void setup(void)
{
  pinMode(LED_BUILDIN, OUTPUT);//setup build in LED

  // Why are we awake?
  //getWakeUpReason();

  // WiFi.setAutoConnect(false); // prevent early autoconnect
  
  Serial.begin(115200);  // Serial connection from ESP8266 via 3.3v console cable
  Serial.println("setup");

  Wire.begin(SCL,SDA);

  if (aht20.begin() == false)
  {
    Serial.println("AHT20 not detected. Please check wiring. Freezing.");
    while (1);
  }
}


void loop(void)
{
  Serial.println("loop");

  getVoltage();
  readSensorAHT20();
  wifiConnect();
  sendSensorData();

  Serial.println("Sleep");
  ESP.deepSleep(300000000, WAKE_RF_DEFAULT);//5 mins, 1800 * 1e6 = 30 mins
  
  //The AHT20 can respond with a reading every ~50ms. However, increased read time can cause the IC to heat around 1.0C above ambient.
  //The datasheet recommends reading every 2 seconds.
}

void getVoltage(){
  int adcValue = analogRead(analogPin); /* Read the Analog Input value */
  /* Print the output in the Serial Monitor */
  Serial.print("ADC Value = ");
  Serial.println(adcValue);
  
  current_voltage = adcValue * 3.3/1023;
  Serial.print("Voltage = ");
  Serial.println(current_voltage);
}

void wifiConnect() {
  char my_ssid[] = WIFI_SSID;
  char my_auth[] = WIFI_AUTH;
  WiFi.begin(&my_ssid[0], &my_auth[0]);
  while ((WiFi.status() != WL_CONNECTED)) { delay(5); }
  Serial.print("Connect to WIFI: Done");
}

void wifiConnectFast() {
  Serial.println("\nConnect to WIFI: Starting.");
  uint32_t ts_start = millis();
  // WiFi.onEvent(WiFiEvent);
  WiFi.disconnect(true); // delete old config
  WiFi.persistent(false); //Avoid to store Wifi configuration in Flash
  WiFi.mode(WIFI_AP_STA);
  WiFi.config(DATA_IPADDR, DATA_GATEWY, DATA_IPMASK);
  uint8_t my_bssid[] = DATA_WIFI_BSSID;
  char my_ssid[] = WIFI_SSID;
  char my_auth[] = WIFI_AUTH;
  WiFi.begin(&my_ssid[0], &my_auth[0], DATA_WIFI_CH, &my_bssid[0], true);
  int countRetry = 0;
  while ((WiFi.status() != WL_CONNECTED)) {
    if(WIFI_CONNECTION_TIMEOUT < countRetry){
      break;
    }
    countRetry+=5;
    delay(5);
  }
  Serial.print("Timing wifiConnect(): "); Serial.print(millis()-ts_start); Serial.println("ms");
  Serial.print("Connect to WIFI: Done");
}

void WiFiEvent(WiFiEvent_t event)
{
    Serial.printf("[WiFi-event] event: %d timestamp:%d\n", event, millis());
    switch (event)
    {
    case WIFI_EVENT_STAMODE_GOT_IP:
        Serial.printf("WiFi connected IP address: %s duration %d\n", WiFi.localIP().toString().c_str(),millis());
        break;
    }
}

void readSensor() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  Serial.println();
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

void readSensorAHT20(){
  int countRetry = 0;
  while(!aht20.available()){
    if(200 < countRetry){
      break;
    }
    Serial.println("Wait aht21");
    countRetry+=20;
    delay(20);
  }
  current_temperature = aht20.getTemperature();
  current_humidity = aht20.getHumidity();

  Serial.println("Currently temp");
  Serial.println(current_temperature);
  
  // if (aht20.available() == true)
  // {
  //   current_temperature = aht20.getTemperature();
  //   current_humidity = aht20.getHumidity();
  // } else {
  //   Serial.println("Failed to read from aht20 sensor!");
  // }
}

void sendSensorData(){
  Serial.println("\nSend Data: Starting.");
  WiFiClient client;
  HTTPClient http;

  http.begin(client, host+"/api/states/sensor." + entity_id+"_temperature");
  http.addHeader("Authorization", "Bearer "+token);
  http.addHeader("Content-Type", "application/json");
  String httpRequestData = "{\"state\": \"" + String(current_temperature) + "\", \"attributes\": {\"unit_of_measurement\": \"°C\", \"device_class\": \"temperature\", \"humidity\":\"" + String(current_humidity) + "\", \"battery_voltage\":\"" + String(current_voltage) + "\"}}";
  int httpResponseCode = http.POST(httpRequestData);
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  http.end();
}

//------------Light sleep----------

// #define sleepTimeSeconds 250
// #define sleepCountMax 3
// int sleepCount = 0;

// // void loop() { 
// //     if (sleepCount == 0) { 
// //         //Do Stuff 
// //     }

// //     lightSleep(); 
// //     if (sleepCount == sleepCountMax) { 
// //         sleepCount = 0; 
// //     } else { 
// //         sleepCount++; 
// //     } 
// // }

void lightSleep(int sleepTimeSeconds) { 
    Serial.println("Light sleep");
    Serial.flush();
    WiFi.mode(WIFI_OFF);
    extern os_timer_t *timer_list; 
    timer_list = nullptr;
    wifi_fpm_set_sleep_type(LIGHT_SLEEP_T); 
    wifi_fpm_set_wakeup_cb(wakeupCallback); 
    wifi_fpm_open(); 
    wifi_fpm_do_sleep(sleepTimeSeconds * 1000 * 1000);
}

void wakeupCallback() { 
    Serial.println("Wakeup"); 
    Serial.flush();
}
//----------------------------------------------

// Place all variables you want to retain across sleep periods in the 
// RTC SRAM. 8kB SRAM on the RTC for all variables.
// RTC_DATA_ATTR int bootCount = 0;

// void getWakeUpReason() {
//   log_w("This is (re)boot #%d", ++bootCount);

//   esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
//   switch (wakeup_reason) {
//     case ESP_SLEEP_WAKEUP_EXT0:
//       log_w("Wakeup caused by external signal using RTC_IO");
//       break;
//     case ESP_SLEEP_WAKEUP_EXT1:
//       log_w("Wakeup caused by external signal using RTC_CNTL");
//       break;
//     case ESP_SLEEP_WAKEUP_TIMER:
//       log_w("Wakeup caused by RTC timer");
//       break;
//     case ESP_SLEEP_WAKEUP_TOUCHPAD:
//       log_w("Wakeup caused by touchpad");
//       break;
//     case ESP_SLEEP_WAKEUP_ULP:
//       log_w("Wakeup caused by ULP program");
//       break;
//     default:
//       log_w("Wakeup was not caused by deep sleep: %d", wakeup_reason);
//   }
// }

void blinkWithLED(int count){
  turnOffLED();
  for (int i = 0; i < count; i++) {
    turnOnLED();
    delay(1000);
    turnOffLED();
    delay(1000);
  }
}

void turnOnLED(){
  digitalWrite(LED_BUILDIN, LOW);
}

void turnOffLED(){
  digitalWrite(LED_BUILDIN, HIGH);
}


