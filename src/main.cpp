/*
 Sketch which publishes temperature data from a DS1820 sensor to a MQTT topic.

 This sketch goes in deep sleep mode once the temperature has been sent to the MQTT
 topic and wakes up periodically (configure SLEEP_DELAY_IN_SECONDS accordingly).

 Hookup guide:
 - connect D0 pin to RST pin in order to enable the ESP8266 to wake up periodically
 - DS18B20:
     + connect VCC (3.3V) to the appropriate DS18B20 pin (VDD)
     + connect GND to the appopriate DS18B20 pin (GND)
     + connect D4 to the DS18B20 data pin (DQ)
     + connect a 4.7K resistor between DQ and VCC.
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define SLEEP_DELAY_IN_SECONDS  60
#define ONE_WIRE_BUS            D4      // DS18B20 pin

const char* ssid = "<YOUR SSID>";
const char* password = "<YOUR WIFI PWD>";

const char* mqtt_server = "<YOUR MQTT BROKER IP OR HOST>";  // RPi
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* mqtt_topic = "sensors/test/temperature";

WiFiClient espClient;
PubSubClient client(espClient);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

char temperatureString[6];

// configure the wifi network
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi IP address: ");
  Serial.println(WiFi.localIP());
}

// Generate a device specific ID using the macAddress
String getDeviceId() {
  return ("iot_" + WiFi.macAddress());
}

// MCU Setup Handler 
void setup() {
  // setup serial port
  Serial.begin(115200);

  // setup WiFi
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  // setup OneWire bus
  DS18B20.begin();
}

// Reconnect to the MQTT Broker
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(getDeviceId().c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Read the Temperature from OneWire DS18B20
float getTemperature() {
  Serial.println("Requesting DS18B20 temperature...");
  float temp;
  do {
    DS18B20.requestTemperatures(); 
    temp = DS18B20.getTempCByIndex(0);
    delay(100);
  } while (temp == 85.0 || temp == (-127.0));
  return temp;
}

//MCU Processing Loop
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float temperature = getTemperature();
  // convert temperature to a string with two digits before the comma and 2 digits for precision
  dtostrf(temperature, 2, 2, temperatureString);
  String message = "{\"deviceID\":\"" + getDeviceId() + "\",\"tempC\":" + temperatureString + "}";
  // send temperature to the serial console
  Serial.print("Sending temperature: ");
  Serial.println(message);
  // send temperature to the MQTT topic
  client.publish(mqtt_topic, message.c_str());
  client.loop(); // without this the message never gets published.

  Serial.println("Closing MQTT connection...");
  client.disconnect();

  Serial.println("Closing WiFi connection...");
  WiFi.disconnect();

  delay(100);

  Serial.print("Entering deep sleep mode for ");
  Serial.print(SLEEP_DELAY_IN_SECONDS);
  Serial.println(" seconds...");
  ESP.deepSleep(SLEEP_DELAY_IN_SECONDS * 1000000, WAKE_RF_DEFAULT);
  //ESP.deepSleep(10 * 1000, WAKE_NO_RFCAL);
  delay(500);   // wait for deep sleep to happen
}

