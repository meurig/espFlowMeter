// For the temp sensor we're using
#include <OneWire.h>
#include <DallasTemperature.h>

// For connecting to thingspeak
#include <ESP8266WiFi.h>

// To manage the wifi password etc
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager


// replace with your channel’s thingspeak API key,
String apiKey = "XEWWEB4L1RXHUZRF";

const char* server = "api.thingspeak.com";
 
// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 2
 
// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
 
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

WiFiClient client;
 
void setup(void)
{
  // start serial port
  Serial.begin(115200);
  Serial.println("temperature_esp01_ds18B20_thingspeak_wifimanager");

  // Start up the library
  sensors.begin();

  WiFiManager wifiManager;

  wifiManager.autoConnect();
  Serial.println("connected...yeey :)");
}
 
 
void loop(void)
{
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  Serial.print(" Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");

  Serial.print("Temperature for Device 1 is: ");

  float t = sensors.getTempCByIndex(0); // Why "byIndex"? 
  float h = 0;
  
  Serial.print(t);
    // You can have more than one IC on the same bus. 
    // 0 refers to the first IC on the wire
    
  if (client.connect(server,80)) { // "184.106.153.149" or api.thingspeak.com
    String postStr = apiKey;
    postStr +="&field1=";
    postStr += String(t);
    postStr +="&field2=";
    postStr += String(h);
    postStr += "\r\n\r\n";
    
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.print(" degrees Celcius Humidity: ");
    Serial.print(h);
    Serial.println("% send to Thingspeak");
  }
  client.stop();
  
  Serial.println("Waiting…");
  // thingspeak needs minimum 15 sec delay between updates
  delay(20000);
}

