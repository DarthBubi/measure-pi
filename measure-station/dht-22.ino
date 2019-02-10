#include "DHT.h"
#define DHTPIN 13 // D7
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// InfluxDB declaration
#include "InfluxDb.h"
#define INFLUXDB_HOST "192.168.x.x"
Influxdb influx(INFLUXDB_HOST);

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char *ssid = "YourSSID";
const char *password = "YourPW";
const int httpPort = 80;

float dht_temperature = 0;
float dht_humidity = 0;

ESP8266WebServer server(80);

unsigned long starttime;
unsigned long sampletime_ms = 30000;

const String red = "#f00";
const String green = "#0f0";

void connectWifi()
{
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
  float h = dht.readHumidity();    //Read Humidity
  float t = dht.readTemperature(); //Read Temperature
  dht_temperature = t;
  dht_humidity = h;

  // Check if valid number if non NaN (not a number) will be send.
  if (isnan(t) || isnan(h))
  {
    Serial.println("DHT22 couldn’t be read");
  }
  else
  {
    Serial.print("Humidity    : ");
    Serial.print(h);
    Serial.print(" %\n");
    Serial.print("Temperature : ");
    Serial.print(t);
    Serial.println(" C");
  }
}

void webserver_root()
{
  String page_content ="<!DOCTYPE html><html>";
  String colour_h, colour_te;

  if (dht_humidity > 60. || dht_humidity < 40)
    colour_h = red;
  else
    colour_h = green;

  if (dht_temperature > 23. || dht_temperature < 20.)
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
  page_content += "<body><h1>Measure Station Bedroom</h1>";
  page_content += "<p>Temperature: " + Float2String(dht_temperature) + " °C <span class=\"dot temp_ind\"></span> </p>";
  page_content += "<p>Humidity: " + Float2String(dht_humidity) + " % <span class=\"dot hum_ind\"></span> </p>";
  page_content += "</body></html>";

  server.send(200, "text/html; charset=utf-8", page_content);
}

void send_data_to_influxdb()
{
  String device = "esp8266-";
  device += String(ESP.getChipId());
  String node = "bedroom";

  InfluxData data("indoor_temperature");
  data.addTag("node", node);
  data.addTag("device", device);
  data.addTag("sensor", "dht22");
  data.addValue("temperature", dht_temperature);
  data.addValue("humidity", dht_humidity);
  influx.prepare(data);

  boolean success = influx.write();
  delay(5000);
  Serial.println("Wrote to InfluxDB with return code " + success);
}

void setup()
{
  Serial.begin(9600); //Output to Serial at 9600 baud
  pinMode(DHTPIN, INPUT);
  delay(10);
  starttime = millis(); // store the start time
  dht.begin();          // Start DHT
  delay(1000);
  connectWifi(); // Start ConnecWifi
  Serial.print("\n");
  Serial.println("ChipId: ");
  Serial.println(ESP.getChipId());

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
