#include <DHTesp.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <JeVe_EasyOTA.h>
#include <PubSubClient.h>
#include "InfluxDb.h"
#define INFLUXDB_HOST "192.168.x.x"

const char *ssid = "YourSSID";
const char *password = "YourPW";
const char *node = "living_room";

#define HOST_NAME "measure-station-bedroom"
EasyOTA OTA(HOST_NAME);

Influxdb influx(INFLUXDB_HOST);

DHTesp dht;
TempAndHumidity dht_val;
float dew_point;

ESP8266WebServer server(80);
WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
String pub_topic, sub_topic;
String mqtt_msg;

unsigned long starttime;
unsigned long sampletime_ms = 30000;

const String red = "#f00";
const String green = "#0f0";

/* void connectWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting ");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
} */

String Float2String(float value)
{
  // Convert a float to String with two decimals.
  char temp[15];
  String s;

  dtostrf(value, 13, 2, temp);
  s = String(temp);
  s.trim();
  return s;
}

void sensorDHT()
{
  TempAndHumidity th = dht.getTempAndHumidity();

  // Check if valid number if non NaN (not a number) will be send.
  if (isnan(th.temperature) || isnan(th.humidity))
  {
    Serial.println("DHT22 couldn’t be read");
  }
  else
  {
    dht_val = th;
    dew_point = dht.computeDewPoint(dht_val.temperature, dht_val.humidity);
    Serial.print("Humidity    : ");
    Serial.print(dht_val.humidity);
    Serial.print(" %\n");
    Serial.print("Temperature : ");
    Serial.print(dht_val.temperature);
    Serial.println(" C");
  }
}

void webserver_root()
{
  String page_content = "<!DOCTYPE html><html>";
  String colour_h, colour_te;

  if (dht.isTooDry(dht_val.temperature, dht_val.humidity) || dht.isTooHumid(dht_val.temperature, dht_val.humidity))
    colour_h = red;
  else
    colour_h = green;

  if (dht.isTooCold(dht_val.temperature, dht_val.humidity) || dht.isTooHot(dht_val.temperature, dht_val.humidity))
    colour_te = red;
  else
    colour_te = green;

  page_content += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  page_content += "<link rel=\"icon\" href=\"data:,\">";
  page_content += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  page_content += ".dot {height: 20px; width: 20px; background-color: #fff; border-radius: 50%; display: inline-block;}";
  page_content += ".temp_ind {background-color: " + colour_te + "}";
  page_content += ".hum_ind {background-color: " + colour_h + "}";
  page_content += "</style></head>";

  // Web Page Heading
  page_content += "<body><h1>Measure Station Living Room</h1>";
  page_content += "<p>Temperature: " + Float2String(dht_val.temperature) + " °C <span class=\"dot temp_ind\"></span> </p>";
  page_content += "<p>Humidity: " + Float2String(dht_val.humidity) + " % <span class=\"dot hum_ind\"></span> </p>";
  page_content += "<p>Dew point: " + Float2String(dht.computeDewPoint(dht_val.temperature, dht_val.humidity)) + " °C </p>";
  page_content += "</body></html>";

  server.send(200, "text/html; charset=utf-8", page_content);
}

void send_data_to_influxdb()
{
  String device = "esp8266-";
  device += String(ESP.getChipId());

  InfluxData data("indoor_temperature");
  data.addTag("node", node);
  data.addTag("device", device);
  data.addTag("sensor", "dht22");
  data.addValue("temperature", dht_val.temperature);
  data.addValue("humidity", dht_val.humidity);
  data.addValue("dew_point", dew_point);
  influx.prepare(data);

  boolean success = influx.write();
  delay(5000);
}

void indoor_temperature_cb(char* topic, byte* payload, unsigned int length)
{
  char msg[length+1];

  for (int i = 0; i < length; i++)
    msg[i] = (char) payload[i];

  msg[length] = '\0';
  
  if (strcmp(msg, "temperature") == 0)
    mqtt_msg = Float2String(dht_val.temperature);
  else if (strcmp(msg, "humidity") == 0)
    mqtt_msg = Float2String(dht_val.humidity);
  else
  {
    Serial.print("Request not valid. Got ");
    Serial.println(msg);
    mqtt_msg = "";
  }
}

void reconnect()
{
  while (!mqtt_client.connected())
  {
    Serial.println("Reconnecting MQTT...");
    if (!mqtt_client.connect("ESP8266Client"))
    {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }

  mqtt_client.subscribe(sub_topic.c_str());
  Serial.println("MQTT Connected...");
}

void setup()
{
  Serial.begin(115200);
  delay(10);
  OTA.onMessage([](const String& message, int line)
  { Serial.println(message); });

  starttime = millis(); // store the start time
  dht.setup(13, DHTesp::DHT22);
  OTA.addAP(ssid, password);
  Serial.print("\n");
  Serial.println("ChipId: ");
  Serial.println(ESP.getChipId());

  if (!MDNS.begin(HOST_NAME))
    Serial.println("Error setting up mDNS");
  else
    Serial.println("mDNS started");

  server.on("/", webserver_root);
  server.begin();

  influx.setDb("homesensordata");

  mqtt_client.setServer(INFLUXDB_HOST, 1883);
  mqtt_client.setCallback(indoor_temperature_cb);
  pub_topic = "/home/measure/" + (String) node + "/value";
  sub_topic = "/home/measure/" + (String) node;
}

void loop()
{
  OTA.loop();
  
  // Checking if it is time to sample
  if ((millis() - starttime) > sampletime_ms)
  {
    starttime = millis(); // store the start time
    sensorDHT();          // getting temperature and humidity
    Serial.println("------------------------------");
    send_data_to_influxdb();
  }

  if (!mqtt_client.connected())
    reconnect();
  mqtt_client.loop();

  if (strcmp(mqtt_msg.c_str(), "") != 0)
  {
    mqtt_client.publish(pub_topic.c_str(), mqtt_msg.c_str());
    mqtt_msg = "";
  }

  // handle client connections
  server.handleClient();
}
