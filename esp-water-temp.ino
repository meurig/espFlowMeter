// For the WifiManager to handle custom parameters (API key)
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

// For the temp sensor we're using
#include <OneWire.h>            //https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h>

// For DHT22 Tempa nd humidity sensor
#include <DHT.h>

// For connecting to thingspeak
#include <ESP8266WiFi.h>        //https://github.com/esp8266/Arduino

// To manage the wifi password etc
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

// replace with your channel’s thingspeak API key,
//String apiKey = "XEWWEB4L1RXHUZRF";

const char* server = "api.thingspeak.com";
 
// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 2
 
// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
 
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// For DHT
#define DHTPIN 2 // what pin we’re connected to
DHT dht(DHTPIN, DHT22,15);

// No default value for the api key, user must supply
char thingspeak_api_key[20];
char sensor_type[8] = "DS"; //Default to DS18B20

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
  // start serial port
  Serial.begin(115200);
  Serial.println("temperature_esp01_ds18B20_thingspeak_wifimanager");

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
          
          strcpy(thingspeak_api_key, json["thingspeak_api_key"]);
          strcpy(sensor_type, json["sensor_type"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_thingspeak_api_key("apikey", "api key", thingspeak_api_key, 17);
  WiFiManagerParameter custom_sensor_type("sensortype", "sensor type (DS/DHT22)", sensor_type, 8);

  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_thingspeak_api_key);
  wifiManager.addParameter(&custom_sensor_type);

  //reset settings - for testing
  //wifiManager.resetSettings();

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

  if (String(sensor_type) == "DHT22")
  {
    // Start the DHT library
    delay(10);
    dht.begin();
  }
  else
  {
    // Start up the DallasTemperature library
    sensors.begin();
  }
}

void loop(void)
{
  float t;
  float h;
  if (String(sensor_type) == "DHT22")
  {
    // DHT
    h = dht.readHumidity();
    t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
  }
    else
  {
    // call sensors.requestTemperatures() to issue a global temperature
    // request to all devices on the bus
    Serial.print(" Requesting temperatures...");
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.println("DONE");
  
    Serial.print("Temperature for Device 1 is: ");
  
    // DS18B20
    t = sensors.getTempCByIndex(0);
        // Why "byIndex"? 
        // You can have more than one IC on the same bus. 
        // 0 refers to the first IC on the wire
    h = 0;
    Serial.println(t);
  }

  if (client.connect(server,80)) { // "184.106.153.149" or api.thingspeak.com
    String apiKey = String(thingspeak_api_key);
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

    Serial.print("Api Key: ");
    Serial.println(thingspeak_api_key);
    Serial.print("Sensor Type: ");
    Serial.println(sensor_type);
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.print(" degrees Celcius Humidity: ");
    Serial.print(h);
    Serial.println("% send to Thingspeak");
    Serial.println("");
  }
  client.stop();
  
  Serial.println("Waiting…");
  // thingspeak needs minimum 15 sec delay between updates
  delay(20000);
}

