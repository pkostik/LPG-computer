#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define PACKET_SIZE 255
#define SERVER_HOST "192.168.4.22"

WiFiUDP udp;

char packetBuffer[PACKET_SIZE+1];
unsigned int localPort = 9999;
int sensorValue;

const char *ssid = "AP_name";
const char *password = "password";

int average_sensor_value()
{
	return analogRead(PIN_A0);
}

void setup()
{
  Serial.begin(115200);
  
  delay(10000);

  WiFi.begin(ssid, password);
  Serial.print(F("UDP Client : ")); 
  Serial.println(WiFi.localIP());
  udp.begin(localPort);
}

void loop() {
    //Serial.print(packetBuffer);
    sensorValue = average_sensor_value();
    for(int i=0;i<PACKET_SIZE;i++)packetBuffer[i]=0;
    sprintf(packetBuffer, "%d", sensorValue);
    //Serial.println();
    Serial.println(packetBuffer);
    udp.beginPacket(SERVER_HOST, localPort);
    //udp.write(sensorValue);
    udp.write(packetBuffer);
    udp.endPacket();	
    delay(500); 
}