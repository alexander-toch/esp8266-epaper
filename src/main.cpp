#include <secrets.h>
#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>
#include <fonts/GothamRoundedBook.h>
#include <fonts/GothamRoundedBold.h>
#include <fonts/GothamRoundedBoldBig.h>
#include <glyphs/weather.h>
#include <glyphs/icons.h>
#include <glyphs/weather_small.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <RTCMemory.h>
#include <map>

// DEBUG FLAG WHEN CONNECTED VIA USB
// #define PC_DEBUG

// select the display class and display driver class in the following file (new style):
#include "paper-config/GxEPD2_display_selection_new_style.h"

// set borders of display inside frame
#define OFFSET_LEFT 18
#define OFFSET_TOP 35
#define OFFSET_RIGHT 23
#define OFFSET_BOTTOM 80

const char *http_endpoint = "http://homeassistant.lan/api/states/sensor.epaper_esp8266_data";

const char *title = "WETTER";
const size_t GLYPH_SIZE_WEATHER = 100;
const size_t GLYPH_SIZE_WEATHER_SMALL = 45;

typedef const unsigned char *weather_icon;
std::map<std::string, weather_icon> weather_icon_map{
    {"clear-night", weather_clear_night},
    {"cloudy", weather_cloudy},
    {"fog", weather_foggy},
    {"hail", weather_thunderstorm},
    {"lightning", weather_thunderstorm},
    {"lightning-rainy", weather_thunderstorm},
    {"partlycloudy", weather_partly_cloudy},
    {"night-partly-cloudy", weather_partly_cloudy_night},
    {"pouring", weather_rainy},
    {"rainy", weather_rainy},
    {"snowy", weather_snowing},
    {"snowy-rainy", weather_snowing},
    {"sunny", weather_sunny},
    {"windy", weather_wind},
    {"windy-variant", weather_wind}};

typedef const unsigned char *weather_icon;
std::map<std::string, weather_icon> weather_icon_map_small{
    {"clear-night", weather_small_clear_night},
    {"cloudy", weather_small_cloudy},
    {"fog", weather_small_foggy},
    {"hail", weather_small_thunderstorm},
    {"lightning", weather_small_thunderstorm},
    {"lightning-rainy", weather_small_thunderstorm},
    {"partlycloudy", weather_small_partly_cloudy},
    {"night-partly-cloudy", weather_small_partly_cloudy_night},
    {"pouring", weather_small_rainy},
    {"rainy", weather_small_rainy},
    {"snowy", weather_small_snowing},
    {"snowy-rainy", weather_small_snowing},
    {"sunny", weather_small_sunny},
    {"windy", weather_small_wind},
    {"windy-variant", weather_small_wind}};

// RTC memory
typedef struct
{
  int counter;
} RTCData;

typedef struct
{
  float temperature_inside;
  float humidity_inside;
  float temperature_outside;
  float humidity_outside;
  float wind_speed;
  String weather_forecast_now;
  String weather_forecast_2h;
  float weather_forecast_2h_temp;
  String weather_forecast_2h_time;
  String weather_forecast_4h;
  float weather_forecast_4h_temp;
  String weather_forecast_4h_time;
  String weather_forecast_6h;
  float weather_forecast_6h_temp;
  String weather_forecast_6h_time;
  String weather_forecast_8h;
  float weather_forecast_8h_temp;
  String weather_forecast_8h_time;
  String time;
  String timestamp;
} HAData;

RTCMemory<RTCData> rtcMemory;
HAData haData;

size_t ctr = 0;

void writeDisplay();
bool loadRTCData();
String httpGETRequest(const char *);

void setup()
{
#ifdef PC_DEBUG
  Serial.begin(115200);
#endif

  WiFi.begin(ssid, password); // Connect to the network

  if (loadRTCData() == false || ctr % 12 == 0) // first load or every 20th time
  {
    // init if first start
    display.init(115200, true, 2, false); // initial = true
    display.setFullWindow();
  }
  else
  {
    // initial false for re-init after processor deep sleep wake up, if display power supply was kept
    // this can be used to avoid the repeated initial full refresh on displays with fast partial update
    display.init(115200, false, 2, false); // initial = false
    display.setPartialWindow(0, 0, display.width(), display.height());
  }
}

bool loadRTCData()
{
  if (rtcMemory.begin())
  {
    RTCData *data = rtcMemory.getData();
    ctr = ++data->counter;
    rtcMemory.save();
    return true;
  }
  else
  {
    RTCData *data = rtcMemory.getData();
    data->counter = 0;
    ctr = 0;
    rtcMemory.save();
    return false;
  }
}

const unsigned char *get_weather_icon(String forecast, bool small = false)
{
  if (small)
  {
    if (weather_icon_map_small.find(forecast.c_str()) != weather_icon_map_small.end())
    {
      return weather_icon_map_small[forecast.c_str()];
    }
    else
    {
      return weather_small_sunny;
    }
  }
  else
  {
    if (weather_icon_map.find(forecast.c_str()) != weather_icon_map.end())
    {
      return weather_icon_map[forecast.c_str()];
    }
    else
    {
      return weather_sunny;
    }
  }
}

void printForecast(int offset_x, int offset_y, weather_icon icon, float temperature, String time)
{
  display.setFont(&GothamRounded_Book14pt8b);

  display.setCursor(OFFSET_LEFT + offset_x, OFFSET_TOP + offset_y);
  display.print(time);

  display.drawBitmap(OFFSET_LEFT + offset_x + 15, OFFSET_TOP + offset_y + 8, icon, GLYPH_SIZE_WEATHER_SMALL, GLYPH_SIZE_WEATHER_SMALL, GxEPD_BLACK);

  if (temperature > 9.99)
  {
    display.setCursor(OFFSET_LEFT + offset_x + 10, OFFSET_TOP + offset_y + 10 + GLYPH_SIZE_WEATHER_SMALL + 28);
  }
  else
  {
    display.setCursor(OFFSET_LEFT + offset_x + 15, OFFSET_TOP + offset_y + 10 + GLYPH_SIZE_WEATHER_SMALL + 28);
  }
  display.setFont(&GothamRounded_Bold14pt8b);
  display.printf("%.0f°C", temperature);
}

void writeDisplay()
{
  display.setRotation(1);

  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.setFont(&GothamRounded_Bold32pt7b); // title
  display.getTextBounds(title, 0, 0, &tbx, &tby, &tbw, &tbh);

  display.firstPage();

  do
  {
    // print Title
    display.setCursor(((display.width() - tbw) / 2) - tbx, OFFSET_TOP + 80);
    display.print(title);

    // print big weather icon and temperature
    weather_icon weather_icon = get_weather_icon(haData.weather_forecast_now);
    display.drawBitmap(OFFSET_LEFT + 10, OFFSET_TOP + 110, weather_icon, GLYPH_SIZE_WEATHER, GLYPH_SIZE_WEATHER, GxEPD_BLACK);
    display.setCursor(OFFSET_LEFT + 110, OFFSET_TOP + 190);
    display.setFont(&GothamRounded_Bold48pt8b);
    display.printf("%.1f°C", haData.temperature_outside);

    // print humidity and wind
    display.setFont(&GothamRounded_Bold14pt8b);
    size_t weatherdetails_offset = 250;
    display.drawBitmap(OFFSET_LEFT + 30, OFFSET_TOP + weatherdetails_offset - GLYPH_SIZE_WEATHER_SMALL / 2, weather_small_wind, GLYPH_SIZE_WEATHER_SMALL, GLYPH_SIZE_WEATHER_SMALL, GxEPD_BLACK);
    display.setCursor(OFFSET_LEFT + 90, OFFSET_TOP + weatherdetails_offset + GLYPH_SIZE_WEATHER_SMALL / 4);
    display.printf("%.1fkm/h", haData.wind_speed);

    display.drawBitmap(OFFSET_LEFT + 230, OFFSET_TOP + weatherdetails_offset - GLYPH_SIZE_WEATHER_SMALL / 2, icon_humidity, GLYPH_SIZE_WEATHER_SMALL, GLYPH_SIZE_WEATHER_SMALL, GxEPD_BLACK);
    display.setCursor(OFFSET_LEFT + 290, OFFSET_TOP + weatherdetails_offset + GLYPH_SIZE_WEATHER_SMALL / 4);
    display.printf("%.1f%%", haData.humidity_outside);

    // print forecasts
    size_t forecast_offset_y = weatherdetails_offset + 60;
    printForecast(30, forecast_offset_y, get_weather_icon(haData.weather_forecast_2h, true), haData.weather_forecast_2h_temp, haData.weather_forecast_2h_time);
    printForecast(130, forecast_offset_y, get_weather_icon(haData.weather_forecast_4h, true), haData.weather_forecast_4h_temp, haData.weather_forecast_4h_time);
    printForecast(230, forecast_offset_y, get_weather_icon(haData.weather_forecast_6h, true), haData.weather_forecast_6h_temp, haData.weather_forecast_6h_time);
    printForecast(330, forecast_offset_y, get_weather_icon(haData.weather_forecast_8h, true), haData.weather_forecast_8h_temp, haData.weather_forecast_8h_time);

    // living room temperature
    display.drawBitmap(OFFSET_LEFT + 30, OFFSET_TOP + 430, icon_living_room, 80, 80, GxEPD_BLACK);
    display.drawBitmap(OFFSET_LEFT + 120, OFFSET_TOP + 430, icon40_thermometer, 40, 40, GxEPD_BLACK);
    display.drawBitmap(OFFSET_LEFT + 120, OFFSET_TOP + 470, icon40_humidity, 40, 40, GxEPD_BLACK);

    display.setFont(&GothamRounded_Bold14pt8b);
    display.setCursor(OFFSET_LEFT + 165, OFFSET_TOP + 460);
    display.printf("%.1f°C", haData.temperature_inside);

    display.setCursor(OFFSET_LEFT + 165, OFFSET_TOP + 500);
    display.printf("%.1f%%", haData.humidity_inside);

    display.drawBitmap(OFFSET_LEFT + 60, OFFSET_TOP + 550, image_dog, 80, 50, GxEPD_BLACK);
    display.drawBitmap(OFFSET_LEFT + 140, OFFSET_TOP + 550, image_dog_inv, 80, 50, GxEPD_BLACK);
    display.drawBitmap(OFFSET_LEFT + 220, OFFSET_TOP + 550, image_dog, 80, 50, GxEPD_BLACK);
    display.drawBitmap(OFFSET_LEFT + 300, OFFSET_TOP + 550, image_dog_inv, 80, 50, GxEPD_BLACK);

    display.setFont(&GothamRounded_Book14pt8b);
    display.getTextBounds("STAND 11:11", 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(((display.width() - tbw) / 2) - tbx - 10, OFFSET_TOP + 670);
    display.printf("STAND %s", haData.time.c_str());

    // calibrate borders
    // display.drawRect(OFFSET_LEFT, OFFSET_TOP, display.width() - OFFSET_LEFT - OFFSET_RIGHT, display.height() - OFFSET_TOP - OFFSET_BOTTOM, GxEPD_BLACK);
  } while (display.nextPage());
}

void getData()
{
  StaticJsonDocument<1024> doc; // change size if needed
  deserializeJson(doc, httpGETRequest(http_endpoint));
  haData.temperature_inside = doc["attributes"]["temperature_inside"].as<float>();
  haData.humidity_inside = doc["attributes"]["humidity_inside"].as<float>();
  haData.temperature_outside = doc["attributes"]["temperature_outside"].as<float>();
  haData.humidity_outside = doc["attributes"]["humidity_outside"].as<float>();
  haData.wind_speed = doc["attributes"]["wind_speed"].as<float>();
  haData.weather_forecast_now = doc["attributes"]["weather_forecast_now"].as<String>();
  haData.weather_forecast_2h = doc["attributes"]["weather_forecast_2h"].as<String>();
  haData.weather_forecast_2h_temp = doc["attributes"]["weather_forecast_2h_temp"].as<float>();
  haData.weather_forecast_2h_time = doc["attributes"]["weather_forecast_2h_time"].as<String>();
  haData.weather_forecast_4h = doc["attributes"]["weather_forecast_4h"].as<String>();
  haData.weather_forecast_4h_temp = doc["attributes"]["weather_forecast_4h_temp"].as<float>();
  haData.weather_forecast_4h_time = doc["attributes"]["weather_forecast_4h_time"].as<String>();
  haData.weather_forecast_6h = doc["attributes"]["weather_forecast_6h"].as<String>();
  haData.weather_forecast_6h_temp = doc["attributes"]["weather_forecast_6h_temp"].as<float>();
  haData.weather_forecast_6h_time = doc["attributes"]["weather_forecast_6h_time"].as<String>();
  haData.weather_forecast_8h = doc["attributes"]["weather_forecast_8h"].as<String>();
  haData.weather_forecast_8h_temp = doc["attributes"]["weather_forecast_8h_temp"].as<float>();
  haData.weather_forecast_8h_time = doc["attributes"]["weather_forecast_8h_time"].as<String>();
  haData.time = doc["attributes"]["time"].as<String>();
  haData.timestamp = doc["attributes"]["timestamp"].as<String>();
}

void loop()
{

  if (WiFi.status() == WL_CONNECTED)
  {
    getData();
    writeDisplay();
    display.powerOff();
#ifdef PC_DEBUG
    ESP.deepSleep(10 * 1000000); // will enter setup() again
#else
    ESP.deepSleep(10 * 60 * 1000000); // sleep for 10 minutes
#endif
  }
  delay(200);
};

String httpGETRequest(const char *serverName)
{
  WiFiClient client;
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(client, serverName);

  // Send HTTP POST request
  http.addHeader("Authorization", header);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0)
  {
#ifdef PC_DEBUG
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
#endif
    payload = http.getString();
  }
  else
  {
#ifdef PC_DEBUG
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
#endif
  }
  // Free resources
  http.end();

  return payload;
}