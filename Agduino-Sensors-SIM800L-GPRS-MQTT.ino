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
#include <SPI.h>
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
/*------------------------------- DS18B20 sensor -----------------------------------*/
#define ds_io_data 3
const int ds_index = 0;
float ds_temp;
OneWire oneWire(ds_io_data);
DallasTemperature sensors(&oneWire);
/*------------------------------- T&RH SHT11 sensor --------------------------------*/
float sht_temp;
float sht_humi;
const int sht_num = 3;
#define sht_dataPin  2
#define sht_clockPin 6
SHT1x sht1x(sht_dataPin, sht_clockPin);
/*------------------------------- Rain sensor --------------------------------------*/
int rainSensor_pin = A0;
boolean rain_st = false;
/*------------------------------- GSM - GPRS - MQTT --------------------------------*/
String MQTT_HOST = "agriconnect.vn";
String MQTT_PORT = "1883";
char* deviceName = "btnode01";
char* mqttTopic = "bt01";
char* mqttUser = "node";
char* mqttPass = "654321";
String sosNum = "ATD988041419;";
/*------------------------------- SOFT - UART --------------------------------------*/
/* Software Serial through which mqtt events log is printed at 9600 baud rate */
/* (RX, TX) */
SoftwareSerial mySerial(8, 9);
/*------------------------------- TIMER --------------------------------------------*/
int timer_period = 5000;
SimpleTimer timer1;

void GSM_MQTT::AutoConnect(void)
{
  /*
     Use this function if you want to use autoconnect(and auto reconnect) facility
     This function is called whenever TCP connection is established (or re-established).
     put your connect codes here.
  */
  connect(deviceName, 1, 1, mqttUser, mqttPass, 1, 0, 0, 0, "", "");
  /*    void connect(char *ClientIdentifier, char UserNameFlag, char PasswordFlag, char *UserName, char *Password, char CleanSession, char WillFlag, char WillQoS, char WillRetain, char *WillTopic, char *WillMessage);
          ClientIdentifier  :Is a string that uniquely identifies the client to the server.
                            :It must be unique across all clients connecting to a single server.(So it will be better for you to change that).
                            :It's length must be greater than 0 and less than 24
                            :Example "qwerty"
          UserNameFlag      :Indicates whether UserName is present
                            :Possible values (0,1)
                            :Default value 0 (Disabled)
          PasswordFlag      :Valid only when  UserNameFlag is 1, otherwise its value is disregarded.
                            :Indicates whether Password is present
                            :Possible values (0,1)
                            :Default value 0 (Disabled)
          UserName          :Mandatory when UserNameFlag is 1, otherwise its value is disregarded.
                            :The UserName corresponding to the user who is connecting, which can be used for authentication.
          Password          :alid only when  UserNameFlag and PasswordFlag are 1 , otherwise its value is disregarded.
                            :The password corresponding to the user who is connecting, which can be used for authentication.
          CleanSession      :If not set (0), then the server must store the subscriptions of the client after it disconnects.
                            :If set (1), then the server must discard any previously maintained information about the client and treat the connection as "clean".
                            :Possible values (0,1)
                            :Default value 1
          WillFlag          :This flag determines whether a WillMessage published on behalf of the client when client is disconnected involuntarily.
                            :If the WillFlag is set, the WillQoS, WillRetain, WillTopic, WilMessage fields are valid.
                            :Possible values (0,1)
                            :Default value 0 (Disables will Message)
          WillQoS           :Valid only when  WillFlag is 1, otherwise its value is disregarded.
                            :Determines the QoS level of WillMessage
                            :Possible values (0,1,2)
                            :Default value 0 (QoS 0)
          WillRetain        :Valid only when  WillFlag is 1, otherwise its value is disregarded.
                            :Determines whether the server should retain the Will message.
                            :Possible values (0,1)
                            :Default value 0
          WillTopic         :Mandatory when  WillFlag is 1, otherwise its value is disregarded.
                            :The Will Message will published to this topic (WillTopic) in case of involuntary client disconnection.
          WillMessage       :Mandatory when  WillFlag is 1, otherwise its value is disregarded.
                            :This message (WillMessage) will published to WillTopic in case of involuntary client disconnection.
  */
}
void GSM_MQTT::OnConnect(void)
{
  /*
     This function is called when mqqt connection is established.
     put your subscription publish codes here.
  */
  subscribe(0, _generateMessageID(), mqttTopic, 1);
  /*    void subscribe(char DUP, unsigned int MessageID, char *SubTopic, char SubQoS);
          DUP       :This flag is set when the client or server attempts to re-deliver a SUBSCRIBE message
                    :This applies to messages where the value of QoS is greater than zero (0)
                    :Possible values (0,1)
                    :Default value 0
          Message ID:The Message Identifier (Message ID) field
                    :Used only in messages where the QoS levels greater than 0 (SUBSCRIBE message is at QoS =1)
          SubTopic  :Topic names to which  subscription is needed
          SubQoS    :QoS level at which the client wants to receive messages
                    :Possible values (0,1,2)
                    :Default value 0
  */
  publish(0, 0, 0, _generateMessageID(), mqttTopic, "{\"aT\":\"123\",\"aRH\":\"12\"}");
  /*  void publish(char DUP, char Qos, char RETAIN, unsigned int MessageID, char *Topic, char *Message);
      DUP       :This flag is set when the client or server attempts to re-deliver a PUBLISH message
                :This applies to messages where the value of QoS is greater than zero (0)
                :Possible values (0,1)
                :Default value 0
      QoS       :Quality of Service
                :This flag indicates the level of assurance for delivery of a PUBLISH message
                :Possible values (0,1,2)
                :Default value 0
      RETAIN    :if the Retain flag is set (1), the server should hold on to the message after it has been delivered to the current subscribers.
                :When a new subscription is established on a topic, the last retained message on that topic is sent to the subscriber
                :Possible values (0,1)
                :Default value 0
      Message ID:The Message Identifier (Message ID) field
                :Used only in messages where the QoS levels greater than 0
      Topic     :Publishing topic
      Message   :Publishing Message
  */
}
void GSM_MQTT::OnMessage(char *Topic, int TopicLength, char *Message, int MessageLength)
{
  /*
    This function is called whenever a message received from subscribed topics
    put your subscription publish codes here.
  */

  /*
     Topic        :Name of the topic from which message is coming
     TopicLength  :Number of characters in topic name
     Message      :The containing array
     MessageLength:Number of characters in message
  */
  mySerial.println(TopicLength);
  mySerial.println(Topic);
  mySerial.println(MessageLength);
  mySerial.println(Message);
}

GSM_MQTT MQTT(20);
/* 20 is the keepalive duration in seconds */
/*------------------------------- SETUP FUNC - RUN ONCE -----------------------------------*/
void setup()
{
  /* 
   initialize mqtt:
   GSM modem should be connected to Harware Serial
   index =0;
  */ 
  MQTT.begin();
  /* Initialize the DS18B20 sensor */
  sensors.begin();
  /* Initialize the timers */
  timer1.setInterval(timer_period, timer_isr);
}
/*------------------------------- LOOP FUNC - LOOP FOREVER --------------------------------*/
void loop()
{
  /* Run timer1 */
  timer1.run();
  /*
     You can write your code here
  */
}
void timer_isr() {
  //char* json = "";
  String json = buildJson();
  int jsonLength = json.length();
  char jsonStr[jsonLength];
  json.toCharArray(jsonStr, jsonLength);
  // Send JSON string to MQTT broker
  if (MQTT.available())
  {
    // Send msg
    MQTT.publish(0, 0, 0, 1, mqttTopic, "{\"aT\":\"123\",\"aRH\":\"12\"}");
  }
  MQTT.processing();
}
/*------------------------------- TIMER_ISR --------------------------------------------*/
/*
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
  // Send data by UART interface
  //  mySerial.print(soilmoistureValue); mySerial.print("%");
  //  mySerial.print("      "); mySerial.print(sht_temp, DEC); mySerial.print("oC");
  //  mySerial.print("      "); mySerial.print(sht_humi); mySerial.print("%");
  //  mySerial.print("      "); mySerial.print(ds_temp); mySerial.print("oC");
  //  mySerial.print("      "); mySerial.print(rain_st, DEC);
  //  mySerial.println();
  // Build JSON string
  String json = buildJson();
  //  mySerial.print("Json string: "); mySerial.print(json); 
  char jsonStr[300];
  json.toCharArray(jsonStr, 300);
  // Send JSON string to MQTT broker
  if (MQTT.available())
  {
    // Send msg
    MQTT.publish(0, 0, 0, 1, mqttTopic, jsonStr);
    delay(3000);
  }
  MQTT.processing();
}
*/
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
  data += "\"aT\":";
  data += "30";
  data += ",";
  
  data += "\"aRH\":";
  data += "27";
  data += "}";
  
  return data;
}
/*
String buildJson() {
  String data = "{";
  data += "\n";
  data += "\"d\": {";
  data += "\n";
  data += "\"Id\": \"nBT01\",";
  data += "\n";
  data += "\"aT\": ";
  //data += sht_temp;
  data += ",";
  data += "\n";
  
  data += "\"aRH\": ";
  //data += sht_humi;
  data += ",";
  data += "\n";

  data += "\"aR\": ";
  //data += rain_st;
  data += ",";
  data += "\n";

//  data += "\"Wind_Speed\": ";
//  data += (int)wind_speed;
//  data += ",";
//  data += "\n";

//  data += "\"Wind_Derection\": ";
//  data += (int)wind_derection;
//  data += ",";
//  data += "\n";

  data += "\"sSMC\": ";
  //data += soilmoistureValue;
  data += ",";
  data += "\n";

//  data += "\"Pump_Status\": ";
//  data += false;
//  data += ",";
//  data += "\n";

//  data += "\"Light_Status\": ";
//  data += false;
//  data += ",";
//  data += "\n";

  data += "\"sT\": ";
  //data += ds_temp;
  data += "\n";
  data += "}";
  data += "\n";
  data += "}";
  
  return data;
}
*/
