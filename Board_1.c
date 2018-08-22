//Updated with Relay integration on 22nd August,2018
// Libraries
#include <SPI.h>
#include <QueueArray.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <Wire.h>
#include <aREST.h>
#include <avr/wdt.h>
#include "Timer.h"
Timer t;
#define relay 5
const byte ledPin = 13;
const byte interruptPin = 3;
volatile byte state = LOW;
volatile byte counter = 0;
volatile byte mode = 0;
volatile int a = 0,z =0;

// Enter a MAC address for your controller below.
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xFE, 0x40 };
// declare a string message.
byte msg[100];
int x = 0;
byte add = 255;
volatile byte send_pkt = 0;
volatile byte c = 0;
// create a queue of characters.
QueueArray <byte> queue;
// IP address in case DHCP fails
IPAddress ip(169,254,39,19);

// Ethernet server
EthernetServer server(80);

// Create aREST instance
aREST rest = aREST();

// Variables to be exposed to the API
int cs;
int object = 0;

// Declare functions to be exposed to the API
int ledControl(String command);

void setup(void)
{
  // Start Serial
  Serial.begin(115200);
  //I2C
  Wire.begin(8);
  Serial.println("Setup");
  pinMode(ledPin, OUTPUT);
  pinMode(relay, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(interruptPin, INPUT_PULLUP);
  digitalWrite(relay,HIGH);
  attachInterrupt(digitalPinToInterrupt(interruptPin), blink, CHANGE);
  // Init variables and expose them to REST API
  cs = 0;
  object = 0;
  rest.variable("cs",&cs);
  rest.variable("Object",&object);
  Wire.onRequest(requestEvent);  //register event
  Wire.onReceive(receiveEvent); // register event
  // Function to be exposed
  rest.function("C",conveyor_speed);
  rest.function("OA",object_queue_add);
  //rest.function("OR",object_queue_remove);

  // Give name & ID to the device (ID should be 6 characters long)
  rest.set_id("001");
  rest.set_name("Nash_Robotics");
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  // Start watchdog
  wdt_enable(WDTO_4S);
  //Setting Configuration
  mode = EEPROM.read(0);
  //Setting Configuration
  Serial.print("Setting Configuration = ");
  Serial.println(mode);
  cs = mode;
}

void loop() {

  // listen for incoming clients
  EthernetClient client = server.available();
  rest.handle(client);
  wdt_reset();
  digitalWrite(ledPin, state);
  t.update();
}

// Custom function accessible by the API

int conveyor_speed(String command) {

  // Get state from command
  int state = command.toInt();
  cs = state;
  EEPROM.write(0, cs);
  Serial.print("Speed:");
  Serial.println(cs);
  return 1;

}
int object_queue_add(String command) {

  // Get state from command
  int state = command.toInt();
  queue.enqueue(state);
  Serial.print(state);
  Serial.println("\tADD");
  return 1;

}
void requestEvent() {

  if(c == 5)
  {
    send_speed();
    Serial.print("Speed");
  }
  if(c==6)
  {
    if(queue.isEmpty())
    {
      Wire.write(255);
      Serial.print("No Data");
      Serial.println(queue.count());
      return;
    }
    add = queue.dequeue();
    Wire.write(add);
    Serial.println("Sent");
  }
}
void receiveEvent(int howMany) {
  while (Wire.available()) {
    c = Wire.read();
    Serial.print(c);
  }
  if(c == 5)
  {
      send_speed();
  }
  if(c == 6)
  {
    Serial.print("Received");
  }
}
void send_speed()
{
  send_pkt = EEPROM.read(0);
  Wire.write(send_pkt);
}
void blink()
{
  state = !state;
  if(state == 1)
  {
    //Incoming
    //Serial.println("Incoming!");
    //Serial.println(millis());
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
    //Outgoing
    counter++;
    //Serial.println("Outgoing!");
    //Serial.println(millis());
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
      a = t.after(1666,doit,1);
      z = t.every(200,check,1);
      Serial.print("Z = ");
      Serial.println(z);

    }
    else if(mode == 3)
    {
      a = t.after(833,doit,1);
      z = t.every(200,check,1);
      Serial.print("Z = ");
      Serial.println(z);
    }
    else if(mode == 4)
    {
      a = t.after(416,doit,1);
      z = t.every(200,check,1);
      Serial.print("Z = ");
      Serial.println(z);
    }
  }
  //counter = 0;
}
void doit()
{
  t.oscillate(7,50,LOW,5);
  object = 0;
  Serial.println("Completed!");
  t.stop(z);
}

void check()
{
  //counter;
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
