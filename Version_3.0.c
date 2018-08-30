//Edited on 30th August,2018
//Reason: For redution of noise, we are adding debounce time of 20ms
#include <SPI.h>
#include <QueueArray.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <Wire.h>
#include <aREST.h>
#include <avr/wdt.h>
#include "Timer.h"
// Global Variables
Timer t;
#define relay 5
const byte interruptPin = 3;
volatile byte state = LOW;
volatile byte counter = 0;
volatile byte mode = 0;
volatile int a = 0,z =0;
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xFE, 0x40 };
volatile byte pkt = 0;
int debouncing_time = 20; //Debouncing Time in Milliseconds
volatile unsigned long last_micros;
QueueArray <byte> queue;
IPAddress ip(169,254,39,19);
EthernetServer server(80);
aREST rest = aREST();
byte cs;
int obj_available = 0;
void setup(void)
{ 
  Serial.begin(115200);
  Wire.begin(8);
  Serial.println("Setting things for you... Wait for the system to start");
  pinMode(relay, OUTPUT);
  pinMode(7, OUTPUT);  
  pinMode(interruptPin, INPUT_PULLUP);
  digitalWrite(relay,HIGH);
  attachInterrupt(digitalPinToInterrupt(interruptPin), Sensor_1_Interrupt, CHANGE);
  rest.variable("cs",&cs);
  rest.variable("Object",&obj_available);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);
  rest.function("C",conveyor_speed);
  rest.function("OA",object_queue_add);
  rest.set_id("001");
  rest.set_name("Nash_Robotics");
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP()); 
  wdt_enable(WDTO_4S);  
  mode = EEPROM.read(0);
  Serial.print("Checking Current Conveyor Speed Configuration in the Controller = ");
  Serial.println(mode);
  cs = mode;
}
int conveyor_speed(String command) {

  int state = command.toInt();
  cs = state;
  EEPROM.write(0, cs);
  Serial.print("Speed:");
  Serial.println(cs);
  return 1;

}
int object_queue_add(String command) {

  int state = command.toInt();
  queue.enqueue(state);
  Serial.print(state);
  Serial.println("\tADD");
  return 1;

}
void requestEvent() {

  if(pkt == 5)
  {
    send_conveyor_speed();
    Serial.print("Speed");
  }
  if(pkt == 6)
  {
    if(queue.isEmpty())
    {
      Wire.write(255);
      Serial.print("No Data");
      Serial.println(queue.count());
      return;
    }
    Wire.write(queue.dequeue());
    Serial.println("Sent");
  }
}
void receiveEvent(int howMany) {
  while (Wire.available()) {
    pkt = Wire.read();
    Serial.print(pkt);
  }
  if(pkt == 5)
  {
      send_conveyor_speed();
  }
  if(pkt == 6)
  {
    Serial.print("Request received from Controller!");
  }
}
void send_conveyor_speed()
{
  Wire.write(EEPROM.read(0));
}
//** ISR for Sensor_1 - Digital pin 3 **//
void Sensor_1_Interrupt()
{
  if((long)(micros() - last_micros) >= debouncing_time * 10000)
  {
    last_micros = micros();
  
    state = !state;
    if(state == 1)
    {
      obj_available = 1;
      if(mode == 2)
      {
        //Low Speed - 0.3m/s
        t.after(660,countervalue,(void*)1);
      }
      if(mode == 3)
      {
        //Medium Speed - 0.6m/s
        t.after(330,countervalue,(void*)1);
      }
      if(mode == 4)
      {
        //High Speed - 1.2m/s
        t.after(166,countervalue,(void*)1);
      }
    }
    if(state == 0)
    {
      counter++;
    }
  }
}
void countervalue()
{
  if(counter == 0)
  return;

  else
  {
    Serial.print("counter = ");
    Serial.println(counter);
    counter = 0;

    if(mode == 2)
    {      
      a = t.after(1666,completed,1);
      z = t.every(200,check,1);
      Serial.print("Z = ");
      Serial.println(z);

    }
    else if(mode == 3)
    {
      a = t.after(833,completed,1);
      z = t.every(200,check,1);
      Serial.print("Z = ");
      Serial.println(z);
    }
    else if(mode == 4)
    {
      a = t.after(416,completed,1);
      z = t.every(200,check,1);
      Serial.print("Z = ");
      Serial.println(z);
    }
  }
}
void completed()
{
  t.oscillate(7,50,LOW,5);
  obj_available = 0;
  Serial.println("Completed!");
  t.stop(z);
}
void check()
{
  if(counter > 0)
  {
    Serial.print("Error Object : CounterValue =");
    Serial.println(counter);
    digitalWrite(relay,LOW);
    delay(500);
    digitalWrite(relay,HIGH);
    Serial.println("Conveyor Turned off");
    counter = 0;
  }
}
void loop() {
  EthernetClient client = server.available();
  rest.handle(client);
  wdt_reset();
  t.update();
}
