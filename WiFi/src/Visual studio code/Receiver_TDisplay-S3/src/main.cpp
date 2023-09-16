#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#include <button.h>
#include "TFT_eSPI.h"
#include <bits/stdc++.h>
#include <ESP32Ping.h>

#define MAX_MODE 6
#define PIN_POWER_ON 15
#define PIN_ADDITIONAL_BUTTON 21
#define H 170
#define W 320
#define M 10
#define PACKET_SIZE 255
#define EEPROM_SIZE 64
#define QUEUE_SIZE 30
#define TOLERANCE 10

std::deque<double> queue;
Button btn2(PIN_ADDITIONAL_BUTTON);
bool wifi_is_connected = false;
TFT_eSPI lcd = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&lcd);
char packetBuffer[PACKET_SIZE+1];
AsyncWebServer server(80);
IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);
WiFiUDP udp;
unsigned int localPort = 9999;

double sensorValue = 0.0;
double percent = 0;
double liters = 0;
double distance = 0;
double initial_liters = 0;
bool initial_value_calculated = false;
bool timer_disconnected=true;
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

void ringMeter(float value, int vmin, int vmax, int x, int y, int r, byte scheme, String units)
{
  sprite.fillSprite(TFT_BLACK);
  
  x += r; y += r;   // Calculate coords of centre of ring

  int w = r / 4;    // Width of outer ring is 1/4 of radius
  
  int angle = 150;  // Half the sweep angle of meter (300 degrees)

  int text_colour = 0; // To hold the text colour

  int v = map(value, vmin, vmax, -angle, angle); // Map the value to an angle v

  byte seg = 5; // Segments are 5 degrees wide = 60 segments for 300 degrees
  byte inc = 5; // Draw segments every 5 degrees, increase to 10 for segmented ring

  // Draw colour blocks every inc degrees
  for (int i = -angle; i < angle; i += inc) {

    // Choose colour from scheme
    int colour = 0;
    switch (scheme) {
      case 0: colour = TFT_RED; break; 
      case 1: colour = TFT_GREEN; break; 
      case 2: colour = TFT_BLUE; break; 
      case 3: colour = TFT_BROWN; break;
      case 4: colour = TFT_DARKCYAN; break; 
      case 5: colour = TFT_OLIVE; break;
      case 6: colour = TFT_CYAN; break; 
      case 7: colour = TFT_YELLOW; break; 
      case 8: colour = TFT_DARKGREEN; break; 
      case 9: colour = TFT_WHITE; break; 
      default: colour = TFT_BLUE; break; 
    }

    // Calculate pair of coordinates for segment start
    float sx = cos((i - 90) * 0.0174532925);
    float sy = sin((i - 90) * 0.0174532925);
    uint16_t x0 = sx * (r - w) + x;
    uint16_t y0 = sy * (r - w) + y;
    uint16_t x1 = sx * r + x;
    uint16_t y1 = sy * r + y;

    // Calculate pair of coordinates for segment end
    float sx2 = cos((i + seg - 90) * 0.0174532925);
    float sy2 = sin((i + seg - 90) * 0.0174532925);
    int x2 = sx2 * (r - w) + x;
    int y2 = sy2 * (r - w) + y;
    int x3 = sx2 * r + x;
    int y3 = sy2 * r + y;

    if (i < v) { // Fill in coloured segments with 2 triangles
      sprite.fillTriangle(x0, y0, x1, y1, x2, y2, colour);
      sprite.fillTriangle(x1, y1, x2, y2, x3, y3, colour);
      text_colour = colour; // Save the last colour drawn
    }
    else // Fill in blank segments
    {
      sprite.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_DARKGREY);
      sprite.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_DARKGREY);
    }
  }
}


void drawStr(String text, double value, unsigned long min, unsigned long max)
{
    int xpos = 10, ypos = 20, gap = 4, radius = 70;
    ringMeter(value, min, max, xpos, ypos, radius, data.theme, text);

    char buf[10];
    byte len = 4; if (value > 999) len = 5;
    dtostrf(value, len, 1, buf);

    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.drawString(buf, 150, 75, 7);

    xpos +=radius;
    ypos +=radius;
    sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    sprite.drawCentreString(text, xpos+130, ypos + 30, 4);

    if(wifi_is_connected)
      sprite.fillRect(xpos-10,ypos-10,20,20,TFT_GREEN);
    else
      sprite.fillRect(xpos-10,ypos-10,20,20,TFT_RED);

    if(queue.size()<QUEUE_SIZE)
    { 
      initial_value_calculated = false;
      sprite.drawString(String(queue.size()), xpos-10, ypos+50, 4);
    }
    else
    {
      if(!initial_value_calculated)
      {
        initial_value_calculated = true;
        initial_liters = liters;
      }
    }

    sprite.pushSprite(0,0);
}

void handleData()
{
    switch(menu)
    {
      case 0:
        sprite.fillSprite(TFT_BLACK);
        sprite.fillRoundRect(M,M,W-M*2,H-M*2,4, TFT_BLUE);
        sprite.fillRect(M*2+W/2,H/2+M, W/2-M*4, H/2-M*3, TFT_BLACK);
        sprite.setTextColor(TFT_WHITE,TFT_BLACK);
        sprite.fillRect(M*2,M*2, W/2-M*2, H/2-M*3, TFT_BLACK);
        sprite.fillRect(M*2,H/2+M, W/2-M*2, H/2-M*3, TFT_BLACK);
        sprite.fillRect(M*2+W/2,M*2, W/2-M*4, H/2-M*3, TFT_BLACK);
        sprite.setTextColor(TFT_WHITE,TFT_BLACK);
        sprite.drawString(String(sensorValue,0),M*3,M*5,4);
        sprite.drawString(String(percent,1),M*4+W/2,M*5,4);
        sprite.drawString(String(liters,1),M*3,M*4+H/2,4);
        sprite.drawString(String(distance,1),M*4+W/2-15,M*4+H/2,4);
        //Draw captions
        sprite.setTextColor(TFT_GREEN, TFT_BLACK);
        sprite.drawString(String("ADC"),M*2 + W/4,
          M*5,4);
        sprite.drawString(String("%"),M*2 + W/2 + W/4 +15,
          M*5,4);
        sprite.drawString(String("L"),M*2 + W/4 +15,
          M*4+H/2,4);
        sprite.drawString(String("D"),M*2 + W/2+W/4 +15,
          M*4+H/2,4);
        sprite.pushSprite(0,0);
      break;     
      case 1:
        drawStr("ADC",sensorValue, data.MinSensorValue, data.MaxSensorValue);
      break;   
      case 2:
        drawStr("%",percent, 0, 100);
      break;   
      case 3:
        drawStr("L",liters, 0, data.liter_capacity);
      break;  
      case 4:
        drawStr("Distance",distance, 0, data.distance_capacity);
      break;
      case 5:
        drawStr("L per trip",liters - initial_liters, 0, data.liter_capacity);
      break;
      case 6:
        sprite.fillSprite(TFT_BLACK);
        sprite.setTextColor(TFT_WHITE, TFT_BLACK);
        sprite.drawString(String(liters,2), 100, 30, 6);
        if(queue.size()<QUEUE_SIZE)
        { 
          initial_value_calculated = false;
        }
        else
        {
          if(!initial_value_calculated && initial_liters<=0.001)
          {
            initial_value_calculated = true;
            initial_liters = liters;
          }
        }       
        sprite.drawString(String((liters - initial_liters),2), 100, 90, 6);
        sprite.drawString(String((sensorValue),2), 100, 150, 6);
        sprite.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
        sprite.drawString("L", 10, 30, 4);
        sprite.drawString("L, trip", 10, 90, 4);
        sprite.drawString("ADC", 10, 150, 4);
        if(wifi_is_connected)
          sprite.fillRect(W-40,H-40,20,20,TFT_GREEN);
        else
          sprite.fillRect(W-40,H-40,20,20,TFT_RED);    
        sprite.pushSprite(0,0);
      break;                     
    }  
}

void resetDataToDefault()
{
  Serial.println("Reset data");
  data.distance_capacity=260;
  data.liter_capacity=40;
  data.menu=3;
  data.MinSensorValue=61;
  data.MaxSensorValue=233;
}

void SaveData()
{
  int eeAddress = 16;
  Serial.println("Save data:");
  EEPROM.put(eeAddress, data);
  EEPROM.commit();
}

void LoadData()
{
  int eeAddress = 16;
  data = EEPROM.get(eeAddress, data);
  Serial.println("Load data:");
}

// REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssid = "AP_name";
const char* password = "password";

String PARAM_INPUT_2 = "input2";
String PARAM_INPUT_3 = "reset";
String PARAM_INPUT_4 = "input4";
String PARAM_INPUT_5 = "input5";
String PARAM_INPUT_6 = "input6";
String PARAM_INPUT_7 = "input7";
String PARAM_INPUT_8 = "input8";
String html PROGMEM = "<!DOCTYPE HTML><html><head> <title>LPG computer settings</title> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> </head><body> <br> <form action=\"/get\"> Tank,[L]: <input type=\"text\" name=\"input4\" value=\"{input4}\"><br> Distance,[km]: <input type=\"text\" name=\"input5\" value=\"{input5}\"><br> Start Menu: <input type=\"text\" name=\"input6\" value=\"{input6}\"><br> ADC min: <input type=\"text\" name=\"input7\" value=\"{input7}\"><br>ADC max: <input type=\"text\" name=\"input8\" value=\"{input8}\"><br> Theme[0-9]: <input type=\"text\" name=\"input2\" value=\"{input2}\"><br> <input type=\"submit\" value=\"Save\"> </form> <form action=\"/get\"> <input type=\"submit\" value=\"Reset\" name=\"reset\"> </form> </body></html>";
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

void setup() {
  //delay(10000);
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(ssid,password) ? "Ready" : "Failed!");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
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
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  lcd.setRotation(3);
  sprite.createSprite(W, H);
  sprite.setTextDatum(3);
  sprite.setSwapBytes(true);
}

int looper = 0;
IPAddress remoteIp;

void loop() 
{
  if(looper>200000)looper=0;
  if(looper%500==0)
  {
    key = btn2.click();
    if(key)
    {
      menu++;
      if(menu>MAX_MODE)menu=0;
      Serial.println(menu); 
    }
    int packetSize = udp.parsePacket();
    if(packetSize>0)
    {
        timer_disconnected = false;
        int len = udp.read(packetBuffer, PACKET_SIZE);
        if (len > 0) 
        {
          packetBuffer[len+1]=0;
          sensorValue = atoi(packetBuffer);
          remoteIp = udp.remoteIP();

          if(queue.size()>0)
          {
            double o = std::accumulate(queue.begin(), queue.end(), 0.0) / queue.size();
            if((sensorValue<=o+o/TOLERANCE)&&(sensorValue>o-o/TOLERANCE))
            {
              queue.push_front(sensorValue);
            }
          }
          else
          {
            queue.push_front(sensorValue);
          }

          while(queue.size() > QUEUE_SIZE)
          {
            queue.pop_back();
          }

          if(queue.size()>0)
          {
            sensorValue = std::accumulate(queue.begin(), queue.end(), 0.0) / queue.size();
            percent = map(sensorValue, data.MinSensorValue, data.MaxSensorValue, 0, 100);
            liters = (percent*data.liter_capacity)/100;
            distance  = (percent*data.distance_capacity)/100;
          }
        }
    }
  }

  if(looper%4000==0)
  {
    wifi_is_connected = WiFi.softAPgetStationNum();
    //wifi_is_connected = Ping.ping(remoteIp, 3);
    //Serial.println(remoteIp.toString());
    //Serial.println(wifi_is_connected);
  }

  handleData();
  looper += 500;
  delay(500);
}
