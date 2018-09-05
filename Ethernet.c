/*
Project : Conveyor Belt
Manager : Mr.Logesh
Guide : Mr.Raghul
Author : Mr.Karthik Raj K
Co-Author : Mr.Amith J
Board : Arduino with Ethernet Shield
Purpose : To establish communication between Intel Server and Divertor Controller
Date : 01 September,2018
*/
// Libraries
#include <SPI.h>
#include <QueueArray.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <Wire.h>
#include <aREST.h>
#include <avr/wdt.h>
#include "Timer.h"

Timer t; //Timer Object Creation
#define relay 5 //Defining Digital pin - 5 as Relay
const byte ledPin = 13; //Initializing Digital pin - 13 as ledPin
const byte interruptPin = 3;//Initializing Digital pin - 3 as Sensor-1 Interrupt
//Initializing state - for Interrupt state change monitoring
volatile byte state = LOW;//Initializing state to LOW
volatile byte counter = 0;//counter - for number of objects arriving at Sensor_1
volatile byte mode = 0;// mode - for selecting Conveyor speed
volatile int a = 0,z =0;//operational Variables

// Enter a MAC address for your controller below.
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xFE, 0x40 };
volatile byte send_pkt = 0;
volatile byte c = 0;
// create a queue
QueueArray <byte> queue;
// IP address
IPAddress ip(169,254,39,19);
// Ethernet server
EthernetServer server(80);
// Create aREST instance
aREST rest = aREST();
// Variables to be exposed to the API
byte cs;//For Conveyor speed
byte object;//For Object presence detection
//** Setup function runs only once at the beginning of the program inorder to Setup
// the environment for the program **//
void setup(void)
{
  //Initialization
  Wire.begin(8);//Initialize I2C Slave at Address 8
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);
  //For Indication fo the Incoming and Outgoing Interrupt signal of the Object
  //pinMode(ledPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(relay, OUTPUT);//Setup relay as OUTPUT
  pinMode(7, OUTPUT);//For indication of Completed operation
  pinMode(interruptPin, INPUT_PULLUP);//Setup interruptPin in Active-LOW Configuration
  digitalWrite(relay,LOW);//relay works on Active-HIGH configuration
  attachInterrupt(digitalPinToInterrupt(interruptPin), Sensor_1_Interrupt, CHANGE);
  //Initialization of REST variables
  cs = 0;
  object = 0;
  rest.variable("cs",&cs);
  rest.variable("Object",&object);
  rest.function("C",conveyor_speed);
  rest.function("OA",object_queue_add);
  rest.set_id("001");
  rest.set_name("Nash_Robotics");
  //Ethernet Setup
  Ethernet.begin(mac, ip);
  server.begin();
  //Start Watchdog timer
  wdt_enable(WDTO_4S);
  //Read Conveyor speed from EEPROM
  mode = EEPROM.read(0);
  cs = mode;
}
//**Custom function accessible by the API**//
int conveyor_speed(String command) {
  cs = command.toInt();
  EEPROM.write(0, cs);
  return cs;
}
int object_queue_add(String command) {
  int div_pos = command.toInt();
  queue.enqueue(div_pos);
  return queue.count();
}
void(*reset)(void)=0;//Reset Function

//I2C Functions
//** I2C onRequest from Master Controller **//
void requestEvent() {
  if(c == 5)
  {
    Wire.write(EEPROM.read(0));//Send conveyor speed
  }
  if(c == 6)
  {
    //If Queue is empty
    if(queue.isEmpty())
    {
      Wire.write(255);//Send 255 as empty queue indication to Master controller
      return;
    }
    Wire.write(queue.dequeue());//Send Queue data when requested by Master Controller
  }
}
//** I2C onReceived from Master Controller **//
void receiveEvent(int howMany) {
  while (Wire.available()) {
    c = Wire.read();
  }
  if(c == 5)
  {
    //Send Conveyor speed
     Wire.write(EEPROM.read(0));
  }
}
//** ISR for Sensor_1 - Digital pin 3 **//
void Sensor_1_Interrupt()
{
  state = !state;
  if(state == 1)
  {
    object =1;
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
//** Object counter **//
//Here, We are checking for additional interrupts
void countervalue()
{
  if(counter == 0)
  return;
  else
  {
    counter = 0;

    if(mode == 2)
    {
      //Low Speed - 0.3m/s
      //Timer set to trigger after 1666ms to indicate safe distance
      a = t.after(1666,completed,1);
      //Timer set for every 200ms to check Extra Interrupts for Same Object
      z = t.every(200,check,1);
    }
    if(mode == 3)
    {
      //Medium Speed - 0.6m/s
      a = t.after(833,completed,1);
      z = t.every(200,check,1);
    }
    if(mode == 4)
    {
      //High Speed - 1.2m/s
      a = t.after(500,completed,1);//416
      z = t.every(200,check,1);
    }
  }
}
//** Function to indicate completed operation signal **//
//Here, Completed operation signal refers to the safe distance travelled by the First
//object, Next object can be placed.
//(For example, 50 centimetres distance between two objects)
void completed()
{
  t.oscillate(7,50,LOW,5);//Completed Signal at Digital pin 7
  object = 0;//REST variable Object = 0
  t.stop(z);//Stop the timer instance
}
//** Function to check the Early Object Arrival **//
void check()
{
  object = 0;
  //If Counter value more than zero then Early Object Arrival Exception
  //has occured.Conveyor belt will be turned off.
  if(counter > 0)
  {
    while(!queue.isEmpty())
    {
      Serial.print(queue.dequeue());//Here,also we can catch an exception
    }
    //Turn off the conveyor belt
    digitalWrite(relay,HIGH);
    delay(500);
    digitalWrite(relay,LOW);
    object = 2;//Error Indication REST variable Object = 2
    counter = 0;//Reset the Counter and continue..
    t.stop(z);//Stop the timer instance
    reset();
  }
}
//** Runs continuously after setup() **//
void loop() {
  EthernetClient client = server.available();// listen for incoming clients
  rest.handle(client);//Handle Client's request
  wdt_reset();//Watchdog timer reset
  digitalWrite(ledPin, state);//Interrupt Indication
  t.update();//Update timer
}
