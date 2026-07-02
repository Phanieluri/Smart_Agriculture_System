#include <WiFi.h>
#include <DHT.h>
#include <PubSubClient.h>

// ================= WiFi Details =================
const char* ssid = "ATCHYUTA";
const char* password = "Sri@tchyuta";

// ================= MQTT Details =================
const char* mqtt_server = "13.127.140.230";
const int mqtt_port = 1883;

// Device ID
const char* DEVICE_ID = "esp32_01";

// ESP32 publishes sensor data to this topic
const char* TELEMETRY_TOPIC = "esp32/sensor_data";

// ESP32 receives ML command from this topic
const char* COMMAND_TOPIC = "esp32/actuation_command";

// Old topic support
const char* LEGACY_COMMAND_TOPIC = "esp32/pump_command";

// ================= Sensor Pins =================
#define DHTPIN 23
#define DHTTYPE DHT11
#define SOIL_PIN 34

// ================= Actuator Pins =================
#define RED_LED_PIN 4
#define GREEN_LED_PIN 5
#define BLUE_LED_PIN 18
#define BUZZER_PIN 19

// ================= Buzzer Type =================
const bool BUZZER_ACTIVE_LOW = true;

// ================= Soil Sensor Calibration =================
const int SOIL_DRY_ADC = 4095;
const int SOIL_WET_ADC = 1250;

// ================= Timing =================
const unsigned long TELEMETRY_INTERVAL_MS = 2000;

// If cloud command stops for this time, stop all actuation
const unsigned long COMMAND_TIMEOUT_MS = 1000;

// MQTT reconnect attempt interval
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 2000;

// Level 1 beep timing
const unsigned long BEEP_ON_MS = 250;
const unsigned long BEEP_OFF_MS = 750;

// Buzzer tone frequency
const unsigned long BUZZER_HALF_PERIOD_US = 500;

// ================= Objects =================
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// ================= Global Variables =================
int currentLevel = -1;

unsigned long lastTelemetryMs = 0;
unsigned long lastCommandMs = 0;
unsigned long lastMqttReconnectAttemptMs = 0;

bool commandActive = false;
bool failsafeTriggered = false;

// Buzzer mode:
// 0 = OFF
// 1 = BEEP
// 2 = CONTINUOUS
int buzzerMode = 0;

bool beepWindowOn = false;
unsigned long lastBeepChangeMs = 0;

bool tonePinState = false;
unsigned long lastToneToggleUs = 0;

// ================= WiFi Connection =================
void setup_wifi() {
  delay(10);

  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    stopAllActuation();
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
}

// ================= Buzzer Raw Control =================
void buzzerRawWrite(bool active) {
  if (BUZZER_ACTIVE_LOW) {
    digitalWrite(BUZZER_PIN, active ? LOW : HIGH);
  } else {
    digitalWrite(BUZZER_PIN, active ? HIGH : LOW);
  }
}

// ================= Stop Buzzer =================
void stopBuzzer() {
  buzzerMode = 0;
  beepWindowOn = false;
  tonePinState = false;
  buzzerRawWrite(false);
}

// ================= Stop All Actuation =================
void stopAllActuation() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);

  stopBuzzer();

  currentLevel = -1;
}

// ================= Generate Tone =================
void generateBuzzerTone() {
  unsigned long nowUs = micros();

  if (nowUs - lastToneToggleUs >= BUZZER_HALF_PERIOD_US) {
    lastToneToggleUs = nowUs;

    tonePinState = !tonePinState;
    buzzerRawWrite(tonePinState);
  }
}

// ================= Update Buzzer =================
void updateBuzzer() {
  if (buzzerMode == 0) {
    buzzerRawWrite(false);
    return;
  }

  if (buzzerMode == 2) {
    generateBuzzerTone();
    return;
  }

  if (buzzerMode == 1) {
    unsigned long nowMs = millis();

    if (beepWindowOn) {
      generateBuzzerTone();

      if (nowMs - lastBeepChangeMs >= BEEP_ON_MS) {
        beepWindowOn = false;
        lastBeepChangeMs = nowMs;
        tonePinState = false;
        buzzerRawWrite(false);
      }
    } else {
      buzzerRawWrite(false);

      if (nowMs - lastBeepChangeMs >= BEEP_OFF_MS) {
        beepWindowOn = true;
        lastBeepChangeMs = nowMs;
        lastToneToggleUs = micros();
      }
    }
  }
}

// ================= Apply Level Command =================
void applyLevel(int level) {
  if (level < 0 || level > 2) {
    Serial.println("Invalid level received.");
    return;
  }

  currentLevel = level;

  if (level == 0) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(BLUE_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);

    stopBuzzer();

    Serial.println("LEVEL 0: Green LED ON, Buzzer OFF");
  }

  else if (level == 1) {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);

    buzzerMode = 1;
    beepWindowOn = false;
    lastBeepChangeMs = millis();
    tonePinState = false;
    buzzerRawWrite(false);

    Serial.println("LEVEL 1: Blue LED ON, Buzzer Beeping");
  }

  else if (level == 2) {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);

    buzzerMode = 2;
    tonePinState = false;
    lastToneToggleUs = micros();

    Serial.println("LEVEL 2: Red LED ON, Buzzer Continuous");
  }
}

// ================= Cloud Command Timeout Check =================
void checkCommandTimeout() {
  if (!commandActive) {
    return;
  }

  unsigned long now = millis();

  if (now - lastCommandMs > COMMAND_TIMEOUT_MS) {
    if (!failsafeTriggered) {
      Serial.println("FAILSAFE: No cloud command received. Stopping all actuation.");
      stopAllActuation();

      failsafeTriggered = true;
      commandActive = false;
    }
  }
}

// ================= Extract Level from Cloud Message =================
int extractNumberAfterKey(String message, String key) {
  int keyIndex = message.indexOf(key);

  if (keyIndex < 0) {
    return -1;
  }

  int colonIndex = message.indexOf(':', keyIndex);

  if (colonIndex < 0) {
    return -1;
  }

  for (int i = colonIndex + 1; i < message.length(); i++) {
    char c = message.charAt(i);

    if (isDigit(c)) {
      return c - '0';
    }
  }

  return -1;
}

int parseLevelFromCommand(String message) {
  message.trim();

  String msg = message;
  msg.toLowerCase();

  int level = extractNumberAfterKey(msg, "level");

  if (level >= 0 && level <= 2) {
    return level;
  }

  level = extractNumberAfterKey(msg, "prediction");

  if (level >= 0 && level <= 2) {
    return level;
  }

  if (msg == "0" || msg.indexOf("low") >= 0 || msg.indexOf("level_0") >= 0) {
    return 0;
  }

  if (msg == "1" || msg.indexOf("medium") >= 0 || msg.indexOf("level_1") >= 0) {
    return 1;
  }

  if (msg == "2" || msg.indexOf("high") >= 0 || msg.indexOf("level_2") >= 0) {
    return 2;
  }

  if (msg.indexOf("pump_off") >= 0) {
    return 0;
  }

  if (msg.indexOf("pump_on") >= 0) {
    return 2;
  }

  return -1;
}

// ================= MQTT Callback =================
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Command received on topic: ");
  Serial.println(topic);

  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Cloud command: ");
  Serial.println(message);

  int level = parseLevelFromCommand(message);

  if (level == -1) {
    Serial.println("Could not understand cloud command.");
    return;
  }

  // Fresh cloud command received
  lastCommandMs = millis();
  commandActive = true;
  failsafeTriggered = false;

  applyLevel(level);
}

// ================= MQTT Reconnect Once =================
void connect_mqtt_once() {
  if (client.connected()) {
    return;
  }

  unsigned long now = millis();

  if (now - lastMqttReconnectAttemptMs < MQTT_RECONNECT_INTERVAL_MS) {
    return;
  }

  lastMqttReconnectAttemptMs = now;

  Serial.print("Connecting to MQTT broker... ");

  String clientId = "ESP32_AGRI_";
  clientId += String(random(0xffff), HEX);

  if (client.connect(clientId.c_str())) {
    Serial.println("connected.");

    client.subscribe(COMMAND_TOPIC);
    client.subscribe(LEGACY_COMMAND_TOPIC);

    Serial.print("Subscribed to: ");
    Serial.println(COMMAND_TOPIC);
  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
}

// ================= Read Soil Moisture =================
int readMoisturePercent() {
  int rawMoisture = 0;

  for (int i = 0; i < 10; i++) {
    rawMoisture += analogRead(SOIL_PIN);
    delay(10);
  }

  rawMoisture = rawMoisture / 10;

  int moisturePercent = map(rawMoisture, SOIL_DRY_ADC, SOIL_WET_ADC, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  Serial.print("Raw Moisture ADC: ");
  Serial.print(rawMoisture);
  Serial.print(" | Moisture Percentage: ");
  Serial.println(moisturePercent);

  return moisturePercent;
}

// ================= Publish Sensor Data =================
void publishTelemetry() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT11 sensor.");
    return;
  }

  int moisture = readMoisturePercent();

  float light = 6.50;

  String payload = "{";
  payload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  payload += "\"temperature\":" + String(temperature, 2) + ",";
  payload += "\"humidity\":" + String(humidity, 2) + ",";
  payload += "\"moisture\":" + String(moisture) + ",";
  payload += "\"light\":" + String(light, 2);
  payload += "}";

  Serial.print("Publishing sensor data: ");
  Serial.println(payload);

  if (client.connected()) {
    client.publish(TELEMETRY_TOPIC, payload.c_str());
  } else {
    Serial.println("MQTT not connected. Telemetry not published.");
  }
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();

  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);
  pinMode(SOIL_PIN, INPUT);

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  stopAllActuation();

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(512);

  randomSeed(micros());

  Serial.println("ESP32 system ready.");
}

// ================= Main Loop =================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Stopping actuation.");
    stopAllActuation();
    commandActive = false;
    failsafeTriggered = true;
    setup_wifi();
  }

  if (!client.connected()) {
    stopAllActuation();
    connect_mqtt_once();
  } else {
    client.loop();
  }

  updateBuzzer();
  checkCommandTimeout();

  unsigned long now = millis();

  if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = now;
    publishTelemetry();
  }
}