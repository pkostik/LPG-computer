#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#include <bits/stdc++.h>

#define PACKET_SIZE 255
#define EEPROM_SIZE 64
#define STACK_SIZE 40

std::deque<int> stack;
char packetBuffer[PACKET_SIZE+1];
AsyncWebServer server(80);
IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);
WiFiUDP udp;
unsigned int localPort = 9999;

int sensorValue = 0;
float percent = 0;
float liters = 0;
float distance = 0;
bool isFistRun=true;
bool key=false;
int menu;

struct EEPROMData
{
  int liter_capacity;
  int distance_capacity;
  int menu;
  unsigned long MinSensorValue;
  unsigned long MaxSensorValue;
  byte theme;
};

EEPROMData data;

void resetDataToDefault()
{
  data.distance_capacity=260;
  data.liter_capacity=37;
  data.menu=0;
  data.MinSensorValue=43;
  data.MaxSensorValue=247;
}

void SaveData()
{
  int eeAddress = 16;
  EEPROM.put(eeAddress, data);
  EEPROM.commit();
}

void LoadData()
{
  int eeAddress = 16;
  data = EEPROM.get(eeAddress, data);
}

const char* ssid = "AP_name";
const char* password = "password";

String PARAM_INPUT_2 = "input2";
String PARAM_INPUT_3 = "reset";
String PARAM_INPUT_4 = "input4";
String PARAM_INPUT_5 = "input5";
String PARAM_INPUT_6 = "input6";
String PARAM_INPUT_7 = "input7";
String PARAM_INPUT_8 = "input8";
String html PROGMEM = "<!DOCTYPE HTML><html><head> <title>LPG computer settings</title> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> </head><body> <br> <form action=\"/get\"> Tank,[L]: <input type=\"text\" name=\"input4\" value=\"{input4}\"><br> Distance,[km]: <input type=\"text\" name=\"input5\" value=\"{input5}\"><br> Start Menu: <input type=\"text\" name=\"input6\" value=\"{input6}\"><br> ADC min: <input type=\"text\" name=\"input7\" value=\"{input7}\"><br>ADC max: <input type=\"text\" name=\"input8\" value=\"{input8}\"><br> Theme[0-5]: <input type=\"text\" name=\"input2\" value=\"{input2}\"><br> <input type=\"submit\" value=\"Save\"> </form> <form action=\"/get\"> <input type=\"submit\" value=\"Reset\" name=\"reset\"> </form> </body></html>";
String html_basic PROGMEM = html;

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

String getHtml()
{
  html = html_basic;
  html.replace("{input2}", itoa(data.theme, packetBuffer, 10));
  html.replace("{input4}", itoa(data.liter_capacity, packetBuffer, 10));
  html.replace("{input5}", itoa(data.distance_capacity, packetBuffer, 10));
  html.replace("{input6}", itoa(data.menu, packetBuffer, 10));
  html.replace("{input7}", itoa(data.MinSensorValue, packetBuffer, 10));
  html.replace("{input8}", itoa(data.MaxSensorValue, packetBuffer, 10));
  return html;
}

void init_networking(void)
{
  EEPROM.begin(EEPROM_SIZE);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid,password);

  IPAddress IP = WiFi.softAPIP();
  server.begin();

  LoadData();
  menu = data.menu;

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getHtml());
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {

    String min_value;
    String max_value;
    String tank_value;
    String distance_value;
    String menu_value;
    String theme;

    if (request->hasParam(PARAM_INPUT_3)) 
    {
      Serial.println("reset");
      resetDataToDefault();
      SaveData();
    }
    else
    if (request->hasParam(PARAM_INPUT_2) || request->hasParam(PARAM_INPUT_4) || 
        request->hasParam(PARAM_INPUT_5) || request->hasParam(PARAM_INPUT_6) || 
        request->hasParam(PARAM_INPUT_7) || request->hasParam(PARAM_INPUT_8))
    {
      theme = request->getParam(PARAM_INPUT_2)->value();
      if(!theme.isEmpty())
      {
        Serial.println(theme);
        data.theme=atoi(theme.c_str()); 
      }
      tank_value = request->getParam(PARAM_INPUT_4)->value();
      if(!tank_value.isEmpty())
      {
        Serial.println(tank_value);
        data.liter_capacity=atoi(tank_value.c_str()); 
      }
      distance_value = request->getParam(PARAM_INPUT_5)->value();
      if(!distance_value.isEmpty())
      {
        Serial.println(distance_value);
        data.distance_capacity = atoi(distance_value.c_str()); 
      }
      menu_value = request->getParam(PARAM_INPUT_6)->value();
      if(!menu_value.isEmpty())
      {
        Serial.println(menu_value);
        data.menu = atoi(menu_value.c_str()); 
        menu=data.menu; 
      }
      min_value = request->getParam(PARAM_INPUT_7)->value();
      if(!min_value.isEmpty())
      {
        Serial.println(min_value);
        data.MinSensorValue = atoi(min_value.c_str());
      }
      max_value = request->getParam(PARAM_INPUT_8)->value();
      if(!max_value.isEmpty())
      {
        Serial.println(max_value);
        data.MaxSensorValue = atoi(max_value.c_str());
      }
      SaveData();      
    }
     
    request->send(200, "text/html", "<a href=\"/\">Home</a>");
  });

  server.onNotFound(notFound);
  server.begin();
  udp.begin(localPort);
}
