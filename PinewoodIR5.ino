/* ***************************** Pinewood.INO *************************************************

This project runs a Cub Car (Pinewood Derby) race using an Arduino NANO.
The system supports either a 3-lane or 4-lane track. The number of lanes is selected
at compile time:

    #define LANE_COUNT 3     // Set to 3 or 4

Race results are displayed using:
  • A bank of LEDs (winner indication)
  • An I2C 4x20 LCD (times, instructions, status)

Two shift registers are used to control up to 16 LEDs, reducing the total number
of required Arduino pins. Each LED is current limited with a 330Ω resistor.

-----------------------------------------------------------------------------------------------
SENSORS – IR BREAK-BEAM (ANALOG)
-----------------------------------------------------------------------------------------------

Each lane uses a discrete infrared (IR) LED emitter and a discrete IR receiver
positioned across the track to create a break-beam sensor.
The receiver signal is read using Arduino ANALOG inputs, to allow for variability in signal strength

At startup a calibration routine runs which:

1. Displays live analog IR signal strength bars for each lane.
2. Captures baseline readings (beam unblocked).
3. Captures blocked readings (car interrupting beam).
4. Automatically determines signal polarity and sets a trigger threshold.

This makes the system tolerant of sensor variation and ambient IR differences.

-----------------------------------------------------------------------------------------------
IR SENSOR WIRING (DISCRETE COMPONENTS – ANALOG ONLY)
-----------------------------------------------------------------------------------------------

Each lane wiring:

IR EMITTER (IR LED)
  Arduino Digital Pin  ->  300Ω resistor  ->  IR LED ANODE (Long Leg)
  IR LED CATHODE (Short Leg / Flat Side) ->  GND

Emitters are ACTIVE HIGH:
  digitalWrite(pin, HIGH) turns the IR beam ON.

IR RECEIVER
  3V3  ->  IR Receiver ANODE (Long Leg)
  IR Receiver CATHODE (Short Leg / Flat Side)
           -> Arduino Analog Input (A0–A3)
           -> 10kΩ resistor -> GND  (external pulldown)

The analog input measures beam intensity.
During calibration, the system determines whether the signal increases or decreases
when the beam is blocked and adjusts automatically.

-----------------------------------------------------------------------------------------------
LANE PIN ASSIGNMENTS (Arduino Nano)
-----------------------------------------------------------------------------------------------

D0 (RX)     -- DO NOT USE (required for uploading)
D1 (TX)     -- DO NOT USE (required for uploading)
D2 (INT0)   -- Timer Button
D3 (INT1)   -- Start Button

D4          -- Lane 1 IR Emitter
D5          -- Lane 2 IR Emitter
D6          -- OPEN
D7          -- Lane 4 IR Emitter (only if LANE_COUNT == 4)
D8          -- Shift Register Latch (Pin 12)
D9          -- Lane 3 IR Emitter
D10 (SS)    -- OPEN
D11 (MOSI)  -- Shift Register Data (Pin 14)
D12 (MISO)  -- Shift Register Clock (Pin 11)
D13 (SCK)   -- OPEN

A0 (D14)    -- Lane 1 IR Receiver (Analog)
A1 (D15)    -- Lane 3 IR Receiver (Analog)
A2 (D16)    -- Lane 2 IR Receiver (Analog)
A3 (D17)    -- Lane 4 IR Receiver (Analog, if enabled)

A4 (SDA)    -- I2C SDA (LCD)
A5 (SCL)    -- I2C SCL (LCD)
A6          -- OPEN
A7          -- OPEN

-----------------------------------------------------------------------------------------------
LANE CONNECTION SUMMARY
-----------------------------------------------------------------------------------------------

Lane 1:
  Emitter D4  -> Receiver A0

Lane 2:
  Emitter D5  -> Receiver A2

Lane 3:
  Emitter D9  -> Receiver A1

Lane 4 (if LANE_COUNT == 4):
  Emitter D7  -> Receiver A3

-----------------------------------------------------------------------------------------------
POWER NOTES
-----------------------------------------------------------------------------------------------

The shift registers, LED banks, LCD, and IR emitters draw significant current.
A laptop USB port may not supply sufficient power.

Recommended:
  • 9V power supply
  • Buck converter regulated to 5V for stable operation

-----------------------------------------------------------------------------------------------
DEBUG MODE
-----------------------------------------------------------------------------------------------

A verbose debugging mode is included to output calibration values,
sensor readings, and race diagnostics to the Serial Monitor.

-----------------------------------------------------------------------------------------------

Author - Jeff Wilts
Version 2.0 -- IR Analog Break-Beam Conversion & Compile-Time Lane Selection  March 2026

************************************************************************************************ */

// #define DEBUG 1                                     // Toggle between debug and run mode by commenting & uncommenting these two lines 
#define DEBUG 0

const int NOSWITCH = 0;                                 // Define if start switch in NO - Normally Open (1) or NC - Normally Closed (0)

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

#include <LiquidCrystal_I2C.h>                      // lcd display library
#include <Wire.h>                                   // i2c libary for lcd display

//*********************Display Stuff  ***********************************************
LiquidCrystal_I2C lcd(0x27,20,4);                   // set the LCD address to 0x27 for a 20 chars and 4 line display

//*********************Shift Register Stuff *****************************************
const int latchPin = 8;
const int clockPin = 12;
const int dataPin = 11;

int numOfRegisters = 2;                           // number of shift registers
byte* registerState;

long effectSpeed = 130;  

//************************Lane Setup Stuff ***************************************
// ---------------- Track configuration ----------------
// Choose number of lanes at COMPILE TIME: set to 3 or 4.
#ifndef LANE_COUNT
  #define LANE_COUNT 3
#endif
#if (LANE_COUNT != 3) && (LANE_COUNT != 4)
  #error "LANE_COUNT must be 3 or 4"
#endif

const int laneCount = LANE_COUNT;                    // number of lanes on the track

int LEDarray[] = { 5, 6, 7, 9, 10, 11, 13, 14, 15 };  // array of LEDS if 3 lanes
int LEDarray4[] = { 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 13, 14, 15, 16 };  // array of LEDS if 4 lanes
const int raceTimeout = 15;                         // How long before race times out in secondes
const uint8_t IR_EMIT1 = 4;
const uint8_t IR_EMIT2 = 5;
const uint8_t IR_EMIT3 = 9;

const uint8_t IR_RECV1 = A0;
const uint8_t IR_RECV2 = A2;
const uint8_t IR_RECV3 = A1;

#if (LANE_COUNT == 4)
const uint8_t IR_EMIT4 = 7;
const uint8_t IR_RECV4 = A3;
#endif

// Per-lane polarity: if true, a car BREAKING the beam drives the raw reading LOWER than baseline.
bool irBlockedIsLow1 = true;
bool irBlockedIsLow2 = true;
bool irBlockedIsLow3 = true;
bool irBlockedIsLow4 = true;

const int TimerButton = 2;                          // the number of the pushbutton pin
int TimerButtonState;                               // the current reading from the input pin

const int StartButton =  3;                         // The track starter button
int StartButtonState;                               // the current reading from the input pin

const uint8_t DebounceDelay = 50;                   // Adjust this value as needed

int input_val1 = 0;                                 // Starting state for LDR1
int input_val2 = 0;                                 // Starting state for LDR2
int input_val3 = 0;                                 // Starting state for LDR3
int input_val4 = 0;                                 // Starting state for LDR4

int thresh1 = 225;                                  // default trigger threshold for lane #1
int thresh2 = 225;                                  // default trigger threshold for lane #2
int thresh3 = 225;                                  // default trigger threshold for lane #3
int thresh4 = 225;                                  // default trigger threshold for lane #3

static inline bool irLaneBlocked(uint8_t lane, int raw) {
  // lane is 1-based
  switch (lane) {
    case 1: return irBlockedIsLow1 ? (raw <= thresh1) : (raw >= thresh1);
    case 2: return irBlockedIsLow2 ? (raw <= thresh2) : (raw >= thresh2);
    case 3: return irBlockedIsLow3 ? (raw <= thresh3) : (raw >= thresh3);
    case 4: return irBlockedIsLow4 ? (raw <= thresh4) : (raw >= thresh4);
    default: return false;
  }
}


//************************************ SETUP ***********************************

void setup()
{  
  pinMode(TimerButton, INPUT);                      // initialize the pushbutton pin as an input with External PULLDOWN resistor:
  pinMode(StartButton, INPUT);                      // initialize the start button pin as an input with External PULLDOWN resistor:
    
// Turn ON IR emitters (active HIGH) and configure receiver pins.
pinMode(IR_EMIT1, OUTPUT); digitalWrite(IR_EMIT1, HIGH);
pinMode(IR_EMIT2, OUTPUT); digitalWrite(IR_EMIT2, HIGH);
pinMode(IR_EMIT3, OUTPUT); digitalWrite(IR_EMIT3, HIGH);
#if (LANE_COUNT == 4)
pinMode(IR_EMIT4, OUTPUT); digitalWrite(IR_EMIT4, HIGH);
#endif

pinMode(IR_RECV1, INPUT);
pinMode(IR_RECV2, INPUT);
pinMode(IR_RECV3, INPUT);
#if (LANE_COUNT == 4)
pinMode(IR_RECV4, INPUT);
#endif

  Serial.begin(9600);
  //*********************Shift Register Stuff *****************************************
  //Initialize array
  registerState = new byte[numOfRegisters];
  for (size_t i = 0; i < numOfRegisters; i++)     // blank out all the registers
  {
    registerState[i] = 0;
  }                                               
  //set pins to output so you can control the shift register
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  
  //************************* Setup i2c Screen*****************************
  Wire.begin();                                    //start i2c address
  lcd.init();
  lcd.home();                       
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.clear();
  lcd.print(F("Startup Complete!"));
   //*************************** End i2c Screen******************************
  
  debugln(F("Splashing Color"));
  effectC(effectSpeed);                         // splash a little colour on LEDs
  debugln(F("End Splashing Color"));
  BlankAll();

  tuneLDR();                                    // run start routine
  
} // end Setup
/* ******************************** Main Loop ************************ */

void loop()
{
  int finishercount = 0;                        // reset count of finishers to 0
  float startTime = 0 ;                         // Start timer
  float Lane1Timer = 0 ;                        // time for lane 1
  float Lane2Timer = 0 ;                        // time for lane 2
  float Lane3Timer = 0 ;                        // time for lane 3
  float Lane4Timer = 0 ;                        // time for lane 4
    
  BlankAll();                                   // Make sure all the LEDS are off
  debugln(F("Close Start Gate"));
  lcd.clear();
  lcd.print(F("Close Start Gate"));  

  if (NOSWITCH){                                // check for normally open switch
    while(!PushedStart()){}                     // Wait for the track start button to be pushed
  } else {                                      // if normally closed switch
    while(!ReleasedStart()){}
  } // endif check for Normally Open Switch
    
  // effectC(effectSpeed/2);                         // splash a little colour on LEDs
  debugln(F("Load Cars"));
  lcd.clear();
  lcd.print(F("Load Cars"));  
    
  effectC(effectSpeed/2);                         // splash a little colour on LEDs
                                                  // wait for start gate to get triggered
  if (NOSWITCH){                                  // check for normally open switch
    while(!ReleasedStart()){}
  } else {                                      // if normally closed switch
    while(!PushedStart()){}                     // Wait for the track start button to be pushed
  } // endif check for Normally Open Switch

  startTime = millis();                           // capture start time
  
  debugln(F("Raceers Away"));
  lcd.setCursor(0,1);
  lcd.clear();
  
  while(1) {
    input_val1 = analogRead(IR_RECV1);
input_val2 = analogRead(IR_RECV2);
input_val3 = analogRead(IR_RECV3);
#if (LANE_COUNT == 4)
    input_val4 = analogRead(IR_RECV4);
#else
    input_val4 = 0;
#endif
    
    if (irLaneBlocked(1, input_val1) && (Lane1Timer == 0 )) 
    {
      finishercount = finishercount+1; 
      debugln(F("Trigger1"));
      Lane1Timer = (millis() - startTime)/1000;
      BankPlaceLight(1,finishercount,laneCount);
      debugln(Lane1Timer);
      lcd.setCursor(0,0);
      lcd.print("Lane 1: " + String(Lane1Timer));
      if (finishercount == 1) 
      {
        lcd.print(F(" WINNER"));
      } // endif WINNER
    }
    if (irLaneBlocked(2, input_val2) && (Lane2Timer == 0 ))
    {
      finishercount = finishercount+1; 
      debugln(F("Trigger2"));
      Lane2Timer = (millis() - startTime)/1000;
      BankPlaceLight(2,finishercount,laneCount);
      debugln(Lane2Timer);
      lcd.setCursor(0,1);
      lcd.print("Lane 2: " + String(Lane2Timer));
      if (finishercount == 1) 
      {
        lcd.print(F(" WINNER"));
      } // endif WINNER

    }
    if (irLaneBlocked(3, input_val3) && (Lane3Timer == 0 ))
    {
      finishercount = finishercount+1; 
      debugln(F("Trigger3"));
      Lane3Timer = (millis() - startTime)/1000;
      BankPlaceLight(3,finishercount,laneCount);
      debugln(Lane3Timer);
      lcd.setCursor(0,2);
      lcd.print("Lane 3: " + String(Lane3Timer));
      if (finishercount == 1) 
      {
        lcd.print(F( " WINNER"));
      } // endif WINNER
    } // end if threshold check
    if (laneCount ==4)
    {
      if (irLaneBlocked(4, input_val4) && (Lane4Timer == 0 ))
      {
        finishercount = finishercount+1; 
        debugln(F("Trigger4"));
        Lane4Timer = (millis() - startTime)/1000;
        BankPlaceLight(4,finishercount,laneCount);
        debugln(Lane4Timer);
        lcd.setCursor(0,3);
        lcd.print("Lane 4: " + String(Lane4Timer));
        if (finishercount == 1) 
        {
          lcd.print(F(" WINNER"));
        } // endif WINNER
      }  // endif threshold check
    } // Endif laneCount == 4
       
    if (finishercount >= laneCount) { break; }      // End the Race

    //  Check race that ran too long
    if ((millis() - startTime) > (raceTimeout*1000) ) { 
      debugln(F("Timed Out"));
      debugln( finishercount );
      lcd.setCursor(5,3);
      lcd.print(F("TIMED OUT"));
      break; } // End the Race
    
  } // End While
  
  debugln(F("Epic Race - Glad its over, Push Timer Button"));
  // lcd.clear();
  // lcd.print(F("Clear Lanes, "));
  // lcd.setCursor(0,1);
  // lcd.print(F("Push Timer Button"));

  while(!ReleasedTimer()){}         // Wait for the button on the timer to be pressed & released
} // end Loop

/* ******************************************************** */
void tuneLDR()
{
  // NOTE: Kept function name tuneLDR() to minimize changes elsewhere in Program.
  // This routine now calibrates IR lightbeams instead of LDRs.

  debugln(F("Starting IR calibration"));
  delay(250);

  int brightval1 = 0;       // baseline (no car blocking)
  int brightval2 = 0;
  int brightval3 = 0;
  int brightval4 = 0;       // unused unless you wire lane 4

  int blockedval1 = 0;      // car blocking the beam
  int blockedval2 = 0;
  int blockedval3 = 0;
  int blockedval4 = 0;      // unused unless you wire lane 4

  // -------------------------
  // Start screen replacement:
  // Live strength bars for each IR receiver until Timer Button press+release.
  // -------------------------
  debugln(F("Show live IR strength, then press Timer Button"));
  BankLight(1, laneCount);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("IR SENSOR STRENGTH"));

  unsigned long lastUI = 0;
  while (!ReleasedTimer()) {
    if (millis() - lastUI >= 60) {
      lastUI = millis();

#if (LANE_COUNT == 4)
      // Compact 4-lane display (2 lanes per row)
      int v1 = analogRead(IR_RECV1);
      int v2 = analogRead(IR_RECV2);
      int v3 = analogRead(IR_RECV3);
      int v4 = analogRead(IR_RECV4);

      // Bars are 0..7 columns wide (to fit 2 per row)
      int b1 = map(v1, 0, 1023, 0, 7);
      int b2 = map(v2, 0, 1023, 0, 7);
      int b3 = map(v3, 0, 1023, 0, 7);
      int b4 = map(v4, 0, 1023, 0, 7);

      if (b1 < 0) b1 = 0; if (b1 > 7) b1 = 7;
      if (b2 < 0) b2 = 0; if (b2 > 7) b2 = 7;
      if (b3 < 0) b3 = 0; if (b3 > 7) b3 = 7;
      if (b4 < 0) b4 = 0; if (b4 > 7) b4 = 7;

      // Row 1: lanes 1 & 2
      lcd.setCursor(0, 1);
      lcd.print(F("1"));
      for (int c = 0; c < 7; c++) lcd.print(c < b1 ? (char)0xFF : ' ');
      lcd.print(F(" 2"));
      for (int c = 0; c < 7; c++) lcd.print(c < b2 ? (char)0xFF : ' ');

      // Row 2: lanes 3 & 4
      lcd.setCursor(0, 2);
      lcd.print(F("3"));
      for (int c = 0; c < 7; c++) lcd.print(c < b3 ? (char)0xFF : ' ');
      lcd.print(F(" 4"));
      for (int c = 0; c < 7; c++) lcd.print(c < b4 ? (char)0xFF : ' ');

      // Instruction
      lcd.setCursor(0, 3);
      lcd.print(F("Push Timer Button   "));
#else
      // 3-lane display (full-width bars)
      int v1 = analogRead(IR_RECV1);
      int v2 = analogRead(IR_RECV2);
      int v3 = analogRead(IR_RECV3);

      // Bars are 0..17 columns wide
      int b1 = map(v1, 0, 1023, 0, 17);
      int b2 = map(v2, 0, 1023, 0, 17);
      int b3 = map(v3, 0, 1023, 0, 17);

      if (b1 < 0) b1 = 0; if (b1 > 17) b1 = 17;
      if (b2 < 0) b2 = 0; if (b2 > 17) b2 = 17;
      if (b3 < 0) b3 = 0; if (b3 > 17) b3 = 17;

      lcd.setCursor(0, 1);
      lcd.print(F("L1 "));
      for (int c = 0; c < 17; c++) lcd.print(c < b1 ? (char)0xFF : ' ');

      lcd.setCursor(0, 2);
      lcd.print(F("L2 "));
      for (int c = 0; c < 17; c++) lcd.print(c < b2 ? (char)0xFF : ' ');

      lcd.setCursor(0, 3);
      lcd.print(F("L3 "));
      for (int c = 0; c < 17; c++) lcd.print(c < b3 ? (char)0xFF : ' ');
#endif
    }
  }

  // Capture baseline (no cars blocking)
  brightval1 = analogRead(IR_RECV1);
  brightval2 = analogRead(IR_RECV2);
  brightval3 = analogRead(IR_RECV3);
#if (LANE_COUNT == 4)
  brightval4 = analogRead(IR_RECV4);
#else
  brightval4 = 0;
#endif

  debugln(F("Got IR baseline"));
  debug(brightval1); debug(F(" ")); debug(brightval2); debug(F(" ")); debug(brightval3);
#if (LANE_COUNT == 4)
  debug(F(" ")); debugln(brightval4);
#else
  debugln("");
#endif

  // -------------------------
  // Phase 2: cars blocking
  // -------------------------
  debugln(F("Put cars on beams, press Timer"));
  delay(250); // small settle

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("CARS ON TRACK"));
  lcd.setCursor(0, 1);
  lcd.print(F("BLOCK BEAMS"));
  lcd.setCursor(0, 3);
  lcd.print(F("Push Timer Button"));
  BankLight(2, laneCount);

  while (!ReleasedTimer()) {}

  blockedval1 = analogRead(IR_RECV1);
  blockedval2 = analogRead(IR_RECV2);
  blockedval3 = analogRead(IR_RECV3);
#if (LANE_COUNT == 4)
  blockedval4 = analogRead(IR_RECV4);
#else
  blockedval4 = 0;
#endif

  debugln(F("Got IR blocked"));
  debug(blockedval1); debug(F(" ")); debug(blockedval2); debug(F(" ")); debug(blockedval3);
#if (LANE_COUNT == 4)
  debug(F(" ")); debugln(blockedval4);
#else
  debugln("");
#endif

  // Determine polarity + threshold per lane (halfway between baseline and blocked)
  irBlockedIsLow1 = (blockedval1 < brightval1);
  irBlockedIsLow2 = (blockedval2 < brightval2);
  irBlockedIsLow3 = (blockedval3 < brightval3);
#if (LANE_COUNT == 4)
  irBlockedIsLow4 = (blockedval4 < brightval4);
#else
  irBlockedIsLow4 = true;
#endif

  thresh1 = (brightval1 + blockedval1) / 2;
  thresh2 = (brightval2 + blockedval2) / 2;
  thresh3 = (brightval3 + blockedval3) / 2;
#if (LANE_COUNT == 4)
  thresh4 = (brightval4 + blockedval4) / 2;
#else
  thresh4 = 225;
#endif

  debugln(F("IR thresholds"));
  debug(thresh1); debug(F(" ")); debug(thresh2); debug(F(" ")); debugln(thresh3);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("IR THRESHOLDS"));
  lcd.setCursor(0, 1);
  lcd.print(F("T1 ")); lcd.print(thresh1);
  lcd.setCursor(7, 1);
  lcd.print(F("T2 ")); lcd.print(thresh2);
  lcd.setCursor(0, 2);
  lcd.print(F("T3 ")); lcd.print(thresh3);
  lcd.setCursor(0, 3);
  lcd.print(F("Push Timer Button"));
  debugln(F("Push Timer Button To Proceed"));

  BankLight(3, laneCount);
  delay(250);
  while (!ReleasedTimer()) {}

  debugln(F("End IR calibration"));
} // end TuneLDR


//*************************************************************************************
// These two routines wait until the push button is RELEASED to record a button press.  Perfect for start button and timer button

bool ReleasedStart() {
  static uint16_t state = 0;
  static unsigned long lastDebounceTime = 0;

  if ((millis() - lastDebounceTime) < DebounceDelay) {          // Check if enough time has passed since the last debounce
    return false;                                               // Debounce in progress, return false
  }
  lastDebounceTime = millis();                                  // Update the debounce time
  state = (state << 1) | digitalRead(StartButton) | 0xfe00;     // Update the button state
  return (state == 0xff00);
} // End ReleasedStart

//*************************************************************************************

bool ReleasedTimer() {                                         // Looks for a button release
  static uint16_t state = 0;
  static unsigned long lastDebounceTime = 0;

  if ((millis() - lastDebounceTime) < DebounceDelay) {          // Check if enough time has passed since the last debounce
    return false;                                               // Debounce in progress, return false
  }
  lastDebounceTime = millis();                                  // Update the debounce time
  state = (state << 1) | digitalRead(TimerButton) | 0xfe00;     // Update the button state
  return (state == 0xff00);
} // end ReleasedTimer

//*************************************************************************************
bool PushedTimer() {

  int reading = digitalRead(TimerButton);
      
    // if the button state has changed:
    if (reading != TimerButtonState) {
      TimerButtonState = reading;
    }
  return(reading);
} // end PushedTimer

//********************************************************
bool PushedStart() {
  int reading = digitalRead(StartButton);
  // debug("StartRead: "); 
  // debugln(reading); 
    
    // if the button state has changed:
    if (reading != TimerButtonState) {
      StartButtonState = reading;
    }
  return(reading);
} // end PushedStart


//********************************************************
void effectC(int speed){
  int prevI = 0;
  for (int i = 0; i < 16; i++){
    regWrite(prevI, LOW);
    regWrite(i, HIGH);
    prevI = i;

    delay(speed);
  }

  for (int i = 15; i >= 0; i--){
    regWrite(prevI, LOW);
    regWrite(i, HIGH);
    prevI = i;

    delay(speed);
  }
}

//***********************************************************************************************

void BankLight(int bank,int laneCount)
// lanecount = number of LEDs in each bank
{
  int startIndex; 
  switch (bank) {
    case 1:
      startIndex = 0;
      break;
    case 2:
      startIndex = 3;;
      break;
    case 3:
      startIndex = 6;
      break;
    default:
      // Invalid bank number
      return;
  } 
  // BlankAll() ;
  for (int i = startIndex; i < (startIndex+laneCount); i++) {
    int value = LEDarray[i];
    regWrite(value, HIGH);
    
  }
}

//***********************************************************************************************
  
void BankPlaceLight(int bank,int Place, int laneCount)
{
  // BlankAll() ;
  int startIndex; 
  switch (bank) {
    case 1:
      startIndex = 0;
      break;
    case 2:
      startIndex = 3;;
      break;
    case 3:
      startIndex = 6;
      break;
    default:
      // Invalid bank number
      return;
  }  // end Switch
  for (int i = startIndex; i < (startIndex+Place); i++) {
    int value = LEDarray[i];
    regWrite(value, HIGH);
    
  } // End for loop
} // end BankPlaceLight


//***************************************************************************************

void BlankAll()
// Turn off all the LEDS
{
  for (int i = 0; i < 16; i++)              // Turn off all the LEDS
  {
    regWrite(i, LOW);
  }
} // end BlankAll

//*************************************************************

void regWrite(int pin, bool state){
  //Determines register
  int reg = pin / 8;
  //Determines pin for actual register
  int actualPin = pin - (8 * reg);

  //Begin session
  digitalWrite(latchPin, LOW);

  for (int i = 0; i < numOfRegisters; i++){
    //Get actual states for register
    byte* states = &registerState[i];

    //Update state
    if (i == reg){
      bitWrite(*states, actualPin, state);
    }

    //Write
    // shiftOut(dataPin, clockPin, MSBFIRST, *states);
    shiftOut(dataPin, clockPin, LSBFIRST, *states);
  }

  //End session
  digitalWrite(latchPin, HIGH);
}