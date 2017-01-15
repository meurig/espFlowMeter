/*
Based on: YF‐ S201 Water Flow Sensor
Water Flow Sensor output processed to read in litres/hour
By: www.hobbytronics.co.uk
*/

// For the WifiManager to handle custom parameters (API key)
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

// For connecting to thingspeak
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

// To manage the wifi password etc
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#define RECONFIG_BUTTON_PIN 0
volatile byte reconfig_state = HIGH;

//Flow measuring stuff
volatile int flow_frequency; // Measures flow sensor pulses
unsigned int l_hour; // Calculated litres/hour
unsigned char flowsensor = 2; // Sensor Input
unsigned long currentTime;
unsigned long cloopTime;
unsigned int loop_count = 0;
unsigned long cummFlow20s = 0;
void flow () // Interrupt function
{
   flow_frequency++;
}

const char* server = "api.thingspeak.com";

// No default value for the api key, user must supply
char thingspeak_api_key[20];
char sensor_type[8] = "YF-S201"; //Default to YF-S201

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

WiFiClient client;
 
void setup(void)
{
  pinMode(RECONFIG_BUTTON_PIN, INPUT_PULLUP); // button default is HIGH
  attachInterrupt(RECONFIG_BUTTON_PIN, pressedButton, FALLING); 
  
  // start serial port
  Serial.begin(115200);
  Serial.println("waterflow_YF-S201_thingspeak_wifimanager");

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    /*
    if (SPIFFS.exists("/config.json")) {
      //file exists - kill it! to be used during testing
      Serial.println("removing config file");
      SPIFFS.remove("/config.json");
    }
    */
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          Serial.println("DEBUG A");
          strcpy(thingspeak_api_key, json["thingspeak_api_key"]);
          Serial.println("DEBUG B");
          strcpy(sensor_type, json["sensor_type"]);
          Serial.println("DEBUG C");
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  Serial.println("DEBUG 1");
  
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_thingspeak_api_key("apikey", "api key", thingspeak_api_key, 17);
  WiFiManagerParameter custom_sensor_type("sensortype", "sensor type (DS/DHT22)", sensor_type, 8);

  WiFiManager wifiManager;

  Serial.println("DEBUG 2");
  
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_thingspeak_api_key);
  wifiManager.addParameter(&custom_sensor_type);

  //reset settings - for testing
  //wifiManager.resetSettings();

  Serial.println("DEBUG 3");
  
  wifiManager.autoConnect();

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  strcpy(thingspeak_api_key, custom_thingspeak_api_key.getValue());
  strcpy(sensor_type, custom_sensor_type.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["thingspeak_api_key"] = thingspeak_api_key;
    json["sensor_type"] = sensor_type;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  if (String(sensor_type) == "YF-S201")
  {
    // initialise code for flow meter
    pinMode(flowsensor, INPUT);
    digitalWrite(flowsensor, HIGH); // Optional Internal Pull-Up
    //Serial.begin(9600);
    attachInterrupt(flowsensor, flow, RISING); // Setup Interrupt
    sei(); // Enable interrupts
    currentTime = millis();
    cloopTime = currentTime;
  }
  else
  {
    // No other sensor types available yet
  }
}


void loop(void)
{
  currentTime = millis();
  // Every second, calculate and print litres/hour
  if(currentTime >= (cloopTime + 1000))
  {
    cloopTime = currentTime; // Updates cloopTime
    // Pulse frequency (Hz) = 7.5Q, Q is flow rate in L/min.
    l_hour = (flow_frequency * 60 / 7.5); // (Pulse frequency x 60 min) / 7.5Q = flowrate in L/hour
    flow_frequency = 0; // Reset Counter
    Serial.print(l_hour, DEC); // Print litres/hour
    Serial.println(" L/hour");
  }
  // Keep track of the cummulative flow rates
  cummFlow20s += l_hour;
  
  // every 20 seconds, post the average of the last 20s worth of readings
  if (loop_count = 20)
  {
    int avgFlow20s = cummFlow20s / 20;
    
    if (client.connect(server,80)) { // "184.106.153.149" or api.thingspeak.com
      String apiKey = String(thingspeak_api_key);
      String postStr = apiKey;
      postStr +="&field1=";
      postStr += String(avgFlow20s);
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
  
      Serial.print("Api Key: ");
      Serial.println(thingspeak_api_key);
      Serial.print("Sensor Type: ");
      Serial.println(sensor_type);
      Serial.print("Flow Rate (20s average): ");
      Serial.print(avgFlow20s);
      Serial.println("% send to Thingspeak");
      Serial.println("");
    }
    client.stop();
    
    Serial.println("Waiting…");
    // thingspeak needs minimum 15 sec delay between updates
    // but we're handling this above, outside this if
    // delay(20000);

    // reset cummulative flow and loop counter
    loop_count = 0;
    cummFlow20s = 0;
  }

  if (reconfig_state == LOW)
  {
    // If button is held low, delete the config file
    if (digitalRead(RECONFIG_BUTTON_PIN) == LOW)
    {
      delay(2000); // Make sure it really is held low and not just bad timing
    }
    if (digitalRead(RECONFIG_BUTTON_PIN) == LOW)
    {
      if (SPIFFS.exists("/config.json")) {
        //file exists - kill it! to be used during testing
        Serial.println("removing config file");
        SPIFFS.remove("/config.json");
        
        // Ensure the button's been released before restarting to avoid reflash boot mode
        while (digitalRead(RECONFIG_BUTTON_PIN) == LOW)
        {
          Serial.println("Release button to restart");
          delay(1000);
        }
      }
    }

    WiFi.disconnect(); // will erase ssid/password
  
    ESP.restart();
    delay(1000);
  }
  loop_count++;
}

void pressedButton() {
  if (reconfig_state != LOW)
  {
    Serial.println(F("BUTTON PRESSED !!!"));
    reconfig_state = LOW;   // interrupt service routine (ISR) can ONLY modify VOLATILE variables
  }
}
