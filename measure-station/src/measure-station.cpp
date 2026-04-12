#include <DHTesp.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <InfluxDb.h>
#include <IotWebConf.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BME280.h>
#ifdef ESP8266
#include <ESP8266HTTPUpdateServer.h>
#elif defined(ESP32)
// For ESP32 IotWebConf provides a drop-in replacement for UpdateServer.
#include <IotWebConfESP32HTTPUpdateServer.h>
#endif

#define CONFIG_VERSION "measure-station-v2.3"
#define STATUS_PIN LED_BUILTIN
#define CONFIG_PIN D2
#define STR_LEN 128
#define NUMBER_LEN 32

const char html_root_open_tag[] PROGMEM = "<!DOCTYPE html><html>";
const char html_root_header_w_style[] PROGMEM =
    "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<link rel=\"icon\" href=\"data:,\">"
    "<style>"
    "body { background: #f6f8fa; margin: 0; padding: 0; }"
    ".container { max-width: 400px; margin: 40px auto; background: #fff; border-radius: 12px; box-shadow: 0 2px 8px rgba(0,0,0,0.08); padding: 24px 20px 20px 20px; }"
    "h1 { font-size: 1.5em; margin-bottom: 0.5em; color: #222; }"
    ".sensor-row { display: flex; align-items: center; justify-content: space-between; margin: 18px 0; }"
    ".sensor-label { font-size: 1.1em; color: #555; }"
    ".sensor-value { font-size: 2em; font-weight: bold; color: #222; }"
    ".dot {height: 20px; width: 20px; border-radius: 50%; display: inline-block; margin-left: 10px; border: 1px solid #ccc;}"
    ".temp_ind {background-color: ";
  const char html_root_header_dynamic_style[] PROGMEM = "%s;} .hum_ind {background-color: %s;}";
  const char html_root_header_end[] PROGMEM = "</style></head>";
  const char html_root_body_start[] PROGMEM = "<body><div class=\"container\"><h1>Measure Station %s</h1>";
  const char html_sensor_row[] PROGMEM = "<div class=\"sensor-row\"><span class=\"sensor-label\">%s</span><span class=\"sensor-value\">%s %s</span></div>";
  const char html_sensor_row_with_indicator[] PROGMEM = "<div class=\"sensor-row\"><span class=\"sensor-label\">%s</span><span class=\"sensor-value\">%s %s <span class=\"dot %s\"></span></span></div>";
  const char html_root_body_config_button[] PROGMEM = "<a href=\"config\"><button type=\"button\">Config</button></a>";
  const char html_root_body_container_end[] PROGMEM = "</div>";
  const char html_root_body_end_tag[] PROGMEM = "</body></html>";

const char intial_host_name[] = "measure-station-test";
const char initial_pw[] = "test1234";
const bool debug_enabled = false;

namespace cfg
{
  char *hostname = NULL;
  char node[STR_LEN] = "";
  char influxdb_host[STR_LEN] = "";
  char influxdb_database[STR_LEN] = "";
  char mqtt_server[STR_LEN] = "";
  char mqtt_user[STR_LEN] = "";
  char mqtt_password[STR_LEN] = "";
  char mqtt_temperature_topic[STR_LEN] = "";
  char mqtt_humidity_topic[STR_LEN] = "";
  char disable_status_led[NUMBER_LEN] = "0";
  char mqtt_dew_point_topic[STR_LEN] = "";
  char bme280_enabled[NUMBER_LEN] = "0";
  char temp_offset[NUMBER_LEN] = "0.0";
}

static char ledStatusValues[][NUMBER_LEN] = { "0", "1" };
static char ledStatusNames[][STR_LEN] = { "Enabled", "Disabled" };
static char bme280EnabledValues[][NUMBER_LEN] = { "0", "1" };
static char bme280EnabledNames[][STR_LEN] = { "Disabled", "Enabled" };

DHTesp dht;
TempAndHumidity dht_val;
float dew_point;

DNSServer dns_server;
ESP8266WebServer server(80);
WiFiClient wifi_client;
#ifdef ESP8266
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
HTTPUpdateServer httpUpdater;
#endif
Influxdb* influx = nullptr;
Adafruit_BME280 bme;
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_humid = bme.getHumiditySensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
sensors_event_t bme_temp_event, bme_humid_event, bme_pressure_event;
PubSubClient mqtt_client(wifi_client);
IotWebConf iotWebConf(intial_host_name, &dns_server, &server, initial_pw, CONFIG_VERSION);
iotwebconf::ParameterGroup influxdb_group = iotwebconf::ParameterGroup("InfluxDB", "InfluxDB Settings");
iotwebconf::TextParameter influxdb_host_param = iotwebconf::TextParameter("InflufDB Host", "influxdb_host", cfg::influxdb_host, STR_LEN);
iotwebconf::TextParameter influxdb_database_param = iotwebconf::TextParameter("InfluxDB Database", "influxdb_database", cfg::influxdb_database, STR_LEN);
iotwebconf::ParameterGroup mqtt_group = iotwebconf::ParameterGroup("MQTT", "MQTT Settings");
iotwebconf::TextParameter mqtt_server_param = iotwebconf::TextParameter("MQTT Server", "mqtt_server", cfg::mqtt_server, STR_LEN);
iotwebconf::TextParameter mqtt_user_param = iotwebconf::TextParameter("MQTT User", "mqtt_user", cfg::mqtt_user, STR_LEN);
iotwebconf::PasswordParameter mqtt_password_param = iotwebconf::PasswordParameter("MQTT Password", "mqtt_password", cfg::mqtt_password, STR_LEN);
iotwebconf::TextParameter mqtt_temperature_topic_param = iotwebconf::TextParameter("MQTT Temperature Topic", "mqtt_temperature_topic", cfg::mqtt_temperature_topic, STR_LEN);
iotwebconf::TextParameter mqtt_humidity_topic_param = iotwebconf::TextParameter("MQTT Humidity Topic", "mqtt_humidity_topic", cfg::mqtt_humidity_topic, STR_LEN);
iotwebconf::TextParameter mqtt_dew_point_topic_param = iotwebconf::TextParameter("MQTT Dew Point Topic", "mqtt_dew_point_topic", cfg::mqtt_dew_point_topic, STR_LEN);
iotwebconf::TextParameter hostname_param = iotwebconf::TextParameter("Node (e.g. the room)", "node", cfg::node, STR_LEN);
//iotwebconf::NumberParameter disable_status_led_param = iotwebconf::NumberParameter("Disable status LED", "disable_status_led", cfg::disable_status_led, NUMBER_LEN, "number", "0..1", "min='0' max='1' step='1'");
iotwebconf::SelectParameter disable_status_led_param = iotwebconf::SelectParameter("Status LED", "disable_status_led", cfg::disable_status_led, NUMBER_LEN, (char*)ledStatusValues, (char*)ledStatusNames, sizeof(ledStatusValues) / NUMBER_LEN, STR_LEN);
iotwebconf::SelectParameter bme280_enabled_param = iotwebconf::SelectParameter("BME280 Sensor", "bme280_enabled", cfg::bme280_enabled, NUMBER_LEN, (char*)bme280EnabledValues, (char*)bme280EnabledNames, sizeof(bme280EnabledValues) / NUMBER_LEN, STR_LEN);
iotwebconf::NumberParameter temp_offset_param = iotwebconf::NumberParameter("Temperature Offset", "temp_offset", cfg::temp_offset, NUMBER_LEN, "number", "-5..5", "min='-5' max='5' step='0.1'");


String pub_topic_temp, pub_topic_humid, pub_topic_dew_point, sub_topic;
String mqtt_msg;

unsigned long starttime;
unsigned long sampletime_ms = 30000;
bool need_reset = false;
bool need_mdns_setup = false;

const char red[] = "#f00";
const char green[] = "#0f0";

// TODO: needs to be finished
void debug_out(const String &text, const int level, const bool linebreak)
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

void formatFloat(char *buffer, size_t bufferSize, float value)
{
  dtostrf(value, 0, 2, buffer);
  buffer[bufferSize - 1] = '\0';
}

void formatDisplayName(char *destination, size_t destinationSize, const char *source)
{
  if (destinationSize == 0)
  {
    return;
  }

  strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';

  bool capitalizeNext = true;
  for (size_t index = 0; destination[index] != '\0'; ++index)
  {
    unsigned char current = static_cast<unsigned char>(destination[index]);
    if (capitalizeNext && isalpha(current))
    {
      destination[index] = static_cast<char>(toupper(current));
      capitalizeNext = false;
    }
    else
    {
      capitalizeNext = destination[index] == ' ';
    }
  }
}

void sendSensorRow(const char *label, float value, const char *unit, const char *indicatorClass = nullptr)
{
  char valueBuffer[16];
  char rowBuffer[256];

  formatFloat(valueBuffer, sizeof(valueBuffer), value);

  if (indicatorClass == nullptr)
  {
    snprintf_P(rowBuffer, sizeof(rowBuffer), html_sensor_row, label, valueBuffer, unit);
  }
  else
  {
    snprintf_P(rowBuffer, sizeof(rowBuffer), html_sensor_row_with_indicator, label, valueBuffer, unit, indicatorClass);
  }

  server.sendContent(rowBuffer);
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
    dht_val.temperature += atof(cfg::temp_offset);
    dew_point = dht.computeDewPoint(dht_val.temperature, dht_val.humidity);
    Serial.print("Humidity    : ");
    Serial.print(dht_val.humidity);
    Serial.print(" %\n");
    Serial.print("Temperature : ");
    Serial.print(dht_val.temperature);
    Serial.println(" C");
  }
}

void sensorBME280()
{
  bme_temp->getEvent(&bme_temp_event);
  bme_humid->getEvent(&bme_humid_event);
  bme_pressure->getEvent(&bme_pressure_event);

  if (isnan(bme_temp_event.temperature) || isnan(bme_humid_event.relative_humidity) || isnan(bme_pressure_event.pressure))
  {
    Serial.println(F("BME280 couldn't be read"));
  }
  else
  {
    bme_temp_event.temperature += atof(cfg::temp_offset);
    dew_point = dht.computeDewPoint(bme_temp_event.temperature, bme_humid_event.relative_humidity);
    Serial.print("BME280 Temperature : ");
    Serial.print(bme_temp_event.temperature);
    Serial.println(" C");
    Serial.print("BME280 Humidity : ");
    Serial.print(bme_humid_event.relative_humidity);
    Serial.println(" %");
    Serial.print("BME280 Pressure : ");
    Serial.print(bme_pressure_event.pressure);
    Serial.println(" hPa");
  }
}

void webserver_root()
{
  char displayNode[STR_LEN];
  char headerBuffer[STR_LEN + 64];
  char styleBuffer[64];
  const bool useBme280 = atoi(cfg::bme280_enabled) == 1;
  const char *colour_h;
  const char *colour_te;

  formatDisplayName(displayNode, sizeof(displayNode), cfg::node);

  if (dht.isTooDry(dht_val.temperature, dht_val.humidity) || dht.isTooHumid(dht_val.temperature, dht_val.humidity))
    colour_h = red;
  else
    colour_h = green;

  if (dht.isTooCold(dht_val.temperature, dht_val.humidity) || dht.isTooHot(dht_val.temperature, dht_val.humidity))
    colour_te = red;
  else
    colour_te = green;

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");
  server.sendContent_P(html_root_open_tag);
  server.sendContent_P(html_root_header_w_style);
  snprintf_P(styleBuffer, sizeof(styleBuffer), html_root_header_dynamic_style, colour_te, colour_h);
  server.sendContent(styleBuffer);
  server.sendContent_P(html_root_header_end);
  snprintf_P(headerBuffer, sizeof(headerBuffer), html_root_body_start, displayNode);
  server.sendContent(headerBuffer);

  if (useBme280)
  {
    sendSensorRow("🌡️ Temperature", bme_temp_event.temperature, "°C");
    sendSensorRow("💧 Humidity", bme_humid_event.relative_humidity, "%");
    sendSensorRow("❄️ Dew point", dew_point, "°C");
    sendSensorRow("⏲️ Pressure", bme_pressure_event.pressure, "hPa");
  }
  else
  {
    sendSensorRow("🌡️ Temperature", dht_val.temperature, "°C", "temp_ind");
    sendSensorRow("💧 Humidity", dht_val.humidity, "%", "hum_ind");
    sendSensorRow("❄️ Dew point", dew_point, "°C");
  }
  server.sendContent_P(html_root_body_config_button);
  server.sendContent_P(html_root_body_container_end);
  server.sendContent_P(html_root_body_end_tag);
  server.sendContent("");
}

void send_data_to_influxdb()
{
  String device = "esp8266-";
  device += String(ESP.getChipId());
  const bool useBme280 = atoi(cfg::bme280_enabled) == 1;

  InfluxData data("indoor_temperature");
  String node = cfg::node;
  node.replace(" ", "_"); // Replace all spaces with underscores
  data.addTag("node", node);
  data.addTag("device", device);
  data.addTag("sensor",  useBme280 ? "bme280" : "dht22");
  data.addValue("temperature",  useBme280 ? bme_temp_event.temperature : dht_val.temperature);
  data.addValue("humidity", useBme280 ? bme_humid_event.relative_humidity : dht_val.humidity);
  data.addValue("dew_point", dew_point);
  influx->prepare(data);

  boolean success = influx->write();
  if (!success)
  {
    Serial.println("Failed to write to InfluxDB");
    Serial.println(influx->getLastErrorMessage());
  }
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  char msg[length + 1];

  for (unsigned int i = 0; i < length; i++)
    msg[i] = static_cast<char>(payload[i]);

  msg[length] = '\0';

  const bool useBme280 = atoi(cfg::bme280_enabled) == 1;

  if (strcmp(msg, "temperature") == 0)
  {
    mqtt_msg = useBme280 ? Float2String(bme_temp_event.temperature) : Float2String(dht_val.temperature);
    boolean success = mqtt_client.publish(pub_topic_temp.c_str(), mqtt_msg.c_str(), true);

    if (!success)
      Serial.println("publishing via mqtt failed.");
  }
  else if (strcmp(msg, "humidity") == 0)
  {
    mqtt_msg = useBme280 ? Float2String(bme_humid_event.relative_humidity) : Float2String(dht_val.humidity);
    boolean success = mqtt_client.publish(pub_topic_humid.c_str(), mqtt_msg.c_str(), true);

    if (!success)
      Serial.println("publishing via mqtt failed.");
  }
  else if (strcmp(msg, "dew_point") == 0)
  {
    mqtt_msg = Float2String(dew_point);
    boolean success = mqtt_client.publish(pub_topic_dew_point.c_str(), mqtt_msg.c_str(), true);

    if (!success)
      Serial.println("publishing via mqtt failed.");
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
  if (!mqtt_client.connect(cfg::hostname, cfg::mqtt_user, cfg::mqtt_password))
  {
    Serial.print("failed, rc=");
    Serial.println(mqtt_client.state());
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
  iotWebConf.addSystemParameter(&hostname_param);
  influxdb_group.addItem(&influxdb_host_param);
  influxdb_group.addItem(&influxdb_database_param);
  iotWebConf.addParameterGroup(&influxdb_group);
  mqtt_group.addItem(&mqtt_server_param);
  mqtt_group.addItem(&mqtt_user_param);
  mqtt_group.addItem(&mqtt_password_param);
  mqtt_group.addItem(&mqtt_temperature_topic_param);
  mqtt_group.addItem(&mqtt_humidity_topic_param);
  mqtt_group.addItem(&mqtt_dew_point_topic_param);
  iotWebConf.addParameterGroup(&mqtt_group);
  iotWebConf.addSystemParameter(&disable_status_led_param);
  iotWebConf.addSystemParameter(&bme280_enabled_param);
  iotWebConf.addSystemParameter(&temp_offset_param);
  iotWebConf.setConfigSavedCallback(&config_saved);
  iotWebConf.setupUpdateServer([](const char *updatePath)
                               { httpUpdater.setup(&server, updatePath); },
                               [](const char *userName, char *password)
                               { httpUpdater.updateCredentials(userName, password); });
  iotWebConf.setWifiConnectionCallback(&wifi_connected);

  cfg::hostname = iotWebConf.getThingName();
  bool valid_config = iotWebConf.init();

  if (!valid_config)
  {
    cfg::node[0] = '\0';
    cfg::influxdb_host[0] = '\0';
    cfg::influxdb_database[0] = '\0';
    cfg::mqtt_server[0] = '\0';
    cfg::mqtt_user[0] = '\0';
    cfg::mqtt_password[0] = '\0';
    cfg::mqtt_temperature_topic[0] = '\0';
    cfg::mqtt_humidity_topic[0] = '\0';
    cfg::disable_status_led[0] = '0';
    cfg::mqtt_dew_point_topic[0] = '\0';
    cfg::bme280_enabled[0] = '0';
    cfg::temp_offset[0] = '0';
  }
  else
  {
    Serial.println(String(cfg::node));
    Serial.println(String(cfg::influxdb_host));
    Serial.println(String(cfg::mqtt_server));
    if (influx) {
      delete influx;
    }
    influx = new Influxdb(cfg::influxdb_host, 8086);
    influx->setDb(cfg::influxdb_database);
    mqtt_client.setServer(cfg::mqtt_server, 1883);
    mqtt_client.setCallback(mqtt_callback);
    // TODO: fix hack to replace one space
    String node = cfg::node;
    node.setCharAt(node.indexOf(0x20), 0x5f);
    pub_topic_temp = cfg::mqtt_temperature_topic;
    pub_topic_humid = cfg::mqtt_humidity_topic;
    pub_topic_dew_point = cfg::mqtt_dew_point_topic;
    sub_topic = "/home/measure/" + node + "/get";

    if (atoi(cfg::disable_status_led) == 1)
      iotWebConf.disableBlink();
    
    uint8_t try_count = 0;
    if (cfg::bme280_enabled[0] != '\0' && atoi(cfg::bme280_enabled) == 1)
    {
      while (!bme.begin(0x76) || try_count < 5) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        try_count++;
        delay(2000);
      }
    }
  }

  starttime = millis(); // store the start time
  dht.setup(13, DHTesp::DHT22);
  Serial.print("\n");
  Serial.println("ChipId: ");
  Serial.println(ESP.getChipId());

  if (!MDNS.begin(cfg::hostname, WiFi.localIP()))
    Serial.println("Error setting up mDNS");
  else
    Serial.println("mDNS started");

  server.on("/", webserver_root);
  server.on("/config", []
            { iotWebConf.handleConfig(); });
  server.onNotFound([]()
                    { iotWebConf.handleNotFound(); });
  server.begin();
}

void loop()
{
  const bool useBme280 = atoi(cfg::bme280_enabled) == 1;
  if (need_mdns_setup)
  {
    if (!MDNS.begin(cfg::hostname, WiFi.localIP()))
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
    if (useBme280)
      sensorBME280();
    else
      sensorDHT();          // getting temperature and humidity
    Serial.println("------------------------------");
    if (iotWebConf.getState() == iotwebconf::OnLine && cfg::influxdb_host[0] != '\0')
      send_data_to_influxdb();

    if (cfg::mqtt_server[0] != '\0')
    {
      if (iotWebConf.getState() == iotwebconf::NetworkState::OnLine && !mqtt_client.connected())
      {
        reconnect();
      }

      mqtt_msg = useBme280 ? Float2String(bme_temp_event.temperature) : Float2String(dht_val.temperature);
      boolean success = mqtt_client.publish(pub_topic_temp.c_str(), mqtt_msg.c_str(), true);

      if (!success)
        Serial.println("publishing temperature via mqtt failed.");

      mqtt_msg = useBme280 ? Float2String(bme_humid_event.relative_humidity) : Float2String(dht_val.humidity);
      success = mqtt_client.publish(pub_topic_humid.c_str(), mqtt_msg.c_str(), true);

      if (!success)
        Serial.println("publishing humidity via mqtt failed.");

      mqtt_msg = Float2String(dew_point);
      success = mqtt_client.publish(pub_topic_dew_point.c_str(), mqtt_msg.c_str(), true);

      if (!success)
        Serial.println("publishing dew point via mqtt failed.");
    }
  }

  if (cfg::mqtt_server[0] != '\0')
  {
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
  delay(1); // Yield CPU to reduce unnecessary cycles
}
