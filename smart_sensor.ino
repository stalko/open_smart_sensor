#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_AHTX0.h>
#include <AHT20.h>
#include <Esp.h>
#include "./DNSServer.h"
#include "ESPAsyncWebServer.h"
#include <ESP_EEPROM.h>
#include <AsyncElegantOTA.h>

#define DHTTYPE DHT22
#define SDA 0
#define SCL 2

#define WIFI_CONNECTION_TIMEOUT 3000 // 3sec 

#define LED_BUILDIN 2
#define analogPin A0 /* ESP8266 Analog Pin ADC0 = A0 */
#define WAKE_UP_DUE_TO_HARD_RESET 6
#define WIFI_AP_NAME "OSS"

const unsigned long SLEEP_SECOND =  1000 * 1000; // 1 sec

Adafruit_AHTX0 aht;
AHT20 aht20;
float current_humidity;
float current_temperature;
float current_voltage;

struct Configuration {
  String ssid;
  String pass;

  boolean optimize_wifi_connection;

  byte bssid[6]; //TODO: implement reading
  int32_t channel; //TODO: implement reading
  String local_ip;  //TODO: implement reading
  String gateway; //TODO: implement reading
  String subnet; //TODO: implement reading

  String ha_url;
  String ha_access_key;

  int deep_sleep_sec;

  float max_voltage;
} settings;

uint8_t wakeup_reason;

const byte        DNS_PORT = 53;          // Capture DNS requests on port 53
IPAddress         ap_ip(10, 10, 10, 1);    // Private network for server
DNSServer         dns_server;              // Create the DNS object
AsyncWebServer    server(80);             // HTTP server
boolean config_mode = false;
 
void setup(void)
{
  pinMode(LED_BUILDIN, OUTPUT);//setup build in LED

  Serial.begin(115200);  // Serial connection from ESP8266 via 3.3v console cable
  Serial.println("setup");

  Wire.begin(SCL,SDA);

  // Why are we awake?
  getWakeUpReason();

  EEPROM.begin(sizeof(Configuration));

  if(EEPROM.percentUsed() >= 0){
    Serial.println("EEPROM has data from a previous run.");
    Serial.print(EEPROM.percentUsed());
    Serial.println("% of ESP flash space currently used");
    loadConfig();
  }

  // WiFi.setAutoConnect(false); // prevent early autoconnect

  if (aht20.begin() == false)
  {
    Serial.println("AHT20 not detected. Please check wiring. Freezing.");
    while (1);
  }

  if(wakeup_reason == WAKE_UP_DUE_TO_HARD_RESET){// double click reset button
    config_mode = true;
    //TODO: Or when wifi config is empty
  }

  if(config_mode){
    setupWiFiAndServer();
  }
}


void loop(void)
{
  if(config_mode) {
    dns_server.processNextRequest();
  }
  else {
    getVoltage();
    readSensorAHT20();
    wifiConnect();
    sendSensorData();

    Serial.println("Sleep");
    ESP.deepSleep(settings.deep_sleep_sec * SLEEP_SECOND, WAKE_RF_DEFAULT);//5 mins, 1800 * 1e6 = 30 mins
    
    //The AHT20 can respond with a reading every ~50ms. However, increased read time can cause the IC to heat around 1.0C above ambient.
    //The datasheet recommends reading every 2 seconds.
  }
}

void getVoltage(){
  int adc_value = analogRead(analogPin); /* Read the Analog Input value */
  /* Print the output in the Serial Monitor */
  Serial.print("ADC Value = ");
  Serial.println(adc_value);
  
  current_voltage = adc_value * 3.3/1023;
  Serial.print("Voltage = ");
  Serial.println(current_voltage);
}

void wifiConnect() {
  WiFi.begin(settings.ssid, settings.pass);
  while ((WiFi.status() != WL_CONNECTED)) { delay(5); }//TODO: handle timeout
  Serial.print("Connect to WIFI: Done");
}

// void wifiConnectFast() {
//   Serial.println("\nConnect to WIFI: Starting.");
//   uint32_t ts_start = millis();
//   WiFi.disconnect(true); // delete old config
//   WiFi.persistent(false); //Avoid to store Wifi configuration in Flash
//   WiFi.mode(WIFI_AP_STA);
//   WiFi.config(DATA_IPADDR, DATA_GATEWY, DATA_IPMASK);
//   uint8_t my_bssid[] = DATA_WIFI_BSSID;
//   char my_ssid[] = WIFI_SSID;
//   char my_auth[] = WIFI_AUTH;
//   WiFi.begin(&my_ssid[0], &my_auth[0], DATA_WIFI_CH, &my_bssid[0], true);
//   int countRetry = 0;
//   while ((WiFi.status() != WL_CONNECTED)) {
//     if(WIFI_CONNECTION_TIMEOUT < countRetry){
//       break;
//     }
//     countRetry+=5;
//     delay(5);
//   }
//   Serial.print("Timing wifiConnect(): "); Serial.print(millis()-ts_start); Serial.println("ms");
//   Serial.print("Connect to WIFI: Done");
// }

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
}

void sendSensorData(){
  Serial.println("\nSend Data: Starting.");
  WiFiClient client;
  HTTPClient http;

  http.begin(client, settings.ha_url);
  http.addHeader("Authorization", "Bearer "+settings.ha_access_key);
  http.addHeader("Content-Type", "application/json");
  String httpRequestData = "{\"state\": \"" + String(current_temperature) + "\", \"attributes\": {\"unit_of_measurement\": \"°C\", \"device_class\": \"temperature\", \"humidity\":\"" + String(current_humidity) + "\", \"battery_voltage\":\"" + String(current_voltage) + "\"}}";
  int httpResponseCode = http.POST(httpRequestData);
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  http.end();
}

void getWakeUpReason() {
  rst_info* rinfo = ESP.getResetInfoPtr();
  wakeup_reason = rinfo->reason;
  Serial.println(wakeup_reason);

  switch (wakeup_reason) {
    case REASON_DEFAULT_RST:
      Serial.println("Power on");
      break;
    case REASON_WDT_RST:
      Serial.println("Hardware Watchdog");
      break;
    case REASON_EXCEPTION_RST:
      Serial.println("Exception");
      break;
    case REASON_SOFT_WDT_RST:
      Serial.println("Software Watchdog");
      break;
    case REASON_SOFT_RESTART:
      Serial.println("Software/System restart");
      break;
    case REASON_DEEP_SLEEP_AWAKE:
      Serial.println("Deep-Sleep Wake"); //
      break;
    case REASON_EXT_SYS_RST:
      Serial.println("External System");
      break;
    default:
      Serial.println("Unknown");
  }
}

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


const char html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=0">
    <title>OSS - opensource smart sensor</title>
    
  </head>
  <body>
    <h1>OSS configuration mode</h1>

    <p>Temperature: %PLACEHOLDER_TEMPERATURE% ºC</p>
    <p>Humidity: %PLACEHOLDER_HUMIDITY% <span>&#37;</span> </p>
    <p>Battery: %PLACEHOLDER_BATTERY_VOLTAGE% V</p>


    <form action="/save" method="POST">
      <label for="wifi_name">First name:</label>
      <br>
      <input type="text" name="wifi_name" placeholder="Wi-Fi name" value="%PLACEHOLDER_SSID%" style="width: 95%;">
      <br>

      <label for="wifi_pass">Wifi Password:</label>
      <br>
      <input type="password" name="wifi_pass" placeholder="Wi-Fi password" value="%PLACEHOLDER_PASS%" style="width: 95%;">
      <br>

      <input type="checkbox" id="optimize_wifi" name="optimize_wifi" value="optimize_wifi">
      <label for="optimize_wifi">Optimize wifi connection</label>
      <br>
      <br>

      <label for="ha_url">Home Assistant URL:</label>
      <br>
      <input type="text" name="ha_url" placeholder="Home Assistant URL" value="%PLACEHOLDER_HA_URL%" style="width: 95%;">
      <br>
      
      <label for="ha_access_key">Home Assistant Access Key:</label>
      <br>
      <input type="text" name="ha_access_key" placeholder="Home Assistant Access Key" value="%PLACEHOLDER_HA_ACCESS_KEY%" style="width: 95%;">
      <br>
      <br>


      <label for="deep_sleep">Deep sleep duration:</label>
      <br>
      <input type="number" name="deep_sleep" placeholder="Deep sleep duration(s)" min="5" max="4260" value="%PLACEHOLDER_DEEP_SLEEP%" style="width: 95%;">
      <br>
      <br>

      <label for="battery">Batteries type:</label>
      <select id="battery" name="battery">
        <option value="3.3">Alkalines</option>
        <option value="2.9">NiMH</option>
      </select>
      <br>
      <br>
      
      <input type="submit" value="Save">
    </form>

    <hr>

    <br>
    <form action="/reboot" method="POST"><input type="submit" value="Reboot"></form>
  </body>
</html>)rawliteral";

String processor(const String& var)
{
  Serial.println(var);
  if(var == "PLACEHOLDER_TEMPERATURE") {
    return String(current_temperature);
  }
  else if(var == "PLACEHOLDER_HUMIDITY") {
    return String(current_humidity);
  }
  else if(var == "PLACEHOLDER_BATTERY_VOLTAGE") {
    return String(current_voltage);
  }
  else if(var == "PLACEHOLDER_SSID") {
    return settings.ssid;
  }
  else if(var == "PLACEHOLDER_PASS") {
    return settings.pass;
  }
  else if(var == "PLACEHOLDER_HA_URL") {
    return settings.ha_url;
  }
  else if(var == "PLACEHOLDER_HA_ACCESS_KEY") {
    return settings.ha_access_key;
  }
  else if(var == "PLACEHOLDER_DEEP_SLEEP") {
    return String(settings.deep_sleep_sec);
  }
  return String();
}

//---------------------------WIFI AND SERVER---------------------------------

void setupWiFiAndServer()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ap_ip, ap_ip, IPAddress(255, 255, 255, 0));
  WiFi.softAP(WIFI_AP_NAME);

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  //dnsServer.start(DNS_PORT, "*", ap_ip);
  dns_server.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    getVoltage();
    readSensorAHT20();
    request->send_P(200, "text/html", html, processor);
  });

  server.on("/save", HTTP_POST, handleSaveConfig);
  server.on("/reboot", HTTP_POST, handleReset);
  server.onNotFound(notFound);

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA

  server.begin();
}

void notFound(AsyncWebServerRequest *request) {
  request->redirect("/");
}

void handleSaveConfig(AsyncWebServerRequest *request) {
  Serial.println("Save config");

  // int params = request->params();
  // for (int i = 0; i < params; i++) {
  //   AsyncWebParameter* p = request->getParam(i);
  //   Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
  // }


  if (request->hasParam("wifi_name")) {
    settings.ssid = request->getParam("wifi_name", true)->value();
    Serial.println(settings.ssid);
  }

  if (request->hasParam("wifi_pass")) {
    settings.pass = request->getParam("wifi_pass", true)->value();
    Serial.println(settings.pass);
  }

  if (request->hasParam("ha_url")) {
    settings.ha_url = request->getParam("ha_url", true)->value();
    Serial.println(settings.ha_url);
  }

  if (request->hasParam("ha_access_key")) {
    settings.ha_access_key = request->getParam("ha_access_key", true)->value();
    Serial.println(settings.ha_access_key);
  }

  if (request->hasParam("deep_sleep")) {
    String deep_sleep = request->getParam("deep_sleep", true)->value();
    Serial.println(deep_sleep);
    settings.deep_sleep_sec = deep_sleep.toInt();
  }

  saveConfig();
  
  request->send_P(200, "text/html", "<h1>Configurations successfully saved</h1><form action=\"/reboot\" method=\"POST\"><input type=\"submit\" value=\"Reboot\"></form>");

  // webServer.send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
  //   return;
}

void handleReset(AsyncWebServerRequest *request) {
  Serial.println("Sleep");
  request->send_P(200, "text/html", "");
  ESP.deepSleep(2e6, WAKE_RF_DEFAULT);//Deep sleep mode for 2 seconds
}

//--------------------------------EEPROM---------------------------------

void saveConfig(){
  Serial.print("Saving to EEPROM:");
  Serial.println(settings.ssid);
  Serial.println(settings.pass);
  Serial.println(settings.deep_sleep_sec);
  Serial.println(settings.ha_url);
  Serial.println(settings.ha_access_key);

  boolean okWiping = EEPROM.wipe();
  Serial.println((okWiping) ? "Wiping OK" : "Wiping failed");

  EEPROM.put(0, settings);
  Serial.println("Written custom data type!");

  // write the data to EEPROM
  boolean ok = EEPROM.commit();
  Serial.println((ok) ? "Commit OK" : "Commit failed");

  delay(2000);
}

void loadConfig(){
  EEPROM.get(0, settings);
  Serial.println("Read custom object from EEPROM: ");
  Serial.println(settings.ssid);
  Serial.println(settings.pass);
  Serial.println(settings.deep_sleep_sec);
  Serial.println(settings.ha_url);
  Serial.println(settings.ha_access_key);
  Serial.println("Done");
}