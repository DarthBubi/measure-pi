#include <DHTesp.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <InfluxDb.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
// Include Update server
#ifdef ESP8266
# include <ESP8266HTTPUpdateServer.h>
#elif defined(ESP32)
# include <IotWebConfESP32HTTPUpdateServer.h>
#endif

#define CONFIG_VERSION "measure-station-v1.1"
#define STATUS_PIN LED_BUILTIN
#define CONFIG_PIN D2
#define STR_LEN 128
#define NUMBER_LEN 32

const char* html_root_header_w_style PROGMEM = "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">" \
                                               "<link rel=\"icon\" href=\"data:,\">"\
                                               "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}"\
                                               ".dot {height: 20px; width: 20px; background-color: #fff; border-radius: 50%; display: inline-block;}";
const char* html_root_header_end PROGMEM = "</style></head>";
const char* html_root_body_config_button PROGMEM = "<a href=\"config\"><button type=\"button\">Config</button></a>";
const char* html_root_body_end_tag PROGMEM = "</body></html>";

const char intial_host_name[] = "measure-station-test";
const char initial_pw[] = "test1234";
const bool debug_enabled = false;

namespace cfg
{
  char* hostname;
  char node[STR_LEN];
  char influxdb_host[STR_LEN];
  char influxdb_database[STR_LEN];
  char mqtt_server[STR_LEN];
  char disable_status_led[NUMBER_LEN];
}

static char ledStatusValues[][NUMBER_LEN] = { "0", "1" };
static char ledStatusNames[][STR_LEN] = { "Enabled", "Disabled" };

DHTesp dht;
TempAndHumidity dht_val;
float dew_point;

DNSServer dns_server;
ESP8266WebServer server(80);
WiFiClient wifi_client;

// Create Update Server
#ifdef ESP8266
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
HTTPUpdateServer httpUpdater;
#endif

Influxdb* influx;
PubSubClient mqtt_client(wifi_client);
IotWebConf iotWebConf(intial_host_name, &dns_server, &server, initial_pw, CONFIG_VERSION);
iotwebconf::ParameterGroup settingsGroup = iotwebconf::ParameterGroup("settingsGroup", "Measure Station Settings");
iotwebconf::TextParameter influxdb_host_param = iotwebconf::TextParameter("InfluxDB Host", "influxdb_host", cfg::influxdb_host, STR_LEN);
iotwebconf::TextParameter influxdb_database_param = iotwebconf::TextParameter("InfluxDB Database", "influxdb_database", cfg::influxdb_database, STR_LEN);
iotwebconf::TextParameter mqtt_server_param = iotwebconf::TextParameter("MQTT Server", "mqtt_server", cfg::mqtt_server, STR_LEN);
iotwebconf::TextParameter hostname_param = iotwebconf::TextParameter("Node (e.g. the room)", "node", cfg::node, STR_LEN);
//iotwebconf::NumberParameter disable_status_led_param = iotwebconf::NumberParameter("Disable status LED", "disable_status_led", cfg::disable_status_led, NUMBER_LEN, "number", "0..1", "min='0' max='1' step='1'");
iotwebconf::SelectParameter disable_status_led_param = iotwebconf::SelectParameter("Status LED", "disable_status_led", cfg::disable_status_led, NUMBER_LEN, (char*)ledStatusValues, (char*)ledStatusNames, sizeof(ledStatusValues) / NUMBER_LEN, STR_LEN);


String pub_topic_temp, pub_topic_humid, sub_topic;
String mqtt_msg;

unsigned long starttime;
unsigned long sampletime_ms = 30000;
bool need_reset = false;
bool need_mdns_setup = false;

const String red = "#f00";
const String green = "#0f0";

// TODO: needs to be finished
void debug_out(const String& text, const int level, const bool linebreak)
{
	if (debug_enabled)
  {
		if (linebreak)
			Serial.println(text);
		else
			Serial.print(text);
	}
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

void captialize(String& s)
{
  int a;
  int b = -1;
  do{
      a = b + 1;
      b = s.indexOf(0x20, a);
      String tmp = s.substring(a, b - a);
      tmp[0] = toupper(tmp[0]);
      s.replace(s.substring(a, b - a), tmp);
  }
  while(b != -1);
}

void sensorDHT()
{
  TempAndHumidity th = dht.getTempAndHumidity();

  // Check if valid number if non NaN (not a number) will be send.
  if (isnan(th.temperature) || isnan(th.humidity))
  {
    Serial.println(F("DHT22 couldn’t be read"));
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
  String node = cfg::node;
  captialize(node);

  if (dht.isTooDry(dht_val.temperature, dht_val.humidity) || dht.isTooHumid(dht_val.temperature, dht_val.humidity))
    colour_h = red;
  else
    colour_h = green;

  if (dht.isTooCold(dht_val.temperature, dht_val.humidity) || dht.isTooHot(dht_val.temperature, dht_val.humidity))
    colour_te = red;
  else
    colour_te = green;

  page_content += String(html_root_header_w_style);
  page_content += ".temp_ind {background-color: " + colour_te + "}";
  page_content += ".hum_ind {background-color: " + colour_h + "}";
  page_content += String(html_root_header_end);

  // Web Page Heading
  page_content += "<body><h1>Measure Station " + node +  "</h1>";
  page_content += "<p>Temperature: " + Float2String(dht_val.temperature) + " °C <span class=\"dot temp_ind\"></span> </p>";
  page_content += "<p>Humidity: " + Float2String(dht_val.humidity) + " % <span class=\"dot hum_ind\"></span> </p>";
  page_content += "<p>Dew point: " + Float2String(dew_point) + " °C </p>";
  page_content += String(html_root_body_config_button);
  page_content += String(html_root_body_end_tag);

  server.send(200, "text/html; charset=utf-8", page_content);
}

void send_data_to_influxdb()
{
  String device = "esp8266-";
  device += String(ESP.getChipId());

  InfluxData data("indoor_temperature");
  // TODO: fix hack to replace one space
  String node = cfg::node;
  node.setCharAt(node.indexOf(0x20), 0x5f);
  data.addTag("node", node);
  data.addTag("device", device);
  data.addTag("sensor", "dht22");
  data.addValue("temperature", dht_val.temperature);
  data.addValue("humidity", dht_val.humidity);
  data.addValue("dew_point", dew_point);
  influx->prepare(data);

  boolean success = influx->write();
}

void mqtt_callback(char* topic, byte* payload, unsigned int length)
{
  char msg[length+1];

  for (unsigned int i = 0; i < length; i++)
    msg[i] = static_cast<char>(payload[i]);

  msg[length] = '\0';

  if (strcmp(msg, "temperature") == 0)
  {
    mqtt_msg = Float2String(dht_val.temperature);
    mqtt_client.publish(pub_topic_temp.c_str(), mqtt_msg.c_str(), true);
  }
  else if (strcmp(msg, "humidity") == 0)
  {
    mqtt_msg = Float2String(dht_val.humidity);
    mqtt_client.publish(pub_topic_humid.c_str(), mqtt_msg.c_str(), true);
  }
  else
  {
    Serial.print("Request not valid. Got ");
    Serial.println(msg);
  }
}

void reconnect()
{
  Serial.println("Reconnecting MQTT...");
  if (!mqtt_client.connect(cfg::hostname))
  {
    Serial.print("failed, rc=");
    Serial.print(mqtt_client.state());
    return;
  }

  mqtt_client.subscribe(sub_topic.c_str());
  Serial.println("MQTT connected.");
}

void config_saved()
{
  Serial.println("Configuration was updated.");
  need_reset = true;
}

void wifi_connected()
{
  need_mdns_setup = true;
}

void setup()
{
  Serial.begin(115200);
  delay(10);

  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  settingsGroup.addItem(&hostname_param);
  settingsGroup.addItem(&influxdb_host_param);
  settingsGroup.addItem(&influxdb_database_param);
  settingsGroup.addItem(&mqtt_server_param);
  settingsGroup.addItem(&disable_status_led_param);
  iotWebConf.addParameterGroup(&settingsGroup);
  iotWebConf.setConfigSavedCallback(&config_saved);
  iotWebConf.setupUpdateServer(
    [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
    [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });
  iotWebConf.setWifiConnectionCallback(&wifi_connected);

  cfg::hostname = iotWebConf.getThingName();
  bool valid_config = iotWebConf.init();
  
  if (!valid_config)
  {
    cfg::node[0] = '\0';
    cfg::influxdb_host[0] = '\0';
    cfg::influxdb_database[0] = '\0';
    cfg::mqtt_server[0] = '\0';
    cfg::disable_status_led[0] = '0';
  }
  else
  {
    Serial.println(String(cfg::node));
    Serial.println(String(cfg::influxdb_host));
    Serial.println(String(cfg::mqtt_server));
    influx = new Influxdb(cfg::influxdb_host);
    influx->setDb(cfg::influxdb_database);

    mqtt_client.setServer(cfg::mqtt_server, 1883);
    mqtt_client.setCallback(mqtt_callback);
    // TODO: fix hack to replace one space
    String node = cfg::node;
    node.setCharAt(node.indexOf(0x20), 0x5f);
    pub_topic_temp = "/home/measure/" + node + "/temperature";
    pub_topic_humid = "/home/measure/" + node + "/humidity";
    sub_topic = "/home/measure/" + node + "/get";

    if (atoi(cfg::disable_status_led) == 1)
      iotWebConf.disableBlink();
  }

  starttime = millis(); // store the start time
  dht.setup(13, DHTesp::DHT22);
  Serial.print("\n");
  Serial.println("ChipId: ");
  Serial.println(ESP.getChipId());

  if (!MDNS.begin(cfg::hostname))
    Serial.println("Error setting up mDNS");
  else
    Serial.println("mDNS started");

  server.on("/", webserver_root);
  server.on("/config", []{iotWebConf.handleConfig();});
  server.onNotFound([](){iotWebConf.handleNotFound();});
  server.begin();
}

void loop()
{
  if (need_mdns_setup)
  {
    if (!MDNS.begin(cfg::hostname))
      Serial.println("Error setting up mDNS");
    else
      Serial.println("mDNS started");
    
    need_mdns_setup = false;
  }
  iotWebConf.doLoop();

  // Checking if it is time to sample
  if ((millis() - starttime) > sampletime_ms)
  {
    starttime = millis(); // store the start time
    sensorDHT();          // getting temperature and humidity
    Serial.println("------------------------------");
    if (iotWebConf.getState() == iotwebconf::OnLine)
      send_data_to_influxdb();
  }

  if (cfg::mqtt_server[0] != '\0')
  {
    if ((iotWebConf.getState() == iotwebconf::OnLine) && (!mqtt_client.connected()))
      reconnect();
    mqtt_client.loop();
  }

  if (need_reset)
  {
    Serial.println("Rebooting after 1 second.");
    iotWebConf.delay(1000);
    ESP.restart();
  }

  // handle client connections
  server.handleClient();
}
