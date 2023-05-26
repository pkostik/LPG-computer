#include <TM1637.h>
#include "button.h"
#include <SPI.h>
#include "Mirf.h"
#include "nRF24L01.h"
#include "MirfHardwareSpiDriver.h"

#define CLK 3
#define DIO 5
#define BTN_PIN 2
#define DISPLAY_POWER_PIN 4
#define DISPLAY_DEFAULT_BRTN 4

Nrf24l Mirf = Nrf24l(10, 9);
bool key=false;
bool first_run=true;
int sensor=0;
int percent=0;

int current_mode=2;
int _min=5;
int _max=1023;
int distance_capacity=260;
int liter_capacity=37;

TM1637 disp(CLK, DIO);
Button btn(BTN_PIN);

bool do_we_need_re_power_up_display()
{
  return (current_mode<4)&&(current_mode>=0)&&(digitalRead(DISPLAY_POWER_PIN) == LOW);
}

void power_up_display()
{
    digitalWrite(DISPLAY_POWER_PIN, HIGH);
    disp.init();
    disp.set(DISPLAY_DEFAULT_BRTN);
}

void setup() {
  Serial.begin(9600);
  pinMode(DISPLAY_POWER_PIN, OUTPUT);
  power_up_display();
  Mirf.spi = &MirfHardwareSpi;
  Mirf.init();

  Mirf.setRADDR((byte *)"FGHIJ"); //Set your own address (receiver address) using 5 characters
  Mirf.payload = sizeof(sensor);
  Mirf.channel = 90;             //Set the used channel
  Mirf.config();
}

bool between(int value, int minv, int maxv)
{
  return (value>=minv)&&(value<maxv);
}

int oldValue;
int get_wpgh1_value()
{
      Mirf.getData((byte *) &sensor);
      Serial.print("Got data: ");
      Serial.println(sensor);
      return sensor;
}

void display_text(char ch, int value)
{
  if(oldValue != value)
  {
    disp.clearDisplay();
    disp.displayByte(ch, _empty, _empty, _empty);
    disp.displayInt(value);
    oldValue = value;
  }
}   

void loop() 
{
  key = btn.click();

  if (Mirf.dataReady()) {
    sensor = get_wpgh1_value();
  }
  
  if((key)||(first_run))
  {
    if(!first_run)
    {
      current_mode++;
      if(current_mode>4)current_mode=0;
    }
    
    if(do_we_need_re_power_up_display()){
      power_up_display();
    }  
      
    delay(100);
    disp.clearDisplay();
    disp.displayByte(_dash, _empty, _empty, _empty);
    disp.display(3, current_mode);
    delay(700);
  }

  switch (current_mode) 
  {
    case 0:
      //Show digital read value
      if(oldValue != sensor)
      {
        disp.clearDisplay();
        disp.displayIntZero(sensor);
        oldValue = sensor;
      }
      break;
      
    case 1:
      //Show percentage % value
      percent = map(sensor, _min, _max, 0, 100);
      display_text(_equal, percent);
      break;
      
   case 2:
      //Show liters capacity
      percent = map(sensor, _min, _max, 0, 100);
      display_text(_L, (percent*liter_capacity)/100);
      break;   
      
   case 3:
      //Show distance left
      percent = map(sensor, _min, _max, 0, 100);
      display_text(_under, (percent*distance_capacity)/100);
      break;  
      
   case 4:
      //Display power off
      disp.clearDisplay();
      digitalWrite(DISPLAY_POWER_PIN, LOW);
      break;                     
  }
       
  first_run = false;
  delay(150);
}
