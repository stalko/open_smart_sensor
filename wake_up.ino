#include <ESP8266WiFi.h>
#include <Esp.h>

const unsigned long SLEEP_INTERVAL = 20 * 1000 * 1000; // 20 sec

void setup() {
  Serial.begin(115200);  // Serial connection from ESP-01 via 3.3v console cable
  Serial.println("setup");
  getWakeUpReason();
  // put your setup code here, to run once:
  pinMode(2, OUTPUT);
  pinMode(14, INPUT_PULLUP);
}

void loop() {
  Serial.println("loop");
  static uint8_t state;

  // digitalRead() returns the current level of the pin,
  // either 1 for a high logic level or 0 for a low logic level.
  // Notice how the button is at a high level (value returns 1) when
  // it's not pressed. This is because the pull-up resistor keeps the pin at
  // a high level when it's not connected to ground through the button.
  // When the button is pressed then the input pin connects to ground
  // and reads a low level (value returns 0).
  //state = digitalRead(14);

  // Remember that the internal led turn on
  // when the pin is LOW
  // if(state == 0){//turn ON the light
  //   digitalWrite(2, LOW);
  //   Serial.println("TURN ON THE LIGHT");
  // } else {
  //   digitalWrite(2, HIGH);
  //   Serial.println("TURN OFF THE LIGHT");
  // }
  delay(1000);

  //--------------------------------
  // Serial.println("RTC Memory Test");
 
  // long *rtcMemory = (long *)0x60001200;
 
  // Serial.print("current value = ");
  // Serial.println(*rtcMemory);


  // (*rtcMemory)++;
  // Serial.print("new value = ");
  // Serial.println(*rtcMemory);


  Serial.println("RTC Memory Test NEW");
  Serial.println(getInternalBootCode());

  //--------------------------------
  Serial.println("Sleep");
  ESP.deepSleep(SLEEP_INTERVAL - micros(), WAKE_RF_DEFAULT);
  //ESP.deepSleepMax()
}

//RTC_DATA_ATTR int bootCount = 0;

void getWakeUpReason() {
  rst_info* rinfo = ESP.getResetInfoPtr();
  uint8_t wakeup_reason = rinfo->reason;
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
      Serial.println("Deep-Sleep Wake");
      break;
    case REASON_EXT_SYS_RST:
      Serial.println("External System");
      break;
    default:
      Serial.println("Unknown");
  }
}

int getInternalBootCode()
{
    uint32_t result;
    ESP.rtcUserMemoryRead(0, &result, 1);
    return (int)result;
}

void setInternalBootCode(int value)
{
    uint32_t storeValue = value;
    ESP.rtcUserMemoryWrite(0, &storeValue, 1);
}

