#include <PZEM004Tv30.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include <PubSubClient.h>

// ---------- PZEM ----------
HardwareSerial PZEMSerial(2);
PZEM004Tv30 pzem(&PZEMSerial, 16, 17, 0x01);

// ---------- Memory ----------
Preferences prefs;
float todayEnergy = 0; 
int lastDay = -1;

// ---------- WiFi ----------
const char* ssid = "Techorizon";
const char* password = "Maa@1234";

// ---------- Time ----------
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

// ---------- ThingsBoard ----------
const char* TB_HOST = "demo.thingsboard.io";
const int   TB_PORT = 1883;  
const char* TB_TOKEN = "n8p88e4cyk2oove9bf43";

// ---------- MQTT ----------
WiFiClient espClient;
PubSubClient client(espClient);

// ---------- Reconnect Function ----------
void reconnectTB() {
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard...");
    if (client.connect("ESP32_Device", TB_TOKEN, NULL)) {
      Serial.println("Connected!");
    } else {
      Serial.print("Failed, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // PZEM UART
  PZEMSerial.begin(9600, SERIAL_8N1, 16, 17);

  // Load previous energy
  prefs.begin("energy", false);
  todayEnergy = prefs.getFloat("today", 0.0);

  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi connected");

  // Time sync
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Initialize MQTT
  client.setServer(TB_HOST, TB_PORT);

  Serial.println("Energy Monitoring Started");
}

void loop() {
  // Ensure MQTT connected
  if (!client.connected()) {
    reconnectTB();
  }
  client.loop();

  // Get time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Time error");
    delay(1000);
    return;
  }

  int currentDay = timeinfo.tm_mday;

  // -------- Reset at Midnight --------
  if (lastDay != -1 && currentDay != lastDay) {
    prefs.putFloat("yesterday", todayEnergy);
    todayEnergy = 0;
    prefs.putFloat("today", todayEnergy);
  }
  lastDay = currentDay;

  // -------- Read PZEM --------
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power   = pzem.power();

  if (!isnan(power)) {
    todayEnergy += (power / 1000.0) * (1.0 / 3600.0);
    prefs.putFloat("today", todayEnergy);
  }

  // -------- Upload to ThingsBoard --------
  String payload = "{";
  payload += "\"voltage\":" + String(voltage) + ",";
  payload += "\"current\":" + String(current) + ",";
  payload += "\"power\":" + String(power) + ",";
  payload += "\"todayEnergy\":" + String(todayEnergy);
  payload += "}";

  client.publish("v1/devices/me/telemetry", payload.c_str());

  Serial.println("Uploaded → " + payload);

  delay(1000);
}