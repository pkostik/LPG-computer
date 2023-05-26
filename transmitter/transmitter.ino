//Transmitter program

#include <SPI.h>
#include "Mirf.h"
#include "nRF24L01.h"
#include "MirfHardwareSpiDriver.h"
#define CALCULATION_NUMBER 10

Nrf24l Mirf = Nrf24l(10, 9);

int get_freq_int_value(int* arr)
{
    int arr_size = sizeof(arr)/sizeof(arr[0]);
    int num = 0;
    for (int i = 0; i < arr_size; i++) {
      num += arr[i];
    }
    return (int)num/arr_size;
}

int average_sensor_value()
{
  int sensor_data[CALCULATION_NUMBER];
  for (int i=0;i<CALCULATION_NUMBER;i++)
  {
    sensor_data[i] = analogRead(A0);
    delay(200);
  }
  
  return get_freq_int_value(sensor_data);
}

int value;
void setup()
{
  Serial.begin(9600);
  Mirf.spi = &MirfHardwareSpi;
  Mirf.init();
  //Set your own address (sender address) using 5 characters
  Mirf.setRADDR((byte *)"ABCDE");
  Mirf.payload = sizeof(value);
  Mirf.channel = 90;              //Set the channel used
  Mirf.config();
}

void loop()
{
  Mirf.setTADDR((byte *)"FGHIJ");           //Set the receiver address
  value = average_sensor_value();
  Mirf.send((byte *)&value);                //Send instructions, send value
  Serial.print("Wait for sending.....");
  while (Mirf.isSending()) delay(1);        //Until you send successfully, exit the loop
  Serial.print("Send success:");
  Serial.println(value);
}
