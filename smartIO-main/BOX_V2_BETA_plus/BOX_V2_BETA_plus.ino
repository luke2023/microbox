/*
v1 (sensor max) for gen1
v2 gor gen2
v3.2 gor gen2,v3.3for gen3
*/


#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_MAX31865.h>
#include <SimpleDHT.h>
#include <WiFiManager.h>
#include "smoothFonts.h"
#include <TFT_eSPI.h>  // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include <ESP32Time.h>
#include "time.h"
#include "soc/rtc_wdt.h"
ESP32Time rtc;
int pinDHT11 = 32;
int pinBuzzer = 12;
long lastepoc = 0;
SimpleDHT22 dht22(pinDHT11);


// Use software SPI: CS, DI, DO, CLK
//Adafruit_MAX31865 thermo = Adafruit_MAX31865(10, 11, 12, 13);
// use hardware SPI, just pass in the CS pin
Adafruit_MAX31865 thermo1 = Adafruit_MAX31865(2);
Adafruit_MAX31865 thermo2 = Adafruit_MAX31865(15);
// The value of the Rref resistor. Use 430.0 for PT100 and 4300.0 for PT1000
#define RREF1 430.8
#define RREF2 429.0
// The 'nominal' 0-degrees-C resistance of the sensor
// 100.0 for PT100, 1000.0 for PT1000
#define RNOMINAL 100.0
TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

// Replace the next variables with your SSID/Password combination
const char* ssid = "Buffalo-G-F560";
const char* password = "";

// Add your MQTT Broker IP address, example:
//const char* mqtt_server = "192.168.1.144";
const char* mqtt_server = "twhmbt.synology.me";
int state = 0;
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
TaskHandle_t ntp;
//float temperature = 0;

int flag = -210000;
// LED Pin
const int ledPin = 4;
bool input, oldInput;
bool alarmState;
long alarmFlag;
long alarmStart;
String userName, deviceName, newUserName, newDeviceName;
// void readMemory(){
//   EEPROM.begin(160);
//   for (int i = 160; i < 240; i++)
//     {
//       UserName += char(EEPROM.read(i));
//     }
//   for (int i = 240; i < 320; i++)
//     {
//       UserName += char(EEPROM.read(i));
//     }
// }
// void writeMemory(){
//   for (int i = 0; i < newUserName.length(); i++)
//     {
//       EEPROM.write(i+160, newUserName[i]);
//       Serial.print("Wrote: ");
//       Serial.println(newUserName[i]);
//     }

//   //寫入EEPROM 32~95的位置
//   Serial.println("writing eeprom pass:");
//   for (int i = 0; i < write_password.length(); ++i)
//     {
//       EEPROM.write(240+i, newDeviceName[i]);
//       Serial.print("Wrote: ");
//       Serial.println(newDeviceName[i]);
//     }
// }
void setup() {
  // WiFiManagerParameter username("User name","user","name", 80);
  // WiFiManagerParameter devicename("Device name","device","name", 80);
  // wifiManager.addParameter(&username);
  // wifiManager.addParameter(&devicename);

  xTaskCreatePinnedToCore(
    toNtp, /* Task function. */
    "ntp", /* name of task. */
    10000, /* Stack size of task */
    NULL,  /* parameter of the task */
    1,     /* priority of the task */
    &ntp,  /* Task handle to keep track of created task */
    0);

  pinMode(pinBuzzer, OUTPUT);
  oldInput = input;
  Serial.begin(115200);
  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  //status = bme.begin();
  // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // it is a good practice to make sure your code sets wifi mode how you want it.


  // put your setup code here, to run once:
  Serial.begin(115200);

  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  // wm.resetSettings();

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  res = wm.autoConnect("PasteurBox Gen2", "88888888");  // password protected ap

  if (!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }
  //setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  Serial.println("Adafruit MAX31865 PT100 Sensor Test!");
  thermo1.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary
  thermo2.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary
  //pinMode(ledPin, OUTPUT);
  lastMsg = -700000.0;
  tft.init();
  tft.setRotation(1);
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  if (String(topic) == "inputwater/alarm2") {
    // Serial.print("Changing output to ");
    // if (messageTemp == "on") {
    //   Serial.println("on");
    //   //digitalWrite(ledPin, HIGH);
    // } else if (messageTemp == "off") {
    //   Serial.println("off");
    //   // digitalWrite(ledPin, LOW);
    input = 1;
    //}
  }
  setTimezone("CST-8");
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String MQTTClientid = "esp32-" + String(random(1000000, 9999999));
    char* MQTTUser = "mqttuser";  //不須帳密
    char* MQTTPassword = "mqttuser";
    if (client.connect(MQTTClientid.c_str(), MQTTUser, MQTTPassword)) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("inputwater/alarm2");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  long now = millis();
  if (now - lastMsg > 3000 * 1) {
    //     float temp1a,temp2a;
    // for(int i =0;i<20;i++){
    //     temp1a+=thermo1.temperature(RNOMINAL, RREF);
    //     temp2a+=thermo2.temperature(RNOMINAL, RREF);
    //   }
    float temperature1 = thermo1.temperature(RNOMINAL, RREF1);

    float temperature2 = thermo2.temperature(RNOMINAL, RREF2);
    ;

    //     Serial.print("RTD value: ");
    //     Serial.println(rtd);
    //     float ratio = rtd;
    //     ratio /= 32768;
    //     Serial.print("Temperature = ");
    //     Serial.println(thermo1.temperature(RNOMINAL, RREF));
    // Serial.println(thermo2.temperature(RNOMINAL, RREF));
    // Check and print any faults
    Serial.println("1: ");

    uint8_t fault = thermo1.readFault();
    if (fault) {
      Serial.print("Fault 0x");
      Serial.println(fault, HEX);
      if (fault & MAX31865_FAULT_HIGHTHRESH) {
        Serial.println("RTD High Threshold");
      }
      if (fault & MAX31865_FAULT_LOWTHRESH) {
        Serial.println("RTD Low Threshold");
      }
      if (fault & MAX31865_FAULT_REFINLOW) {
        Serial.println("REFIN- > 0.85 x Bias");
      }
      if (fault & MAX31865_FAULT_REFINHIGH) {
        Serial.println("REFIN- < 0.85 x Bias - FORCE- open");
      }
      if (fault & MAX31865_FAULT_RTDINLOW) {
        Serial.println("RTDIN- < 0.85 x Bias - FORCE- open");
      }
      if (fault & MAX31865_FAULT_OVUV) {
        Serial.println("Under/Over voltage");
      }
      thermo1.clearFault();
    }
    Serial.println("2: ");
    fault = thermo2.readFault();
    if (fault) {
      Serial.print("Fault 0x");
      Serial.println(fault, HEX);
      if (fault & MAX31865_FAULT_HIGHTHRESH) {
        Serial.println("RTD High Threshold");
      }
      if (fault & MAX31865_FAULT_LOWTHRESH) {
        Serial.println("RTD Low Threshold");
      }
      if (fault & MAX31865_FAULT_REFINLOW) {
        Serial.println("REFIN- > 0.85 x Bias");
      }
      if (fault & MAX31865_FAULT_REFINHIGH) {
        Serial.println("REFIN- < 0.85 x Bias - FORCE- open");
      }
      if (fault & MAX31865_FAULT_RTDINLOW) {
        Serial.println("RTDIN- < 0.85 x Bias - FORCE- open");
      }
      if (fault & MAX31865_FAULT_OVUV) {
        Serial.println("Under/Over voltage");
      }
      thermo2.clearFault();
    }
    float temperature = 0;
    float humidity = 0;
    int err = SimpleDHTErrSuccess;
    if ((err = dht22.read2(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
      Serial.print("Read DHT22 failed, err=");
      Serial.print(SimpleDHTErrCode(err));
      Serial.print(",");
      Serial.println(SimpleDHTErrDuration(err));
      delay(2000);
      return;
    }
    Serial.print("Sample OK: ");
    Serial.print((float)temperature);
    Serial.print(" *C, ");
    Serial.print((float)humidity);
    Serial.println(" RH%");

    char humString[10];
    dtostrf(double(humidity), 4, 2, humString);
    char tempString[10];
    dtostrf(double(temperature), 4, 2, tempString);
    // Serial.print("Humidity: ");
    // Serial.println(humString);


    char temp1String[10];
    dtostrf(double(temperature1), 4, 2, temp1String);
    char temp2String[10];
    dtostrf(double(temperature2), 4, 2, temp2String);
    Serial.print("temp1: ");
    Serial.println(temp1String);
    Serial.print("temp2: ");
    Serial.println(temp2String);
    client.publish("inputwater/water221", temp2String);
    client.publish("inputwater/water222", temp1String);
    client.publish("inputwater/temp22", tempString);
    client.publish("inputwater/humi22", humString);
    lastMsg = now;


    Serial.println(input);
    Serial.println(oldInput);
    Serial.println(alarmState);
    Serial.println(alarmFlag);
    Serial.println(alarmStart);




    Serial.println("rtctime");
    Serial.println(rtc.getTime("RTC0: %A, %B %d %Y %H:%M:%S"));
    tft.setTextFont(7);
    tft.setCursor(100, 0, 2);
    tft.loadFont(smooth);
    tft.fillScreen(TFT_WHITE);

    // Set "cursor" at top left corner of display (0,0) and select font 2
    // (cursor will move to next line automatically during printing with 'tft.println'
    //  or stay on the line is there is room for the text with tft.print)
    tft.setCursor(0, 0, 2);
    // Set the font colour to be white with a black background, set text size multiplier to 1
    //tft.setTextColor(TFT_WHITE, TFT_BLACK);
    //tft.setTextSize(30);
    // We can now plot text on screen using the "print" class
    //tft.println("Hello World!");

    // Set the font colour to be yellow with no background, set to font 7
    tft.setTextColor(TFT_YELLOW);
    //tft.setTextFont(2);
    tft.println(temperature1);

    // Set the font colour to be red with black background, set to font 4
    tft.setTextColor(TFT_BLUE);
    //  tft.setTextFont(4);
    tft.println(temperature2);  // Should print DEADBEEF
    tft.unloadFont();
  }
  if (input) {
    if (oldInput == 0) {
      oldInput = 1;
      alarmStart = now;
    }
    if (now - alarmStart > 60 * 1000) {
      input = 0;
      oldInput = 0;
      digitalWrite(pinBuzzer, 0);
    }

    if (now - alarmFlag > 800) {
      if (alarmState) {
        digitalWrite(pinBuzzer, 0);
        alarmState = !alarmState;
      } else {
        digitalWrite(pinBuzzer, 1);
        alarmState = !alarmState;
      }

      alarmFlag = now;
    }
  }
  if (WiFi.status() != WL_CONNECTED) {
    ESP.restart();
  }
}
void toNtp(void* pvParameters) {
  vTaskDelay(500 / portTICK_PERIOD_MS);
  //Serial.print("timer running on core ");
  //Serial.println(xPortGetCoreID());
  while (1) {

    if (rtc.getEpoch() - lastepoc > 3000) {
      initTime("CST-8");

      //Serial.println("inok");
      printLocalTime();
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      //stopWifi();
      rtc.setTime(getTimeepoc());
      // Serial.println("rtctime");
      // Serial.println(rtc.getTime("RTC0: %A, %B %d %Y %H:%M:%S"));
      // Serial.println(rtc.getEpoch());
      // Serial.println(lastepoc);
      lastepoc = rtc.getEpoch();
    }
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

unsigned long getTimeepoc() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}
void setTimezone(String timezone) {
  Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(), 1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}
void initTime(String timezone) {
  struct tm timeinfo;

  Serial.println("Setting up time");
  configTime(0, 0, "pool.ntp.org");  // First connect to NTP server, with 0 TZ offset
  if (!getLocalTime(&timeinfo)) {
    Serial.println("  Failed to obtain time");
    return;
  }
  Serial.println("  Got the time from NTP");
  // Now we can set the real timezone
  setTimezone(timezone);
}
void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time 1");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
}
void setTime(int yr, int month, int mday, int hr, int minute, int sec, int isDst) {
  struct tm tm;

  tm.tm_year = yr - 1900;  // Set date
  tm.tm_mon = month - 1;
  tm.tm_mday = mday;
  tm.tm_hour = hr;  // Set time
  tm.tm_min = minute;
  tm.tm_sec = sec;
  tm.tm_isdst = isDst;  // 1 or 0
  time_t t = mktime(&tm);
  Serial.printf("Setting time: %s", asctime(&tm));
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
}