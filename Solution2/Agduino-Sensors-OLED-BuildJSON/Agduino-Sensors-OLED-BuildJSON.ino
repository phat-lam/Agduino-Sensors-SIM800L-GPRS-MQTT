/*
 * Simple code for wsn id 01 monitoring environmental parameters (soil & air) in fruit farm. 
 * Reads sensors and sends data to gateway by GSM/GPRS - SIM800L module. 
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
 * D8: soft-UART RXD pin, connect to TXD of GPS module.
 * D9: soft-UART TXD pin, connect to RXD of GPS module.
 * A0: Rain sensor analog output.
 * A4: OLED SDA pin.
 * A5: OLED SCL pin.
 * A6: DFrobot soil sensor analog output.
 * RXD(D0): soft-UART TXD (D9) of Agduino.
 * TXD(D1): soft-UART RXD (D8) of Agduino.
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
/* SOFT - UART */
//#include <SoftwareSerial.h>
#include <UART_ARDUINO.h>
/*------------------------------- Capacitive soil moisture sensor ------------------*/
// Capacitive soil moisture sensor
int soilPin = A6;
const int soilNum = 10;
int soilmoistureValue;
const int airValue = 530;
const int waterValue = 284;
static char soilmoistureBuff[4];
/*------------------------------- DS18B20 sensor -----------------------------------*/
#define ds_io_data 3
const int ds_index = 0;
float ds_temp;
static char dstempBuff[4];
OneWire oneWire(ds_io_data);
DallasTemperature sensors(&oneWire);
/*------------------------------- T&RH SHT11 sensor --------------------------------*/
float sht_temp;
float sht_humi;
const int sht_num = 3;
#define sht_dataPin  2
#define sht_clockPin 6
static char shttempBuff[4];
static char shthumiBuff[3];
SHT1x sht1x(sht_dataPin, sht_clockPin);
/*------------------------------- Rain sensor --------------------------------------*/
int rainSensor_pin = A0;
boolean rain_st = false;
static char rainBuff[2];
/*------------------------------- NODE ---------------------------------------------*/
char* nodeID = "01";
/*------------------------------- SOFT - UART --------------------------------------*/
// Software Serial through which mqtt events log is printed at 9600 baud rate
// (RX, TX)
//SoftwareSerial mySerial(8, 9);
/*------------------------------- TIMER --------------------------------------------*/
int timer_period = 30000;
SimpleTimer timer1;
/*------------------------------- SETUP FUNC - RUN ONCE -----------------------------------*/
void setup()
{
  // Initialize the UART
  Serial.begin(9600);
  //mySerial.begin(9600);
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
void timer_isr()
{
  // Soil SMC - T
  soilmoistureValue = (int)readSoilSensor(soilPin, soilNum, waterValue, airValue);
  ds_temp = readDs18b20(ds_index);
  // Air T&RH - Rain
  sht_temp = sht1x.readTemperatureC();
  sht_humi = sht1x.readHumidity();
  if (isnan(sht_humi) || isnan(sht_temp))
  {
    //mySerial.println("Failed to read from SHT sensor!");
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
  // Send msg by UART
  Serial.print(json);
}
/*------------------------------- SUB-FUNC_CAPACITOR SOIL MOISTURE SENSOR -------------*/
float readSoilSensor(int analogPin, int num, int waterVal, int airVal)
{
  int val = 0;
  int sum = 0;
  float SMC = 0;
  for(int i = 0; i < num; i++)
  {
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
float readDs18b20(int index)
{
  float temp;
  // request to the sensor
  sensors.requestTemperatures();
  temp = sensors.getTempCByIndex(index);
  return temp;
}
/*------------------------------- SUB-FUNC_RAIN SENSOR --------------------------------*/
boolean checkRain(int pin, int refVal)
{
  int val = analogRead(pin);
  if(val > refVal) return false;
  else return true;
}
/*------------------------------- SUB-FUNC_JSON STRING --------------------------------*/
String buildJson()
{
  String data = "{";
  data += "\"node\":";
  data += "\"";
  data += nodeID;
  data += "\"";
  data += ",";
  data += "\"aT\":";
  data += shttempBuff;
  data += ",";
  data += "\"aRH\":";
  data += shthumiBuff;
  data += ",";
  data += "\"aR\":";
  data += rainBuff;
  data += ",";
  data += "\"sMC\":";
  data += soilmoistureBuff;
  data += ",";
  data += "\"sT\":";
  data += dstempBuff;
  data += "}";
  return data;
}
