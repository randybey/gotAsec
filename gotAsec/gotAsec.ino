#include <Timezone.h>
#include <Time.h>
#include <TimeLib.h>

// bluefruit adds
#include <Arduino.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
#include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"
#include "BluefruitConfig.h"

#define FACTORYRESET_ENABLE         1
#define MINIMUM_FIRMWARE_VERSION    "0.6.6"
#define MODE_LED_BEHAVIOUR          "MODE"
/*=========================================================================*/
// Create the bluefruit object
SoftwareSerial bluefruitSS = SoftwareSerial(BLUEFRUIT_SWUART_TXD_PIN, BLUEFRUIT_SWUART_RXD_PIN);
Adafruit_BluefruitLE_UART ble(bluefruitSS, BLUEFRUIT_UART_MODE_PIN,
                              BLUEFRUIT_UART_CTS_PIN, BLUEFRUIT_UART_RTS_PIN);
// Set up time function definitions
#define TIME_MSG_LEN 11 // time sync to PC is HEADER followed by Unix time_t as ten ASCII digits
#define TIME_HEADER 'T' // Header tag for serial time sync message
#define TIME_REQUEST 7 // ASCII bell character requests a time sync message 

// all my definitions
int sensePin = 0;
int ledPin = 6;
String output;
int val = 0;
time_t startTime;
time_t endTime;
time_t central;
time_t utc;
long duration;
// for pretty print later
String months[13] = {"Festivus", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

//Set up the timezone functions
TimeChangeRule usCDT = {"CDT", Second, Sun, Mar, 2, -300};  //UTC - 5 hours
TimeChangeRule usCST = {"CST", First, Sun, Nov, 2, -360};   //UTC - 6 hours
Timezone usCentral(usCDT, usCST);

void setup() {
  while (!Serial);  // required for Flora & Micro
  delay(500);
  Serial.begin(9600);
  Serial.println();
  Serial.println("Please enter ctime in form T123467890");
  // wait long enough for dumb human to enter time then process same.
  delay(10000);
  processSyncMessage();
  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));
  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );
  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ) {
      // try twice before giving up
      if ( ! ble.factoryReset() ) {
        error(F("Couldn't factory reset"));
      }
    }
  }
  /* Disable command echo from Bluefruit */
  ble.echo(false);
  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();
  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in UART mode"));
  Serial.println(F("Then Enter characters to send to Bluefruit"));
  Serial.println();
  ble.verbose(false);  // debug info is a little annoying after this point!
  /* Wait for connection */
  while (! ble.isConnected()) {
    delay(500);
  }
  Serial.println(F("******************************"));
  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    // Change Mode LED Activity
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }
  // check the size of the FIFOs for RX and TX -- should show 1024 bytes?
  ble.sendCommandCheckOK("AT+BLEUARTFIFO");
  // Set module to DATA mode
  Serial.println( F("Switching to DATA mode!") );
  ble.setMode(BLUEFRUIT_MODE_DATA);
  Serial.println(F("******************************"));
  analogReference(DEFAULT); //default
  pinMode(ledPin, OUTPUT);
  // good morning, sunshine
  Serial.println("Hello Dave, how are you this morning?");
}

void loop() {
  // we want to output a 1 if "someone is there" and a 0 if "no one is there"
  // or, it's light = no one there, it's dark = someone is there
  val = check_it();
  delay(1000);
  if ( val == 0 && val == check_it()) {
    //cha-ching, they're still there
    // turn the LED on to signify a visitor present
    digitalWrite(ledPin, HIGH);
    //preserve the time this happened
    Serial.print("Someone came...");
    startTime = now();
    startTime = usCentral.toLocal(startTime);
    // wait for them to go away
    while ( check_it() == 0) {
      delay(1000);
    }
    // went dark, they left.
    digitalWrite(ledPin, LOW);
    Serial.println("...They left");
    //preserve the time
    endTime = now();
    endTime = usCentral.toLocal(endTime);
    duration = endTime - startTime;
	
	//BLE apparently has a 20 byte cap on a payload -- which is not being reassembled properly at the bluefruit app
	// output: MMM DD HH:MM XX MINS  ..... all I can squeeze in there.
	//         12345678901234567890
    output = months[month(startTime)] + " " + String(day(startTime)) + " " + \
             String(hour(startTime)) + ":" + String(minute(startTime)) + " " + String(duration) + " mins\n";
    Serial.println(output);
    ble.print(output);
    delay(500);
  }
}

void processSyncMessage() {
  // if time sync available from serial port, update time and return true
  while (Serial.available() >= TIME_MSG_LEN ) { // time message consists of header & 10 ASCII digits
    char c = Serial.read() ;
    // Serial.print(c);
    if ( c == TIME_HEADER ) {
      time_t pctime = 0;
      for (int i = 0; i < TIME_MSG_LEN - 1; i++) {
        c = Serial.read();
        if ( c >= '0' && c <= '9') {
          pctime = (10 * pctime) + (c - '0') ; // convert digits to a number
        }
      }
      setTime(pctime); // Sync Arduino clock to the time received on the serial port
    }
  }
}

int check_it()
{
  val = analogRead(sensePin);
  val = map(val, 500, 700, 0, 1);
  //Serial.println(val);
  return val;
}

// I'd like you to meet my little friend
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}
