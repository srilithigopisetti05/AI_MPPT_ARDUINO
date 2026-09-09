#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "model_coefficients.h"

// ==========================================
// CONFIGURATION & CREDENTIALS
// ==========================================
const char* WIFI_SSID       = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD   = "YOUR_WIFI_PASSWORD";

// GPS Coordinates
const float LATITUDE        = 17.3850; 
const float LONGITUDE       = 78.4867;

// Execution Timing Configuration
const uint32_t API_UPDATE_INTERVAL_MS = 600000; // 10 mins
const uint32_t UART_TX_INTERVAL_MS    = 5000;   // 5s

// Hardware Serial 2 Pins
#define RXD2 16
#define TXD2 17

// Sanity Limits
const float VMPP_MIN_CLAMP = 12.0;
const float VMPP_MAX_CLAMP = 22.0;

// State Variables
float temp_2m = 25.0;
float humidity_2m = 50.0;
float cloud_cover = 0.0;
float shortwave_irradiance = 800.0;

float nano_pv_v = 0.0;
float nano_pv_i = 0.0;
float nano_pv_p = 0.0;
float nano_bat_v = 12.6;
int   nano_pwm = 0;
bool  nano_ai_active = false;

float current_predicted_vmpp = 18.0;
uint32_t last_api_call = 0;
uint32_t last_uart_tx = 0;

void connectToWiFi();
void fetchWeatherData();
void processNanoTelemetry();
void printExhibitionDashboard();

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println(F("\n========================================"));
  Serial.println(F("    ESP32 AI-MPPT SUPERVISORY NODE      "));
  Serial.println(F("========================================"));

  connectToWiFi();
  fetchWeatherData();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  uint32_t current_time = millis();
  if (current_time - last_api_call >= API_UPDATE_INTERVAL_MS || last_api_call == 0) {
    last_api_call = current_time;
    fetchWeatherData();
  }

  processNanoTelemetry();

  if (current_time - last_uart_tx >= UART_TX_INTERVAL_MS) {
    last_uart_tx = current_time;

    float raw_prediction = predict_vmpp_embedded(
        temp_2m, 
        humidity_2m, 
        cloud_cover, 
        shortwave_irradiance, 
        nano_bat_v
    );

    if (raw_prediction < VMPP_MIN_CLAMP) raw_prediction = VMPP_MIN_CLAMP;
    if (raw_prediction > VMPP_MAX_CLAMP) raw_prediction = VMPP_MAX_CLAMP;
    current_predicted_vmpp = raw_prediction;

    Serial2.print("VMPP:");
    Serial2.println(current_predicted_vmpp, 2);

    printExhibitionDashboard();
  }
}

void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print(F("[WiFi] Connecting to: "));
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\n[WiFi] Connected successfully."));
  } else {
    Serial.println(F("\n[WiFi] Connection Failed. Operating in Offline mode."));
  }
}

void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE, 4) +
               "&longitude=" + String(LONGITUDE, 4) +
               "&current=temperature_2m,relative_humidity_2m,cloud_cover,shortwave_radiation";

  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        temp_2m = doc["current"]["temperature_2m"] | temp_2m;
        humidity_2m = doc["current"]["relative_humidity_2m"] | humidity_2m;
        cloud_cover = doc["current"]["cloud_cover"] | cloud_cover;
        shortwave_irradiance = doc["current"]["shortwave_radiation"] | shortwave_irradiance;
      }
    }
    http.end();
  }
}

void processNanoTelemetry() {
  while (Serial2.available() > 0) {
    String line = Serial2.readStringUntil('\n');
    line.trim();
    
    if (line.startsWith("DATA,")) {
      int pvv_idx = line.indexOf("PVV,");
      int pvi_idx = line.indexOf(",PVI,");
      int pvp_idx = line.indexOf(",PVP,");
      int bat_idx = line.indexOf(",BAT,");
      int pwm_idx = line.indexOf(",PWM,");
      int ai_idx  = line.indexOf(",AI,");

      if (pvv_idx != -1 && pvi_idx != -1 && bat_idx != -1) {
        nano_pv_v = line.substring(pvv_idx + 4, pvi_idx).toFloat();
        nano_pv_i = line.substring(pvi_idx + 5, pvp_idx).toFloat();
        nano_pv_p = line.substring(pvp_idx + 5, bat_idx).toFloat();
        nano_bat_v = line.substring(bat_idx + 5, pwm_idx).toFloat();
        nano_pwm   = line.substring(pwm_idx + 5, ai_idx).toInt();
        nano_ai_active = (line.substring(ai_idx + 4).toInt() == 1);
      }
    }
  }
}

void printExhibitionDashboard() {
  Serial.println(F("\n========================================"));
  Serial.println(F(" AI-ASSISTED MPPT SOLAR CONTROLLER    "));
  Serial.println(F("========================================"));
  Serial.printf("Temperature : %.1f C | Solar: %.1f W/m2\n", temp_2m, shortwave_irradiance);
  Serial.printf("Target VMPP : %.2f V\n", current_predicted_vmpp);
  Serial.printf("PV Voltage  : %.2f V | Battery: %.2f V\n", nano_pv_v, nano_bat_v);
  Serial.printf("AI Status   : %s\n", nano_ai_active ? "ACTIVE" : "FALLBACK (OFFLINE)");
  Serial.println(F("========================================\n"));
}



#code on arduino ide