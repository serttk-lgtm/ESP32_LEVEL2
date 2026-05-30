#ifndef DEV_WEATHER_H
#define DEV_WEATHER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// AQI index -> ข้อความ (มาตรฐาน OpenWeatherMap 1-5)
static const char* aqiLabel(int aqi) {
  switch (aqi) {
    case 1: return "Good";
    case 2: return "Fair";
    case 3: return "Moderate";
    case 4: return "Poor";
    case 5: return "V.Poor";
    default: return "N/A";
  }
}

struct WeatherData {
  float   temp        = 0;    // °C
  int     humidity    = 0;    // %
  int     rainChance  = 0;    // % (pop จาก forecast หรือ rain flag จาก current)
  float   pm25        = 0;    // µg/m³
  int     aqi         = 0;    // 1-5
  bool    valid       = false;
  unsigned long fetchedAt = 0; // millis()
};

class DevWeather {
private:
  WeatherData data;

  // ดึง JSON จาก URL (HTTPS, ไม่ verify cert สำหรับ embedded)
  bool fetchJson(const String& url, JsonDocument& doc) {
    WiFiClientSecure client;
    client.setInsecure();  // ไม่ verify SSL cert — ยอมรับได้บน embedded

    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(10000);
    int code = http.GET();

    if (code != 200) {
      Serial.printf("[Weather] HTTP %d for %s\n", code, url.c_str());
      http.end();
      return false;
    }

    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();

    if (err) {
      Serial.printf("[Weather] JSON parse error: %s\n", err.c_str());
      return false;
    }
    return true;
  }

  // Current Weather API — temp, humidity, rain flag
  bool fetchCurrent() {
    String url = "https://api.openweathermap.org/data/2.5/weather"
                 "?lat=" OWM_LAT "&lon=" OWM_LON
                 "&units=metric&appid=" OWM_API_KEY;

    JsonDocument doc;
    if (!fetchJson(url, doc)) return false;

    data.temp     = doc["main"]["temp"].as<float>();
    data.humidity = doc["main"]["humidity"].as<int>();

    // rain.1h มีค่า = กำลังฝนตก; rain chance ดูจาก pop ใน forecast
    // ที่นี่ใช้ความน่าจะเป็นจาก clouds + weather id แทน (current ไม่มี pop)
    // weather id 2xx=thunder, 3xx=drizzle, 5xx=rain, 6xx=snow, 7xx=atmo
    int wid = doc["weather"][0]["id"].as<int>();
    bool raining = (wid >= 200 && wid < 700);
    // ถ้ากำลังฝนตกอยู่ให้ 100%, ไม่ก็ดู cloud cover เป็น proxy
    if (raining) {
      data.rainChance = 100;
    } else {
      int clouds = doc["clouds"]["all"].as<int>();  // 0-100
      // แปลง cloud cover → rain chance อย่างหยาบๆ (ไม่มี pop ใน current)
      data.rainChance = clouds / 2;  // max 50% ถ้าฟ้าครึ้มเต็มที่แต่ไม่ฝนตก
    }

    return true;
  }

  // Forecast API — ดึง pop (probability of precipitation) ช่วง 3h ถัดไป
  bool fetchForecastPop() {
    String url = "https://api.openweathermap.org/data/2.5/forecast"
                 "?lat=" OWM_LAT "&lon=" OWM_LON
                 "&units=metric&cnt=1&appid=" OWM_API_KEY;

    JsonDocument doc;
    if (!fetchJson(url, doc)) return false;

    float pop = doc["list"][0]["pop"].as<float>();  // 0.0 - 1.0
    data.rainChance = (int)(pop * 100);
    return true;
  }

  // Air Pollution API — PM2.5, AQI
  bool fetchAirPollution() {
    String url = "https://api.openweathermap.org/data/2.5/air_pollution"
                 "?lat=" OWM_LAT "&lon=" OWM_LON
                 "&appid=" OWM_API_KEY;

    JsonDocument doc;
    if (!fetchJson(url, doc)) return false;

    data.aqi  = doc["list"][0]["main"]["aqi"].as<int>();
    data.pm25 = doc["list"][0]["components"]["pm2_5"].as<float>();
    return true;
  }

public:
  // อัปเดตข้อมูลทั้งหมด — คืนค่า true ถ้าสำเร็จอย่างน้อย Current Weather
  bool update() {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[Weather] WiFi not connected");
      return false;
    }

    Serial.println("[Weather] Fetching...");
    bool ok = fetchCurrent();
    if (ok) {
      fetchForecastPop();   // ถ้าล้มเหลวก็ยังมี rainChance จาก current
      fetchAirPollution();  // optional — ถ้าล้มเหลว pm25/aqi ยังเป็น 0
      data.valid = true;
      data.fetchedAt = millis();
      Serial.printf("[Weather] T=%.1f°C H=%d%% Rain=%d%% PM2.5=%.1f AQI=%d(%s)\n",
                    data.temp, data.humidity, data.rainChance,
                    data.pm25, data.aqi, aqiLabel(data.aqi));
    }
    return ok;
  }

  const WeatherData& getData() const { return data; }

  // ตรวจว่าถึงเวลาอัปเดตหรือยัง
  bool isDue(unsigned long intervalSec = WEATHER_UPDATE_SEC) const {
    if (!data.valid) return true;
    return (millis() - data.fetchedAt) >= intervalSec * 1000UL;
  }
};

#endif // DEV_WEATHER_H
