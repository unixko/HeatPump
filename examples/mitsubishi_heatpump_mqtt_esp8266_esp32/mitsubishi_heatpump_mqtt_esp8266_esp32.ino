
#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <HeatPump.h>

#include "mitsubishi_heatpump_mqtt_esp8266_esp32.h"

#ifdef OTA
  #ifdef ESP32
    #include <WiFiUdp.h>
    #include <ESPmDNS.h>
  #else
    #include <ESP8266mDNS.h>
  #endif
  #include <ArduinoOTA.h>
#endif

// wifi, mqtt and heatpump client instances
WiFiClient espClient;
PubSubClient mqtt_client(espClient);
HeatPump hp;
unsigned long lastTempSend;

// debug mode, when true, will send all packets received from the heatpump to topic heatpump_debug_topic
// this can also be set by sending "on" to heatpump_debug_set_topic
bool _debugMode = false;


void setup() {
  pinMode(redLedPin, OUTPUT);
  digitalWrite(redLedPin, HIGH);
  pinMode(blueLedPin, OUTPUT);
  digitalWrite(blueLedPin, HIGH);

  WiFi.hostname(client_id);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    // wait 500ms, flashing the blue LED to indicate WiFi connecting...
    digitalWrite(blueLedPin, LOW);
    delay(250);
    digitalWrite(blueLedPin, HIGH);
    delay(250);
  }

  // startup mqtt connection
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
  mqttConnect();
  // connect to the heatpump. Callbacks first so that the hpPacketDebug callback is available for connect()
  hp.setSettingsChangedCallback(hpSettingsChanged);
  hp.setStatusChangedCallback(hpStatusChanged);
  hp.setPacketCallback(hpPacketDebug);

  #ifdef OTA
  ArduinoOTA.setHostname(client_id);
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.begin();
  #endif

  hp.connect(&Serial);

  #ifdef HA
  mqttAutoDiscovery();
  #endif
  
  lastTempSend = millis();
}

void hpSettingsChanged() {
  const size_t bufferSize = JSON_OBJECT_SIZE(6);
  DynamicJsonDocument root(bufferSize);

  heatpumpSettings currentSettings = hp.getSettings();

  root["power"]       = currentSettings.power;
  root["mode"]        = currentSettings.mode;
  root["temperature"] = currentSettings.temperature;
  root["fan"]         = currentSettings.fan;
  root["vane"]        = currentSettings.vane;
  root["wideVane"]    = currentSettings.wideVane;
  //root["iSee"]        = currentSettings.iSee;

  char buffer[512];
  serializeJson(root, buffer);

  bool retain = true;
  if (!mqtt_client.publish(heatpump_topic, buffer, retain)) {
    mqtt_client.publish(heatpump_debug_topic, "failed to publish to heatpump topic");
  }
}

void hpStatusChanged(heatpumpStatus currentStatus) {
  // send room temp and operating info
  const size_t bufferSizeInfo = JSON_OBJECT_SIZE(2);
  DynamicJsonDocument rootInfo(bufferSizeInfo);

  rootInfo["roomTemperature"] = currentStatus.roomTemperature;
  rootInfo["operating"]       = currentStatus.operating;

  char bufferInfo[512];
  serializeJson(rootInfo, bufferInfo);

  if (!mqtt_client.publish(heatpump_status_topic, bufferInfo, true)) {
    mqtt_client.publish(heatpump_debug_topic, "failed to publish to room temp and operation status to heatpump/status topic");
  }

  // send the timer info
  const size_t bufferSizeTimers = JSON_OBJECT_SIZE(5);
  DynamicJsonDocument rootTimers(bufferSizeTimers);

  rootTimers["mode"]          = currentStatus.timers.mode;
  rootTimers["onMins"]        = currentStatus.timers.onMinutesSet;
  rootTimers["onRemainMins"]  = currentStatus.timers.onMinutesRemaining;
  rootTimers["offMins"]       = currentStatus.timers.offMinutesSet;
  rootTimers["offRemainMins"] = currentStatus.timers.offMinutesRemaining;

  char bufferTimers[512];
  serializeJson(rootTimers, bufferTimers);

  if (!mqtt_client.publish(heatpump_timers_topic, bufferTimers, true)) {
    mqtt_client.publish(heatpump_debug_topic, "failed to publish timer info to heatpump/status topic");
  }
}

void hpPacketDebug(byte* packet, unsigned int length, char* packetDirection) {
  if (_debugMode) {
    String message;
    for (int idx = 0; idx < length; idx++) {
      if (packet[idx] < 16) {
        message += "0"; // pad single hex digits with a 0
      }
      message += String(packet[idx], HEX) + " ";
    }

    const size_t bufferSize = JSON_OBJECT_SIZE(8);
    DynamicJsonDocument root(bufferSize);

    root[packetDirection] = message;

    char buffer[512];
    serializeJson(root, buffer);

    if (!mqtt_client.publish(heatpump_debug_topic, buffer)) {
      mqtt_client.publish(heatpump_debug_topic, "failed to publish to heatpump/debug topic");
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Copy payload into message buffer
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';

  if (strcmp(topic, heatpump_set_topic) == 0) { //if the incoming message is on the heatpump_set_topic topic...
    // Parse message into JSON
    const size_t bufferSize = JSON_OBJECT_SIZE(6);
    DynamicJsonDocument root(bufferSize);
    DeserializationError error = deserializeJson(root, message);

    if (error) {
      mqtt_client.publish(heatpump_debug_topic, "!root.success(): invalid JSON on heatpump_set_topic...");
      return;
    }

    // Retrieve the values
    if (root.containsKey("power")) {
      const char* power = root["power"];
      hp.setPowerSetting(power);
    }

    if (root.containsKey("mode")) {
      const char* mode = root["mode"];
      hp.setModeSetting(mode);
    }

    if (root.containsKey("temperature")) {
      float temperature = root["temperature"];
      hp.setTemperature(temperature);
    }

    if (root.containsKey("fan")) {
      const char* fan = root["fan"];
      hp.setFanSpeed(fan);
    }

    if (root.containsKey("vane")) {
      const char* vane = root["vane"];
      hp.setVaneSetting(vane);
    }

    if (root.containsKey("wideVane")) {
      const char* wideVane = root["wideVane"];
      hp.setWideVaneSetting(wideVane);
    }

    if (root.containsKey("remoteTemp")) {
      float remoteTemp = root["remoteTemp"];
      hp.setRemoteTemperature(remoteTemp);
    }
    else if (root.containsKey("custom")) {
      String custom = root["custom"];

      // copy custom packet to char array
      char buffer[(custom.length() + 1)]; // +1 for the NULL at the end
      custom.toCharArray(buffer, (custom.length() + 1));

      byte bytes[20]; // max custom packet bytes is 20
      int byteCount = 0;
      char *nextByte;

      // loop over the byte string, breaking it up by spaces (or at the end of the line - \n)
      nextByte = strtok(buffer, " ");
      while (nextByte != NULL && byteCount < 20) {
        bytes[byteCount] = strtol(nextByte, NULL, 16); // convert from hex string
        nextByte = strtok(NULL, "   ");
        byteCount++;
      }

      // dump the packet so we can see what it is. handy because you can run the code without connecting the ESP to the heatpump, and test sending custom packets
      hpPacketDebug(bytes, byteCount, "customPacket");

      hp.sendCustomPacket(bytes, byteCount);
    }
    else {
      bool result = hp.update();

      if (!result) {
        mqtt_client.publish(heatpump_debug_topic, "heatpump: update() failed");
      }
    }
  #ifdef HA
  } else if (strcmp(topic, heatpump_set_mode_topic) == 0) { //if the incoming message is on the heatpump_set_mode_topic topic...
      if (strcmp(message, "off") == 0) {
        const char* power = "OFF";
        hp.setPowerSetting(power);
      } else if (strcmp(message, "fan_only") == 0) {
        const char* power = "ON";
        hp.setPowerSetting(power);      
        const char* mode = "FAN";
        hp.setModeSetting(mode);
      } else {
        const char* power = "ON";
        hp.setPowerSetting(power);
        const char* mode = strupr(message);
        hp.setModeSetting(mode);
      }
      hp.update();
  } else if (strcmp(topic, heatpump_set_temp_topic) == 0) { //if the incoming message is on the heatpump_set_temp_topic topic...
      float temperature = atof(message);
      hp.setTemperature(temperature);
      hp.update();
  } else if (strcmp(topic, heatpump_set_fan_topic) == 0) { //if the incoming message is on the heatpump_set_fan_topic topic...
      const char* fan = strupr(message);      
      hp.setFanSpeed(fan);
      hp.update();
  } else if (strcmp(topic, heatpump_set_vane_topic) == 0) { //if the incoming message is on the heatpump_set_vane_topic topic...
      const char* vane = strupr(message);
      hp.setVaneSetting(vane);
      hp.update();
  #endif
  } else if (strcmp(topic, heatpump_debug_set_topic) == 0) { //if the incoming message is on the heatpump_debug_set_topic topic...
      if (strcmp(message, "on") == 0) {
      _debugMode = true;
      mqtt_client.publish(heatpump_debug_topic, "debug mode enabled");
    } else if (strcmp(message, "off") == 0) {
      _debugMode = false;
      mqtt_client.publish(heatpump_debug_topic, "debug mode disabled");
    }
  } else {
    mqtt_client.publish(heatpump_debug_topic, strcat("heatpump: wrong mqtt topic: ", topic));
  }
}

void mqttConnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    // Attempt to connect
    if (mqtt_client.connect(client_id, mqtt_username, mqtt_password)) {
      mqtt_client.subscribe(heatpump_set_topic);
      mqtt_client.subscribe(heatpump_debug_set_topic);
      #ifdef HA
      mqtt_client.subscribe(heatpump_set_mode_topic);
      mqtt_client.subscribe(heatpump_set_temp_topic);
      mqtt_client.subscribe(heatpump_set_fan_topic);
      mqtt_client.subscribe(heatpump_set_vane_topic);
      #endif
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

#ifdef HA
void mqttAutoDiscovery() {
  const String chip_id                 = String(ESP.getChipId());
  const String mqtt_discov_topic       = String(mqtt_discov_prefix) + "/climate/" + chip_id + "/config";
  
  const size_t bufferSizeDiscovery = JSON_OBJECT_SIZE(50);
  DynamicJsonDocument rootDiscovery(bufferSizeDiscovery);

  rootDiscovery["name"]                = ha_entity_id;
  rootDiscovery["uniq_id"]             = chip_id;
  rootDiscovery["~"]                   = heatpump_topic;
  rootDiscovery["mode_cmd_t"]          = heatpump_set_mode_topic;
  rootDiscovery["temp_cmd_t"]          = heatpump_set_temp_topic;
  rootDiscovery["fan_mode_cmd_t"]      = heatpump_set_fan_topic;
  rootDiscovery["swing_mode_cmd_t"]    = heatpump_set_vane_topic;
  rootDiscovery["min_temp"]            = min_temp;
  rootDiscovery["max_temp"]            = max_temp;
  rootDiscovery["temp_step"]           = temp_step;
  JsonArray modes                 = rootDiscovery.createNestedArray("modes");
    modes.add("off");
    modes.add("auto");
    modes.add("cool");
    modes.add("dry");
    modes.add("heat");
    modes.add("fan_only");
  JsonArray fan_modes             = rootDiscovery.createNestedArray("fan_modes");
    fan_modes.add("auto");
    fan_modes.add("quiet");
    fan_modes.add("1");
    fan_modes.add("2");
    fan_modes.add("3");
    fan_modes.add("4");
  JsonArray swing_modes           = rootDiscovery.createNestedArray("swing_modes");
    swing_modes.add("auto");
    swing_modes.add("1");
    swing_modes.add("2");
    swing_modes.add("3");
    swing_modes.add("4");
    swing_modes.add("5");
    swing_modes.add("swing");
  rootDiscovery["curr_temp_t"]         = "~/status";
  rootDiscovery["current_temperature_template"] = "{{ value_json.roomTemperature }}";
  rootDiscovery["mode_stat_t"]         = "~";
  rootDiscovery["mode_stat_tpl"]       = "{{ 'off' if value_json.power == 'OFF' else value_json.mode | lower | replace('fan', 'fan_only')}}";
  rootDiscovery["temp_stat_t"]         = "~";
  rootDiscovery["temp_stat_tpl"]       = "{{ value_json.temperature }}";
  rootDiscovery["fan_mode_stat_t"]     = "~";
  rootDiscovery["fan_mode_stat_tpl"]   = "{{ value_json.fan | lower }}";
  rootDiscovery["swing_mode_stat_t"]   = "~";
  rootDiscovery["swing_mode_stat_tpl"] = "{{ value_json.vane | lower }}";
  JsonObject device                    = rootDiscovery.createNestedObject("device");
    device["name"]                     = ha_entity_id;
    JsonArray ids = device.createNestedArray("ids");
      ids.add(chip_id);
    device["mf"]                       = "UNIXKO";
    device["mdl"]                      = "Mitsubishi Electric Heat Pump";
    device["sw"]                       = controller_sw_version;

  char bufferDiscovery[1280];
  serializeJson(rootDiscovery, bufferDiscovery);

  bool retain = true;
  if (!mqtt_client.publish(mqtt_discov_topic.c_str(), bufferDiscovery, retain)) {
    mqtt_client.publish(heatpump_debug_topic, "failed to publish to HA Discovery topic");
  }
}
#endif

void loop() {
  if (!mqtt_client.connected()) {
    mqttConnect();
  }

  hp.sync();

  if (millis() > (lastTempSend + SEND_ROOM_TEMP_INTERVAL_MS)) { // only send the temperature every 60s
    hpStatusChanged(hp.getStatus());
    lastTempSend = millis();
  }

  mqtt_client.loop();

#ifdef OTA
  ArduinoOTA.handle();
#endif
}
