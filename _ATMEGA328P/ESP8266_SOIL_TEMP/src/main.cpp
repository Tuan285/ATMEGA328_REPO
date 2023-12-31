#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 20, 4);
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

#define WIFI_SSID "BeNgocCute"
#define WIFI_PASSWORD "12345678"

#define MQTT_HOST "test.mosquitto.org"
#define MQTT_PORT 1883

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;
//------------------------ TOPIC --------------------
#define MANUAL "MANUAL"
#define MOTOR "MOTOR"
#define TEMPERATURE "TEMPERATURE"
#define SOIL "SOIL"
#define MINTEMPERATURE "MINTEMP"
#define MAXTEMPERATURE "MAXTEMP"
#define MINSOIL "MINSOIL"
#define MAXSOIL "MAXSOIL"
//--------------------- DEFINE DS18B20---------------
const int oneWireBus = 4;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
//-------------------- MODULE ----------------
#define RELAY 2
#define CHOOSE 36
#define UP 39
#define DOWN 34
#define pinSOIL 36
//-------------------- DEFINE DATA --------------
uint32_t prevTimePubTempSoil, prevTimePubData;
uint32_t prevTimeLcd;
typedef struct
{
  float tempC;
  float Soil;

  int minSoil;
  int maxSoil;

  int minTempC;
  int maxTempC;

  bool manual;
  bool motor;

} Data_t;

typedef struct
{
  uint16_t releaseTime;
  uint16_t pressTime;
  bool lastState;
  bool currState;
  bool isPress;
  bool isLongDetect;

} Button_t;
typedef struct
{
  bool modeSetup;
  uint8_t mode;
} State_t;
State_t state;
Data_t data;
Button_t BTchoose, BTup, BTdown;

uint16_t tt = 0;
bool tempSoilChange = 0, manualMotorChange = 0;

void connectToWifi()
{
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event)
{
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event)
  {
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    connectToMqtt();
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi lost connection");
    xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    xTimerStart(wifiReconnectTimer, 0);
    break;
  }
}

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  uint16_t packetIdSub1 = mqttClient.subscribe(MANUAL, 2);
  uint16_t packetIdSub2 = mqttClient.subscribe(MOTOR, 2);
  uint16_t packetIdSub3 = mqttClient.subscribe(MINTEMPERATURE, 2);
  uint16_t packetIdSub4 = mqttClient.subscribe(MAXTEMPERATURE, 2);
  uint16_t packetIdSub5 = mqttClient.subscribe(MINSOIL, 2);
  uint16_t packetIdSub6 = mqttClient.subscribe(MAXSOIL, 2);
  // Serial.print("Subscribing at QoS 2, packetId: ");
  // Serial.println(packetIdSub1);
  // Serial.print("Subscribing at QoS 2, packetId: ");
  // Serial.println(packetIdSub2);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId)
{
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  // Serial.println("Publish received.");
  Serial.print("Topic: ");
  Serial.println(topic);
  // Serial.print("Message received :");
  String messageTemp;
  for (int i = 0; i < len; i++)
  {
    messageTemp += (char)payload[i];
  }
  // Serial.println(messageTemp);

  if (String(topic) == MANUAL)
  {
    data.manual = messageTemp.toInt();
    lcd.clear();
    delay(10);
  }
  else if (String(topic) == MOTOR)
  {
    data.motor = messageTemp.toInt();
  }
  else if (String(topic) == MINTEMPERATURE)
  {
    data.minTempC = messageTemp.toFloat();
  }
  else if (String(topic) == MAXTEMPERATURE)
  {
    data.maxTempC = messageTemp.toFloat();
  }
  else if (String(topic) == MINSOIL)
  {
    data.minSoil = messageTemp.toFloat();
  }
  else if (String(topic) == MAXSOIL)
  {
    data.maxSoil = messageTemp.toFloat();
  }
}

void onMqttPublish(uint16_t packetId)
{
  // Serial.println("Publish acknowledged.");
  // Serial.print("  packetId: ");
  // Serial.println(packetId);
}

void initMQTTWiFi()
{
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();
}

void initModule()
{
  lcd.init(); // initialize the lcd
  lcd.backlight();
  sensors.begin();

  pinMode(RELAY, OUTPUT);
  pinMode(CHOOSE, INPUT);
  pinMode(UP, INPUT);
  pinMode(DOWN, INPUT);
  pinMode(pinSOIL, INPUT);
}

bool checkData(float value, float valueMin, float valueMax)
{
  bool error;
  if (value > valueMax || value < valueMin)
  {
    error = 1;
  }
  else
  {
    error = 0;
  }
  return error;
}

void stateButton()
{
  lcd.setCursor(0, 1);
  lcd.print("MIN:");

  lcd.setCursor(11, 1);
  lcd.print("MAX:");

  lcd.setCursor(0, 3);
  lcd.print("MIN:");

  lcd.setCursor(11, 3);
  lcd.print("MAX:");

  if (state.modeSetup == 1)
  {
    if (tt % 2 == 0)
    {
      switch (state.mode)
      {
      case 0:
        lcd.setCursor(4, 1);
        lcd.print(data.minTempC);
        break;
      case 1:
        lcd.setCursor(15, 1);
        lcd.print(data.maxTempC);
        break;
      case 2:
        lcd.setCursor(4, 3);
        lcd.print(data.minSoil);
        break;
      case 3:
        lcd.setCursor(15, 3);
        lcd.print(data.maxSoil);
        break;
      default:
        break;
      }
    }
    else
    {
      switch (state.mode)
      {
      case 0:
        lcd.setCursor(4, 1);
        lcd.print("     ");
        break;
      case 1:
        lcd.setCursor(15, 1);
        lcd.print("     ");
        break;
      case 2:
        lcd.setCursor(4, 3);
        lcd.print("     ");
        break;
      case 3:
        lcd.setCursor(15, 3);
        lcd.print("     ");
        break;
      default:
        break;
      }
    }
  }
  else if (state.modeSetup == 0)
  {
    lcd.setCursor(4, 1);
    lcd.print(data.minTempC);
    lcd.setCursor(15, 1);
    lcd.print(data.maxTempC);
    lcd.setCursor(4, 3);
    lcd.print(data.minSoil);
    lcd.setCursor(15, 3);
    lcd.print(data.maxSoil);
  }
}

void lcdShowMain()
{
  // WARNING
  // NORMAL.
  lcd.setCursor(0, 0);
  lcd.print("T: ");
  lcd.print(data.tempC);
  if (checkData(data.tempC, data.minTempC, data.maxTempC) == 1)
  {
    lcd.setCursor(12, 0);
    lcd.print(" WARNING");
  }
  else if (checkData(data.tempC, data.minTempC, data.maxTempC) == 0)
  {
    lcd.setCursor(12, 0);
    lcd.print(" NORMAL!");
  }

  lcd.setCursor(0, 2);
  lcd.print("S: ");
  lcd.print(data.Soil);
  if (checkData(data.Soil, data.minSoil, data.maxSoil) == 1)
  {
    lcd.setCursor(12, 2);
    lcd.print(" WARNING");
  }
  else if (checkData(data.Soil, data.minSoil, data.maxSoil) == 0)
  {
    lcd.setCursor(12, 2);
    lcd.print(" NORMAL!");
  }
  if (data.manual == 1)
  {
    stateButton();
  }
}

void buttonChoose()
{
  BTchoose.currState = digitalRead(CHOOSE);
  if (BTchoose.lastState == HIGH && BTchoose.currState == LOW)
  {
    BTchoose.pressTime = millis();
    BTchoose.isPress = true;
    BTchoose.isLongDetect = false;
    Serial.println("Press");
  }
  else if (BTchoose.lastState == LOW && BTchoose.currState == HIGH)
  {
    BTchoose.releaseTime = millis();
    BTchoose.isPress = false;
    Serial.println("Release");
    if (BTchoose.releaseTime - BTchoose.pressTime > 2000)
    {
      if (state.modeSetup == 0)
      {
        state.mode = 0;
        state.modeSetup = 1;
      }
      else if (state.modeSetup == 1)
      {
        tempSoilChange = 1;
        state.modeSetup = 0;
        Serial.println("Long press 1s !!!");
      }
    }
    else if (BTchoose.releaseTime - BTchoose.pressTime > 10)
    {
      Serial.println("Choose Short !");
      state.mode++;
      lcd.setCursor(4, 1);
      lcd.print(data.minTempC);
      lcd.setCursor(15, 1);
      lcd.print(data.maxTempC);
      lcd.setCursor(4, 3);
      lcd.print(data.minSoil);
      lcd.setCursor(15, 3);
      lcd.print(data.maxSoil);
      tt = 0;
      if (state.mode >= 4)
      {
        state.mode = 0;
      }
    }
  }
  BTchoose.lastState = BTchoose.currState;
}

void buttonUp()
{
  BTup.currState = digitalRead(UP);
  if (BTup.lastState == HIGH && BTup.currState == LOW)
  {
    Serial.println("Press");
    BTup.pressTime = millis();
  }
  else if (BTup.lastState == LOW && BTup.currState == HIGH)
  {
    Serial.println("Release");
    BTup.releaseTime = millis();

    if ((BTup.releaseTime - BTup.pressTime) > 2000)
    {
      lcd.clear();
      manualMotorChange = 1;
      if (data.manual == 0)
      {
        data.manual = 1;
      }
      else if (data.manual == 1)
      {
        data.manual = 0;
      }
      Serial.println("manual on/off");
    }
    else if ((BTup.releaseTime - BTup.pressTime) > 5)
    {
      if (state.modeSetup == 1)
      {
        if (state.mode == 0)
        {
          data.minTempC++;
        }
        else if (state.mode == 1)
        {
          data.maxTempC++;
        }
        else if (state.mode == 2)
        {
          data.minSoil++;
        }
        else if (state.mode == 3)
        {
          data.maxSoil++;
        }
      }
      Serial.println("Button Up short");
    }
  }
  BTup.lastState = BTup.currState;
}

void buttonDown()
{
  BTdown.currState = digitalRead(DOWN);
  if (BTdown.lastState == HIGH && BTdown.currState == LOW)
  {
    Serial.println("Press");
    BTdown.pressTime = millis();
  }
  else if (BTdown.lastState == LOW && BTdown.currState == HIGH)
  {
    Serial.println("Release");
    BTdown.releaseTime = millis();
    if ((BTdown.releaseTime - BTdown.pressTime) > 5)
    {
      if (data.manual == 0)
      {
        if (data.motor == 1)
        {
          data.motor = 0;
        }
        else if (data.motor == 0)
        {
          data.motor = 1;
        }
      }
      if (state.modeSetup == 1)
      {
        if (state.mode == 0)
        {
          data.minTempC--;
        }
        else if (state.mode == 1)
        {
          data.maxTempC--;
        }
        else if (state.mode == 2)
        {
          data.minSoil--;
        }
        else if (state.mode == 3)
        {
          data.maxSoil--;
        }
      }
      Serial.println("Button Down short");
    }
  }
  BTdown.lastState = BTdown.currState;
}

void setup()
{
  initModule();
  initMQTTWiFi();
  Serial.begin(115200);
  Serial.println();

  BTchoose.lastState = HIGH;
  BTup.lastState = HIGH;
  BTup.lastState = LOW;
  state.modeSetup = 0;
  data.manual = 0;
  data.motor = 1;
}

void loop()
{
  buttonDown();
  buttonUp();
  buttonChoose();
  digitalWrite(RELAY, data.motor);
  if (millis() - prevTimeLcd >= 200)
  {
    tt++;
    lcdShowMain();
  }
  sensors.requestTemperatures();
  data.tempC = sensors.getTempCByIndex(0);
  if (data.tempC < -100)
  {
    data.tempC = 0;
  }
  // data.tempC = random(0, 30);
  data.Soil = map(analogRead(35), 0, 4095, 0, 100);
  // data.Soil = random(30, 60);
  if (millis() - prevTimePubTempSoil >= 4000)
  {
    prevTimePubTempSoil = millis();
    mqttClient.publish(SOIL, 2, true, String(data.Soil).c_str());
    mqttClient.publish(TEMPERATURE, 2, true, String(data.tempC).c_str());
  }
  if (tempSoilChange == 1)
  {
    mqttClient.publish(MINTEMPERATURE, 2, true, String(data.minTempC).c_str());
    mqttClient.publish(MAXTEMPERATURE, 2, true, String(data.maxTempC).c_str());
    mqttClient.publish(MINSOIL, 2, true, String(data.minSoil).c_str());
    mqttClient.publish(MAXSOIL, 2, true, String(data.maxSoil).c_str());

    tempSoilChange = 0;
  }
  if (manualMotorChange == 1)
  {
    mqttClient.publish(MANUAL, 2, true, String(data.manual).c_str());
    mqttClient.publish(MOTOR, 2, true, String(data.motor).c_str());
    manualMotorChange = 0;
  }
}
