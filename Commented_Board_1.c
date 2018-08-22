// Libraries
#include <SPI.h>
#include <QueueArray.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <Wire.h>
#include <aREST.h>
#include <avr/wdt.h>
#include "Timer.h"
// Global Variables
Timer t;//Timer Object Creation
#define relay 5//Defining Digital pin - 5 as Relay
const byte ledPin = 13;//Initializing Digital pin - 13 as ledPin
const byte interruptPin = 3;//Initializing Digital pin - 3 as Sensor-1 Interrupt
//Initializing state - for Interrupt state change monitoring
volatile byte state = LOW;
volatile byte counter = 0;//counter - for number of objects arriving at Sensor_1
volatile byte mode = 0;// mode - for selecting Conveyor speed
volatile int a = 0,z =0;//operational Variables
// Enter a MAC address for your controller below.
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xFE, 0x40 };
volatile byte pkt = 0;//Incoming packets from I2C bus
// Initialize Object Decision Queue
QueueArray <byte> queue;
//IP Address for the Controller
IPAddress ip(169,254,39,19);
// Ethernet server
EthernetServer server(80);
// Create aREST instance
aREST rest = aREST();
// Variables to be exposed to the API
byte cs;
int obj_available = 0;
// Setup function runs only once at the beginning of the program inorder to Setup
// the environment for the program
void setup(void)
{
  //Start Serial - Debugger
  Serial.begin(115200);
  //Initialize I2C Slave at Address 8
  Wire.begin(8);
  Serial.println("Setting things for you... Wait for the system to start");
  //For Indication fo the Incoming and Outgoing Interrupt signal of the Object
  //pinMode(ledPin, OUTPUT);
  pinMode(relay, OUTPUT);//Setup relay as OUTPUT
  pinMode(7, OUTPUT);//For indication of Completed operation
  //Setup interruptPin in Active-LOW Configuration (below)
  pinMode(interruptPin, INPUT_PULLUP);

  //**Setting Digital pin configurations as per requirement**//

  //relay is set to Digital-HIGH Configuration(below)
  //relay works on Active-LOW configuration
  digitalWrite(relay,HIGH);
  //Setting Interrupt for the Sensor_1 for detecting object's arrival on the Belt
  attachInterrupt(digitalPinToInterrupt(interruptPin), Sensor_1_Interrupt, CHANGE);
  // Initialize variables and expose them to REST API
  rest.variable("cs",&cs);//Rest Variable for Conveyor Speed
  //Rest Variable for Object available in the Conveyor belt.i.e, Incoming Object
  //(below)
  rest.variable("Object",&obj_available);
  //Registering Events to be handled by the I2C Bus
  Wire.onRequest(requestEvent);  //register onRequest event
  Wire.onReceive(receiveEvent); // register onReceive event
  // Function to be exposed via the REST API
  rest.function("C",conveyor_speed);// Conveyor Speed REST API
  //Object Decision to be added by the Server using this REST API (below)
  rest.function("OA",object_queue_add);
  //EXTRAS - STARTING
  // Give name & ID to the device
  rest.set_id("001");
  rest.set_name("Nash_Robotics");
  //EXTRAS - ENDING
  Ethernet.begin(mac, ip);//Initializes the ethernet library and network settings.
  server.begin();//Tells the server to begin listening for incoming connections.
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());//Current IP of the Controller
  // Start watchdog
  wdt_enable(WDTO_4S);
  //Reading Conveyor Speed configuration from EEPROM (below)
  mode = EEPROM.read(0);
  Serial.print("Checking Current Conveyor Speed Configuration in the Controller = ");
  Serial.println(mode);
  cs = mode;//assigning the Conveyor Speed to cs
}
//**Custom function accessible by the API**//
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
//** I2C onRequest from Master Controller **//
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
//** I2C onReceived from Master Controller **//
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

//** Function to send the current Conveyor Speed **//
void send_conveyor_speed()
{
  Wire.write(EEPROM.read(0));
}
//** ISR for Sensor_1 - Digital pin 3 **//
void Sensor_1_Interrupt()
{
  state = !state;
  if(state == 1)
  {
    //Incoming
    //Serial.println("Incoming!");
    //Serial.println(millis());
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
    //Outgoing
    counter++;
    //Serial.println("Outgoing!");
    //Serial.println(millis());
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
    Serial.print("counter = ");
    Serial.println(counter);
    counter = 0;

    if(mode == 2)
    {
      //Low Speed - 0.3m/s
      //Timer set to trigger after 1666ms to indicate safe distance
      a = t.after(1666,completed,1);
      //Timer set for every 200ms to check Extra Interrupts for Same Object
      z = t.every(200,check,1);
      Serial.print("Z = ");
      Serial.println(z);

    }
    else if(mode == 3)
    {
      //Medium Speed - 0.6m/s
      a = t.after(833,completed,1);
      z = t.every(200,check,1);
      Serial.print("Z = ");
      Serial.println(z);
    }
    else if(mode == 4)
    {
      //High Speed - 1.2m/s
      a = t.after(416,completed,1);
      z = t.every(200,check,1);
      Serial.print("Z = ");
      Serial.println(z);
    }
  }
}
//** Function to indicate completed operation signal **//
//Here, Completed operation signal refers to the safe distance travelled by the First
//object, Next object can be placed.
//(For example, 60 centimetres distance between two objects)
void completed()
{
  t.oscillate(7,50,LOW,5);//Completed Signal at Digital pin 7
  obj_available = 0;
  Serial.println("Completed!");
  t.stop(z);//Stop the timer instance
}
//** Function to check the Early Object Arrival **//
void check()
{
  //If Counter value more than zero then Early Object Arrival Exception
  //has occured.Conveyor belt will be turned off.
  if(counter > 0)
  {
    Serial.print("Error Object : CounterValue =");
    Serial.println(counter);
    digitalWrite(relay,LOW);
    delay(500);
    digitalWrite(relay,HIGH);
    Serial.println("Conveyor Turned off");
    counter = 0;//Reset the Counter and continue..
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
