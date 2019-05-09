
//#define ESP32

//#define OTA	//Unmask this line to enable OTA
//const char* ota_password = "<YOUR OTA PASSWORD GOES HERE>";	

// wifi settings
const char* ssid     = "<YOUR WIFI SSID GOES HERE>";
const char* password = "<YOUR WIFI PASSWORD GOES HERE>";

// mqtt server settings
const char* mqtt_server   = "<YOUR MQTT BROKER IP/HOSTNAME GOES HERE>";
const int mqtt_port       = 1883;
const char* mqtt_username = "<YOUR MQTT USERNAME GOES HERE>";
const char* mqtt_password = "<YOUR MQTT PASSWORD GOES HERE>";

// mqtt client settings
// Note PubSubClient.h has a MQTT_MAX_PACKET_SIZE of 128 defined, so either raise it to 256 or use short topics
const char* client_id                   = "heatpump"; // Must be unique on the MQTT network
const char* heatpump_topic              = "heatpump";
const char* heatpump_set_topic          = "heatpump/set";
const char* heatpump_status_topic       = "heatpump/status";
const char* heatpump_timers_topic       = "heatpump/timers";

const char* heatpump_debug_topic        = "heatpump/debug";
const char* heatpump_debug_set_topic    = "heatpump/debug/set";

// pinouts
const int redLedPin  = 0; // Onboard LED = digital pin 0 (red LED on adafruit ESP8266 huzzah)
const int blueLedPin = 2; // Onboard LED = digital pin 0 (blue LED on adafruit ESP8266 huzzah)

// sketch settings
const unsigned int SEND_ROOM_TEMP_INTERVAL_MS = 60000;

// Home Assistant MQTT Discovery
// Set PubSubClient.h MQTT_MAX_PACKET_SIZE to 1280
//#define HA
//const char* heatpump_set_mode_topic     = "heatpump/set/mode";
//const char* heatpump_set_temp_topic     = "heatpump/set/temp";
//const char* heatpump_set_fan_topic      = "heatpump/set/fan";
//const char* heatpump_set_vane_topic     = "heatpump/set/vane";
//const char* ha_entity_id                = "Living ROom Air Conditioner"; // Device Name displayed in Home Assistant
//const char* min_temp                    = "16"; // Minimum temperature, check value from heatpump remote control
//const char* max_temp                    = "31"; // Maximum temperature, check value from heatpump remote control
//const char* temp_step                   = "1"; // Temperature setting step, check value from heatpump remote control
//const char* mqtt_discov_prefix          = "homeassistant"; // Home Assistant MQTT Discovery Prefix
//const char* controller_sw_version       = "20190509-2310"; // Software Version displayed in Home Assistant

// For Heat Pump library issue please refer to https://github.com/SwiCago/HeatPump .
// For Home Assistant issue please refer to https://github.com/unixko/HeatPump .
