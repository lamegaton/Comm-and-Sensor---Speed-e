/********************************
 * sleep & alarm for clock
 ********************************/
//scl       A4
//sda       A5
//interrupt D3
#include <Wire.h>
//#include "RTClib.h" replaced by rtc
#include <RTClibExtended.h>
#include <LowPower.h>

#define wakePin 3
#define ledPin 13

byte wkHour = 15;
byte wkMinute = 3;
byte slpMinute = 0;
byte AlarmFlag = 0;
byte ledStatus = 1;

RTC_DS3231 rtc;
/********************************
 * ENABLE CODE
 ********************************/
//PIN FOR SENSOR
#define TRIGGER 4 //orange 
#define ECHO 5 //yellow

//ENABLE FUNCTIONS
#define ABP = 1
#define OTAA = 0
#define SENSOR_ONLY 0 // 1 if only want to run sensor code

#if SENSOR_ONLY == 0
//Cayenne
#include <CayenneLPP.h>
//Lib for lora
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
//#include <node_config.h>
#endif

/********************************
 * DEFINE VARIABLES FOR SENSOR
 * ******************************/
byte i=0;
byte offset = 2;
long duration;
long distance,temp;
long distanceThreshold = 10; // max is 14 feets = 12*14
int vehicleCount = 0;
int Vehicle_Detected_Confidence_Level = 0;

/********************************
 * FOR NUMBER #2 COMM-SETTING
 * *****************************/
#if SENSOR_ONLY == 0
  
/********************************
 * Setup for Cayenne
 * Read more: https://www.thethingsnetwork.org/docs/devices/arduino/api/cayennelpp.html
 * 
 ********************************/
  CayenneLPP lpp(10);//51 bytes maximum payload
  static const PROGMEM u1_t NWKSKEY[16] ={ 0x17, 0xE5, 0x84, 0xC8, 0xC9, 0x65, 0x81, 0x5E, 0xEA, 0xDE, 0x49, 0xEB, 0x38, 0x71, 0xD4, 0x0F };
  static const u1_t PROGMEM APPSKEY[16] ={ 0x56, 0x03, 0x76, 0x0D, 0xA3, 0x47, 0xE3, 0xA4, 0xED, 0x03, 0xD7, 0x24, 0x55, 0x64, 0xF4, 0xE6 };
  static const u4_t DEVADDR = 0x26021CD7 ; 
  
  // These callbacks are only used in over-the-air activation
  void os_getArtEui (u1_t* buf) { }
  void os_getDevEui (u1_t* buf) { }
  void os_getDevKey (u1_t* buf) { }
  
  static uint8_t mydata[2];
  
  static osjob_t sendjob;
  
  // Schedule TX every this many seconds
  const unsigned TX_INTERVAL = 30;
  
  // Pin mapping for arduino nano
  const lmic_pinmap lmic_pins = {
      .nss = 10,//cs
      .rxtx = LMIC_UNUSED_PIN,
      .rst = 2,
      .dio = {6,7,LMIC_UNUSED_PIN},
  };

//NUMBER 1 COMM FUNCTION
void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            // os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            break;
        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}
/********************************
 * LOOP FOR LORA / TX
 * *****************************/
 
void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        /*
        //use without cayenne
        mydata[0]= vehicleCount;
        LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
        */
        LMIC_setTxData2(1, lpp.getBuffer(), lpp.getSize(), 0);
        vehicleCount = 0;
        Serial.println(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}
void send_it() {
  Serial.println(F(" Sending.. "));  
  do_send(&sendjob);//prepare data package
  while ((LMIC.opmode & OP_JOINING) || (LMIC.opmode & OP_TXRXPEND)){
    os_runloop_once();  
  }
  Serial.println(F(" TX_COMPLETE"));
}

void iniLora(){
    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();
    // Set static session parameters. Instead of dynamically establishing a session
    // by joining the network, precomputed session parameters are be provided.
    #ifdef PROGMEM
    // On AVR, these values are stored in flash and only copied to RAM
    // once. Copy them to a temporary buffer here, LMIC_setSession will
    // copy them into a buffer of its own again.
    uint8_t appskey[sizeof(APPSKEY)];
    uint8_t nwkskey[sizeof(NWKSKEY)];
    memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
    memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
    LMIC_setSession (0x13, DEVADDR, nwkskey, appskey);//can be 0x1
    #else
    // If not running an AVR with PROGMEM, just use the arrays directly
    LMIC_setSession (0x13, DEVADDR, NWKSKEY, APPSKEY);
    #endif

    #if defined(CFG_us915)
    // First disable all sub-bands
    for (int b = 0; b < 8; ++b) {
      LMIC_disableSubBand(b);
    }
    // Then enable the channel(s) you want to use
    LMIC_enableChannel(8); // 903.9 MHz
    //LMIC_selectSubBand(1);
    #endif
    // Disable link check validation
    LMIC_setLinkCheckMode(0);

    // TTN uses SF9 for its RX2 window.
    LMIC.dn2Dr = DR_SF9;

    // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
    LMIC_setDrTxpow(DR_SF10,14);//DR_SF7 as default

    // Start job
    do_send(&sendjob);
  }
#endif
/********************************
 * WAKE-UP FUNCTION
 ********************************/
void initAlarm() {
  //Set pin D2 as INPUT for accepting the interrupt signal from DS3231
  pinMode(wakePin, INPUT);

  //switch-on the on-board led for 1 second for indicating that the sketch is ok and running
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  delay(1000);

  //Initialize communication with the clock
  Wire.begin();
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  //rtc.adjust(DateTime(__DATE__, __TIME__));   //set rtc date and time to COMPILE time
  }else{Serial.println(F("Starting"));}
  //clear any pending alarms
  rtc.armAlarm(1, false);
  rtc.clearAlarm(1);
  rtc.alarmInterrupt(1, false);
  rtc.armAlarm(2, false);
  rtc.clearAlarm(2);
  rtc.alarmInterrupt(2, false);

  //Set SQW pin to OFF (in my case it was set by default to 1Hz)
  //The output of the DS3231 INT pin is connected to this pin
  //It must be connected to arduino D2 pin for wake-up
  rtc.writeSqwPinMode(DS3231_OFF);

  //Set alarm1 every day at 18:33
  rtc.setAlarm(ALM1_MATCH_HOURS, wkMinute, wkHour, 0);   //set your wake-up time here (A,mm,hh,ss)
  rtc.alarmInterrupt(1, true);
}
void wakeUp()        // here the interrupt is handled after wakeup
{
}

void sleepBabe(){
    if (AlarmFlag == 0) {
    attachInterrupt(digitalPinToInterrupt(wakePin), wakeUp, LOW);//use interrupt 0 (pin 2) and run function wakeUp when pin 2 gets LOW 
    digitalWrite(ledPin, LOW);                             //switch-off the led for indicating that we enter the sleep mode
    ledStatus = 0;                                         //set the led status accordingly
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);   //arduino enters sleep mode here
    detachInterrupt(digitalPinToInterrupt(wakePin));                            //execution resumes from here after wake-up

    // Exiting sleep mode and clear alarm
    rtc.armAlarm(1, false);
    rtc.clearAlarm(1);
    rtc.alarmInterrupt(1, false);
    AlarmFlag++;
    }
}

/********************************
 * READ SENSOR FUNCTION
 ********************************/
void readSensor(){
  temp = distance;
  digitalWrite(TRIGGER, LOW); // Setting Trigger Pin Low for 2 microseconds
  delayMicroseconds(5);

  digitalWrite(TRIGGER, HIGH);  // Setting Trigger Pin High for 10 microseconds to trigger ranging
  delayMicroseconds(20);

  digitalWrite(TRIGGER, LOW); // Setting Trigger Pin Low
  
  duration = pulseIn(ECHO, HIGH);
  distance = duration * 0.00676;
  if (distance > distanceThreshold) distance = distanceThreshold;
  //if (temp == distance - offset || temp == distance + offset) distance = temp;
  //Serial.println(distance);
}

/********************************
 * COUNT VEHICLE FUNCTION
 ********************************/
void countCar(){
  readSensor();
  while (distance < distanceThreshold)
  {
    Vehicle_Detected_Confidence_Level ++;
    readSensor();
    if (distance >= distanceThreshold){
      delay(5);// to exclude the distance from noise
      readSensor();
      if (distance >= distanceThreshold){
        if (Vehicle_Detected_Confidence_Level >= 5) {
        vehicleCount ++;
        Serial.print("Vehicle Count ");
        Serial.print(vehicleCount);    
        Serial.print(" Confidence level: "); 
        Serial.println(Vehicle_Detected_Confidence_Level);
        }
      }
      Vehicle_Detected_Confidence_Level = 0;
      break;
    }
  }
  #if SENSOR_ONLY == 0
  lpp.reset();
  lpp.addPresence(1,vehicleCount);  
  #endif
  }

void setup() {
    pinMode(TRIGGER, OUTPUT);
    pinMode(ECHO, INPUT);
    Serial.begin(115200);
    delay(1000);
    DateTime now = rtc.now();
    #if SENSOR_ONLY == 0
    iniLora();
    #endif
    initAlarm();
}

void loop() {
    countCar();
    DateTime now = rtc.now();
    if(now.hour() >= 6 && now.hour() <= 20){
      if(now.second()% 30 == 0){
        send_it();
        Serial.println("transmitting at 30 sec!");
      }
    }
    if(now.minute() == slpMinute){
      Serial.println("Entering Sleep mode.. !!!");
      Serial.println(now.hour());
      sleepBabe();
    }
}
