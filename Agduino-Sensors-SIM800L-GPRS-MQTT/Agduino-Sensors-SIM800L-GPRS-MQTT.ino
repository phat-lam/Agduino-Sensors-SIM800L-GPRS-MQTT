/*
 * Simple code for wsn id 01 monitoring environmental parameters (soil & air) in fruit farm. 
 * Reads sensors and sends data to gateway by GSM/GPRS - SIM900A module. 
 * MQTT server: "agriConnect.vn", default port: 1883, topic: "bt01"
 * 
 * Timer period: 30 seconds in normal mode, 10 seconds during irrigation.
 * 
 * Atmega328p-au MCU with arduino bootloader (or Arduino Uno, Pro, Pro Mini).
 * Data transceiver: GSM/GPRS, frequency: ... MHz, default baud-rate: ... kbps.
 * Sensors: temperature & relative humidity SHT11, DFrobot capacitor soil moisture sensor, temperature sensor DS18B20 and rain sensor.
 * 
 * The circuit:
 * D3: Ds18b20 data pin (pull-up with 4K7 Ohm resistor).
 * D6: SHT11 clock pin.
 * D2: SHT11 data pin
 * D8: soft-UART RXD pin (fixed in AltSoftSerial lib)
 * D9: soft-UART TXD pin (fixed in AltSoftSerial lib)
 * A0: Rain sensor analog output.
 * A4: OLED SDA pin.
 * A5: OLED SCL pin.
 * A6: DFrobot soil sensor analog output.
 * RXD(D0): TXD pin (RFDI virtual-USB).
 * TXD(D1): RXD pin (RFDI virtual-USB).
 * 
 * Created 7 June 2017 by AgriConnect.
 */
/* Comes with IDE */
#include <Wire.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
/* SHT1x */
#include <SHT1x.h>
/* DS18B20 sensor lib */
#include <OneWire.h>
#include <DallasTemperature.h>
/* Timer lib */
#include <SimpleTimer.h>
/* SIM800L GPRS - MQTT */
#include <GSM_MQTT.h>
/* SOFT - UART */
#include <SoftwareSerial.h>
/*------------------------------- Capacitive soil moisture sensor ------------------*/
// Capacitive soil moisture sensor
int soilPin = A6;
const int soilNum = 10;
int soilmoistureValue;
const int airValue = 530;
const int waterValue = 284;
static char soilmoistureBuff[3];
/*------------------------------- DS18B20 sensor -----------------------------------*/
#define ds_io_data 3
const int ds_index = 0;
float ds_temp;
static char dstempBuff[3];
OneWire oneWire(ds_io_data);
DallasTemperature sensors(&oneWire);
/*------------------------------- T&RH SHT11 sensor --------------------------------*/
float sht_temp;
float sht_humi;
const int sht_num = 3;
#define sht_dataPin  2
#define sht_clockPin 6
static char shttempBuff[3];
static char shthumiBuff[2];
SHT1x sht1x(sht_dataPin, sht_clockPin);
/*------------------------------- Rain sensor --------------------------------------*/
int rainSensor_pin = A0;
boolean rain_st = false;
static char rainBuff[1];
/*------------------------------- GSM - GPRS - MQTT --------------------------------*/
String MQTT_HOST = "agriconnect.vn";
String MQTT_PORT = "1883";
char* deviceName = "btnode01";
char* mqttTopic = "bt01";
char* mqttUser = "node";
char* mqttPass = "654321";
char* msgMQTT;
/*------------------------------- SOFT - UART --------------------------------------*/
/* Software Serial through which mqtt events log is printed at 9600 baud rate */
/* (RX, TX) */
SoftwareSerial mySerial(8, 9);
/*------------------------------- TIMER --------------------------------------------*/
int timer_period = 10000;
SimpleTimer timer1;

void GSM_MQTT::AutoConnect(void)
{
  connect(deviceName, 1, 1, mqttUser, mqttPass, 1, 0, 0, 0, "", "");
}
void GSM_MQTT::OnConnect(void)
{
  subscribe(0, _generateMessageID(), mqttTopic, 1);
  publish(0, 0, 0, _generateMessageID(), mqttTopic, "Hello");
}
void GSM_MQTT::OnMessage(char *Topic, int TopicLength, char *Message, int MessageLength)
{
  mySerial.println(TopicLength);
  mySerial.println(Topic);
  mySerial.println(MessageLength);
  mySerial.println(Message);
}
GSM_MQTT MQTT(20);
// 20 is the keepalive duration in seconds
/*------------------------------- SETUP FUNC - RUN ONCE -----------------------------------*/
void setup()
{
  // initialize the soft-uart
  mySerial.begin(9600);
  // initialize mqtt:
  // GSM modem should be connected to Harware Serial
  // index =0;
  MQTT.begin();
  // Initialize the DS18B20 sensor
  sensors.begin();
  // Initialize the timers
  timer1.setInterval(timer_period, timer_isr);
}
/*------------------------------- LOOP FUNC - LOOP FOREVER --------------------------------*/
void loop()
{
  // Run timer1
  timer1.run();
}
/*------------------------------- TIMER_ISR --------------------------------------------*/
void timer_isr() {
  // Soil SMC - T
  soilmoistureValue = (int)readSoilSensor(soilPin, soilNum, waterValue, airValue);
  ds_temp = readDs18b20(ds_index);
  // Air T&RH - Rain
  sht_temp = sht1x.readTemperatureC();
  sht_humi = sht1x.readHumidity();
  if (isnan(sht_humi) || isnan(sht_temp)) {
    mySerial.println("Failed to read from SHT sensor!");
    sht_humi=0;
    sht_temp=0;
  }
  // Check rain-status
  rain_st = checkRain(rainSensor_pin, 400);
  // Convert all envi-paras into char[]
  dtostrf((int)(soilmoistureValue * 10),3,0,soilmoistureBuff);
  dtostrf((int)(ds_temp * 10),3,0,dstempBuff);
  dtostrf((int)(sht_temp * 10),3,0,shttempBuff);
  dtostrf((int)sht_humi,2,0,shthumiBuff);
  dtostrf((int)rain_st,1,0,rainBuff);
  // Build JSON string
  String json = buildJson();
  int jsonLength = json.length() + 1;
  // mySerial.print("Json string: "); mySerial.print(json);
  char jsonStr[jsonLength];
  json.toCharArray(jsonStr, jsonLength);
  // Send JSON string to MQTT broker
  if (MQTT.available())
  {
    // Send msg
    MQTT.publish(0, 0, 0, 1, mqttTopic, jsonStr);
  }
  MQTT.processing();
}
/*------------------------------- SUB-FUNC_CAPACITOR SOIL MOISTURE SENSOR -------------*/
float readSoilSensor(int analogPin, int num, int waterVal, int airVal) {
  int val = 0;
  int sum = 0;
  float SMC = 0;
  for(int i = 0; i < num; i++) {
    val = analogRead(analogPin);
    sum += val;
    delay(100);
  }
  val = (int)(sum / num);
  
  SMC = (1.0 - ((float)val - (float)waterVal) / ((float)airVal - (float)waterVal)) * 100.0;
  if(SMC > 100.0) SMC = 100.0;
  if(SMC < 0.0) SMC = 0.0;
  
  return SMC;
}
/*------------------------------- SUB-FUNC_DS18B20 ------------------------------------*/
float readDs18b20(int index) {
  float temp;
  // request to the sensor
  sensors.requestTemperatures();
  temp = sensors.getTempCByIndex(index);
  return temp;
}
/*------------------------------- SUB-FUNC_RAIN SENSOR --------------------------------*/
boolean checkRain(int pin, int refVal) {
  int val = analogRead(pin);
  if(val > refVal) return false;
  else return true;
}
/*------------------------------- SUB-FUNC_JSON STRING --------------------------------*/
String buildJson() {
  String data = "{";
  data += "\"airTemp\": ";
  data += shttempBuff;
  data += ",";
  data += "\"airHumi\": ";
  data += shthumiBuff;
  data += ",";
  data += "\"airRain\": ";
  data += rainBuff;
  data += ",";
  data += "\"soilMoistureContent\": ";
  data += soilmoistureBuff;
  data += ",";
  data += "\"soilTemp\": ";
  data += dstempBuff;
  data += "}";  
  return data;
}
