#include <DHTesp.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include "InfluxDb.h"
#define INFLUXDB_HOST "192.168.x.x"

const char *ssid = "YourSSID";
const char *password = "YourPW";

Influxdb influx(INFLUXDB_HOST);

DHTesp dht;
TempAndHumidity dht_val;

ESP8266WebServer server(80);

unsigned long starttime;
unsigned long sampletime_ms = 30000;

const String red = "#f00";
const String green = "#0f0";

void connectWifi()
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
}

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
  String node = "living_room";

  InfluxData data("indoor_temperature");
  data.addTag("node", node);
  data.addTag("device", device);
  data.addTag("sensor", "dht22");
  data.addValue("temperature", dht_val.temperature);
  data.addValue("humidity", dht_val.humidity);
  influx.prepare(data);

  boolean success = influx.write();
  delay(5000);
  Serial.println("Wrote to InfluxDB with return code " + success);
}

void setup()
{
  Serial.begin(115200);
  delay(10);
  starttime = millis(); // store the start time
  dht.setup(13, DHTesp::DHT22);
  connectWifi();
  Serial.print("\n");
  Serial.println("ChipId: ");
  Serial.println(ESP.getChipId());

  if (!MDNS.begin("measure-station-living-room"))
    Serial.println("Error setting up mDNS");
  else
    Serial.println("mDNS started");

  server.on("/", webserver_root);
  server.begin();

  influx.setDb("homesensordata");
}

void loop()
{
  // Checking if it is time to sample
  if ((millis() - starttime) > sampletime_ms)
  {
    starttime = millis(); // store the start time
    sensorDHT();          // getting temperature and humidity
    Serial.println("------------------------------");
    send_data_to_influxdb();
  }

  // handle client connections
  server.handleClient();
}
