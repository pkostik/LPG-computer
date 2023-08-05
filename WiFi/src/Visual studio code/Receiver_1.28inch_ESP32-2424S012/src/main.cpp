#define LGFX_USE_V1
#include "Arduino.h"
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include "CST816D.h"
#include "LGFX.h"
#include "networking.h"

void setup()
{
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
//  tft.setBrightness(200);
//  tft.setSwapBytes(true);
  tft.init();
  tft.initDMA();
  tft.startWrite();
  tft.setRotation(2);
  touch.begin();
  init_disp();
  init_networking();
}

bool isScreenTouched()
{
  bool FingerNum;
  uint8_t gesture;
  uint16_t touchX, touchY;
  return touch.getTouch(&touchX, &touchY, &gesture);
}

float oldValue=0;
bool lastConnectState=false;
void ringMeter(float value, int vmin, int vmax, int x, int y, int r, byte scheme, char * units)
{  
  x += r; y += r;   // Calculate coords of centre of ring
  int w = r / 6;    // Width of outer ring is 1/4 of radius
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
      case 0: colour = TFT_RED; break; // Fixed colour
      case 1: colour = TFT_GREEN; break; // Fixed colour
      case 2: colour = TFT_BLUE; break; // Fixed colour
      case 3: colour = TFT_ORANGE; break; // Fixed colour
      case 4: colour = TFT_BROWN; break; // Fixed colour
      case 5: colour = TFT_DARKGREEN; break; // Fixed colour                 
      default: colour = TFT_BLUE; break; // Fixed colour
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
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, colour);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, colour);
      text_colour = colour; // Save the last colour drawn
    }
    else // Fill in blank segments
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_BLACK);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_BLACK);
    }
  }
}

void drawStr(char *text, float value, unsigned long min, unsigned long max)
{
  if((value == oldValue)&&(lastConnectState == WiFi.softAPgetStationNum())&&(!isFistRun)) return;
  tft.clear();
  if(WiFi.softAPgetStationNum())
    tft.fillRect(110,40,20,20,TFT_GREEN);
  else
    tft.fillRect(110,40,20,20,TFT_RED);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawCenterString(text, 120, 90, &fonts::DejaVu24);
  char buf[10];
  byte len = 4; if (value > 999) len = 5;
  dtostrf(value, len, 1, buf);
  tft.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
  tft.drawString(buf, 80, 130, &fonts::DejaVu40);
  ringMeter(value, min, max, 0, 0, 120, data.theme, text);
  oldValue = value;
  lastConnectState = WiFi.softAPgetStationNum();
}

void drawStrAdv(char *text, float value, unsigned long min, unsigned long max)
{
  if(stack.size()<STACK_SIZE-1)
  {
    drawStr("Stack size", stack.size(), 0, STACK_SIZE);
  }
  else
  {
    drawStr(text, value, min, max);
  }  
}


void handleData()
{
    switch(menu)
    {   
      case 0:
        drawStrAdv("ADC",sensorValue, data.MinSensorValue, data.MaxSensorValue);
      break;   
      case 1:
        drawStrAdv("%",percent, 0, 100);
      break;   
      case 2:
        drawStrAdv("L",liters, 0, data.liter_capacity);
      break;  
      case 3:
        drawStrAdv("Distance",distance, 0, data.distance_capacity);
      break; 
      default:
        drawStrAdv("ADC",sensorValue, data.MinSensorValue, data.MaxSensorValue);
      break;            
    }  
}

#define MAX_MODE 3

void loop()
{
    key = isScreenTouched();
    if(key)
    {
      menu++;
      if(menu>MAX_MODE)menu=0;
      oldValue=-1;
      tft.drawString(itoa(menu, packetBuffer, 10), 110, 10);
      delay(500);
    }

    int packetSize = udp.parsePacket();
    if(packetSize>0)
    {
        int len = udp.read(packetBuffer, PACKET_SIZE);
        if (len > 0) 
        {
          packetBuffer[len+1]=0;
          sensorValue = atoi(packetBuffer);

          stack.push_front(sensorValue);
          while(stack.size() > STACK_SIZE)
          {
            stack.pop_back();
          }

          if(stack.size()>0)
          {
            sensorValue = std::accumulate(stack.begin(), stack.end(), 0.0) / stack.size();
            percent = map(sensorValue, data.MinSensorValue, data.MaxSensorValue, 0, 100);
            liters = (percent*data.liter_capacity)/100;
            distance  = (percent*data.distance_capacity)/100;
          }
        }
    }
  handleData();
  isFistRun=false;
  delay(500);
}