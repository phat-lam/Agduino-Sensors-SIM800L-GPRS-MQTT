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
 * D8: soft-UART RXD pin, connect to TXD (D1) of Agduino.
 * D9: soft-UART TXD pin, connect to RXD (D0) of Agduino.
 * RXD(D0): TXD pin SIM800L.
 * TXD(D1): RXD pin SIM800L.
 * 
 * Created 7 June 2017 by AgriConnect.
 */
/* Comes with IDE */
/* SIM800L GPRS - MQTT */
#include <GSM_MQTT.h>
/* SOFT - UART */
#include <SoftwareSerial.h>
/*------------------------------- Variables & Constants -----------------------------------*/
char shttemp[4];
char shthumi[3];
char rain[2];
char soilmoisture[4];
char dstemp[4];
/*------------------------------- GSM - GPRS - MQTT ---------------------------------------*/
String MQTT_HOST = "agriconnect.vn";
String MQTT_PORT = "1883";
char* deviceName = "btnode01";
char* mqttTopic = "bt01";
char* mqttUser = "node";
char* mqttPass = "654321";
const int refStrLen = 63;
/*------------------------------- SOFT - UART ---------------------------------------------*/
// Software Serial through which mqtt events log is printed at 9600 baud rate
// (RX, TX)
SoftwareSerial agduinoSerial(9, 8);
/*------------------------------- MQTT - CONNECT-AUTHENTICATION ---------------------------*/
void GSM_MQTT::AutoConnect(void)
{
  connect(deviceName, 1, 1, mqttUser, mqttPass, 1, 0, 0, 0, "", "");
}
void GSM_MQTT::OnConnect(void)
{
  subscribe(0, _generateMessageID(), mqttTopic, 1);
}
void GSM_MQTT::OnMessage(char *Topic, int TopicLength, char *Message, int MessageLength)  {}
GSM_MQTT MQTT(90);
// 90 is the keepalive duration in seconds
/*------------------------------- SETUP FUNC - RUN ONCE -----------------------------------*/
void setup()
{
  // Initialize the SOFT-UART
  agduinoSerial.begin(9600);
  // initialize mqtt:
  // GSM modem should be connected to Harware Serial
  MQTT.begin();
}
/*------------------------------- LOOP FUNC - LOOP FOREVER --------------------------------*/
void loop()
{
  String recStr;
  //int recStrLen;
  //recStr = "{\"airT\": 334,\"airRH\": 71,\"airR\": 0,\"soilMC\":   0,\"soilT\": 317}";
  if(agduinoSerial.available() > 0)
  {
    // Read data from serial interface
    recStr = agduinoSerial.readStringUntil('\0');
    //recStrLen = recStr.length() + 1;
    // Send the string to the Hardware serial
    //Serial.println(recStr);
    //Serial.println(recStrLen);
    
    char* buff = recStr.c_str();
    strcpy(shttemp, strtok(buff, ","));
    strcpy(shthumi, strtok(NULL, ","));
    strcpy(rain, strtok(NULL, ","));
    strcpy(soilmoisture, strtok(NULL, ","));
    strcpy(dstemp, strtok(NULL, ","));

//    Serial.print("SHT-Temp: "); Serial.println(shttemp);
//    Serial.print("SHT-Humi : "); Serial.println(shthumi);
//    Serial.print("Rain: "); Serial.println(rain);
//    Serial.print("SOIL-MC : "); Serial.println(soilmoisture);
//    Serial.print("DS-Temp: "); Serial.println(dstemp);
//    Serial.println("");
    // Build json string
    String json = buildJson();
    int jsonLen = json.length() + 1;
    // Convert a the string into char-array
    char jsonStr[jsonLen];
    json.toCharArray(jsonStr, jsonLen);
    // Send JSON string to MQTT broker
    if (MQTT.available())
    {
      // Send msg
      MQTT.publish(0, 0, 0, 1, mqttTopic, jsonStr);
    }
  }
  MQTT.processing();
  delay(3000);
}
String buildJson()
{
  String data = "{";
  data += "\"airT\": ";
  data += shttemp;
  data += ",";
  data += "\"airRH\": ";
  data += shthumi;
  data += ",";
  data += "\"airR\": ";
  data += rain;
  data += ",";
  data += "\"soilMC\": ";
  data += soilmoisture;
  data += ",";
  data += "\"soilT\": ";
  data += dstemp;
  data += "}";
  return data;
}
