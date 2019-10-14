/**
 * This source file is under General Public License version 3.
 * 
 * This verision uses a built-in Si5351 library
 * Most source code are meant to be understood by the compilers and the computers. 
 * Code that has to be hackable needs to be well understood and properly documented. 
 * Donald Knuth coined the term Literate Programming to indicate code that is written be 
 * easily read and understood.
 * 
 * The Raduino is a small board that includes the Arduin Nano, a 16x2 LCD display and
 * an Si5351a frequency synthesizer. This board is manufactured by Paradigm Ecomm Pvt Ltd
 * 
 * To learn more about Arduino you may visit www.arduino.cc. 
 * 
 * The Arduino works by starts executing the code in a function called setup() and then it 
 * repeatedly keeps calling loop() forever. All the initialization code is kept in setup()
 * and code to continuously sense the tuning knob, the function button, transmit/receive,
 * etc is all in the loop() function. If you wish to study the code top down, then scroll
 * to the bottom of this file and read your way up.
 * 
 * Below are the libraries to be included for building the Raduino 
 * The EEPROM library is used to store settings like the frequency memory, caliberation data, 
 * callsign etc .
 *
 *  The main chip which generates upto three oscillators of various frequencies in the
 *  Raduino is the Si5351a. To learn more about Si5351a you can download the datasheet 
 *  from www.silabs.com although, strictly speaking it is not a requirment to understand this code. 
 *  Instead, you can look up the Si5351 library written by xxx, yyy. You can download and 
 *  install it from www.url.com to complile this file.
 *  The Wire.h library is used to talk to the Si5351 and we also declare an instance of 
 *  Si5351 object to control the clocks.
 */

/**
 * uBITX v5 firmware, modified by K9SUL, Kihwal Lee
 *
 * HW modifications
 * - Uses an Adafruit 292 i2c LCD backpack to free up digital pins. Inspired by the KB1OIQ mod.
 * - Encoder wired to D2, D3 for interrupt-based counting. This is the most reliable method.
 * - More buttons such as tuning step, button lock, band up/down.
 */

#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_LiquidCrystal.h>
#include <Encoder.h>

/**
 * I/O assignment
 * 
 * D2 INT0 Encoder A interrupt
 * D3 INT1 Encoder B interrupt
 * D4 OUT LPF B
 * D5 OUT LPF A
 * D6 OUT CW TONE
 * D7 OUT T/R control
 * D8 OUT CW keying
 * D9 OUT LPF C
 * D10 IN Tuning step
 * D11 IN Lock
 * 
 * A0 IN  Band Up
 * A1 IN Band Down
 * A2 IN F-button
 * A3 IN PTT
 * A6 IN CW Key 
 */

// User inputs
#define BAND_UP (A0)  // up
#define BAND_DN (A1)  // down
#define FBUTTON (A2)  // menu/function button
#define PTT     (A3)  // PTT in
#define ANALOG_KEYER (A6) // CW keying in
#define TU_STEP (10)  // tuning step
#define FLOCK   (11)  // lock

// Internal control outputs
#define TX_RX    (7)  // SSB TX
#define CW_TONE  (6)
#define TX_LPF_A (5)  // turn on LPF A
#define TX_LPF_B (4)  // turn on LPF B
#define TX_LPF_C (9)  // turn on LPF C
#define CW_KEY   (8)  // CW TX

// Band index definitions
#define UBITX_B160 0
#define UBITX_B80 1
#define UBITX_B60 2
#define UBITX_B40 3
#define UBITX_B30 4
#define UBITX_B20 5
#define UBITX_B17 6
#define UBITX_B15 7
#define UBITX_B12 8
#define UBITX_B10 9

/**
 * These are the indices where these user changable settinngs are stored in the EEPROM
 */
#define MASTER_CAL 0
#define LSB_CAL 4
#define USB_CAL 8
#define SIDE_TONE 12
//these are ids of the vfos as well as their offset into the eeprom storage, don't change these 'magic' values
#define VFO_A 16
#define VFO_B 20
#define CW_SIDETONE 24
// Each of 10 bands has its own last-used frequency.  
#define UBITX_BAND_FREQ_BASE 40
// 4 x 10 bytes starting from offset 40. 
#define NEXT_AVAILABLE_OFFSET 44

#define INIT_USB_FREQ   (11059200l)

//we directly generate the CW by programmin the Si5351 to the cw tx frequency, hence, both are different modes
//these are the parameter passed to startTx
#define TX_SSB 0
#define TX_CW 1
#define FIRST_IF 45005000L

// Two main I/O devices
Adafruit_LiquidCrystal lcd(0); // LCD connected through an i2c backpack
Encoder enc1(3,2); // Rotary encoder with both channels on interrupt-enaled pins.

// LCD output buffer
char c[18], b[10];
char printBuff[2][18];  //mirrors what is showing on the two lines of the display

// Band edge data. kHz to Hz conversion only happens on band switching or power on.
static unsigned int band_lower_edge[10] = { 1800, 3500, 5330, 7000, 10100, 14000, 18068, 21000, 24890, 28000 };
static unsigned int band_upper_edge[10] = { 2000, 4000, 5404, 7300, 10150, 14350, 18168, 21450, 24990, 29700 };
unsigned long band_data[10]; // holds last used frequency per band
byte current_band;
unsigned long current_band_lower_edge;
unsigned long current_band_upper_edge;
int tuningStep = 100; // 100 Hz by default
bool locked = false; // freq lock

// operating parameters
char ritOn = 0;
char vfoActive = VFO_A;
unsigned long vfoA=7175000L, vfoB=14200000L, sideTone=800, usbCarrier;
unsigned long frequency, ritRxFrequency, ritTxFrequency;  //frequency is the current frequency on the dial

//these are variables that control the keyer behaviour
extern int32_t calibration;
byte cwDelayTime = 60;

boolean txCAT = false;        //turned on if the transmitting due to a CAT command
char inTx = 0;                //it is set to 1 if in transmit mode (whatever the reason : cw, ptt or cat)
char keyDown = 0;             //in cw mode, denotes the carrier is being transmitted
char isUSB = 0;               //upper sideband was selected, this is reset to the default for the 
                              //frequency when it crosses the frequency border of 10 MHz
byte menuOn = 0;              //set to 1 when the menu is being displayed, if a menu item sets it to zero, the menu is exited
unsigned long cwTimeout = 0;  //milliseconds to go before the cw transmit line is released and the radio goes back to rx mode
unsigned long dbgCount = 0;   //not used now
unsigned char txFilter = 0;   //which of the four transmit filters are in use
boolean modeCalibrate = false;//this mode of menus shows extended menus to calibrate the oscillators and choose the proper
                              //beat frequency

// Switch band. Assumes current_band and frequency are in sync
void switch_band_to(int band) {
  band_data[current_band] = frequency; // save frequency
  set_band(band); // current_band is updated
  frequency = band_data[band]; // load last-used band frequency;
  // go to the middle of the band if no valid prior frequency is found
  if (frequency < current_band_lower_edge || frequency > current_band_upper_edge) {
    frequency = (current_band_lower_edge + current_band_upper_edge)/2;
  }
}

bool is_usb(byte band) {
  return (band > UBITX_B40 || band == UBITX_B60) ? true : false;
}

// set current band to the specified one.
void set_band(int band) {
  current_band_lower_edge = (unsigned long)band_lower_edge[band] * 1000;
  current_band_upper_edge = (unsigned long)band_upper_edge[band] * 1000;
  // set the sideband
  isUSB = is_usb(band);
  current_band = band;
}

// Ensure the frequency stays with in the boundary.
unsigned long ensure_band_limit(unsigned long freq) {
  if (freq < current_band_lower_edge) {
    return current_band_lower_edge;
  } else if (freq > current_band_upper_edge) {
    return current_band_upper_edge;
  } else {
    return freq;
  }
}

// Given a frequency in Hz, return the band index.
int get_band_from_freq(unsigned long freq) {
  int freq_khz = (int)(freq/1000);
  for (int i = UBITX_B160; i <= UBITX_B10; i++) {
    if (freq_khz >= band_lower_edge[i] && freq_khz <= band_upper_edge[i]) {
      return i;
    }
  }
}

void persist_band_data() {
  // save the VFO frequencies
  if (vfoActive == VFO_B) {
    EEPROM.put(VFO_B, frequency);
  } else {
    EEPROM.put(VFO_A, frequency);
  }
  band_data[current_band] = frequency;
  for (int i = UBITX_B160; i <= UBITX_B10; i++) {
    EEPROM.put(UBITX_BAND_FREQ_BASE + (i*4), band_data[i]);
  }
}

void load_band_data() {
  for (int i = UBITX_B160; i <= UBITX_B10; i++) {
    EEPROM.get(UBITX_BAND_FREQ_BASE + (i*4), band_data[i]);
  }
}

/**
 * The uBITX is an upconnversion transceiver. The first IF is at 45 MHz.
 * The first IF frequency is not exactly at 45 Mhz but about 5 khz lower,
 * this shift is due to the loading on the 45 Mhz crystal filter by the matching
 * L-network used on it's either sides.
 * The first oscillator works between 48 Mhz and 75 MHz. The signal is subtracted
 * from the first oscillator to arriive at 45 Mhz IF. Thus, it is inverted : LSB becomes USB
 * and USB becomes LSB.
 * The second IF of 12 Mhz has a ladder crystal filter. If a second oscillator is used at 
 * 57 Mhz, the signal is subtracted FROM the oscillator, inverting a second time, and arrives 
 * at the 12 Mhz ladder filter thus doouble inversion, keeps the sidebands as they originally were.
 * If the second oscillator is at 33 Mhz, the oscilaltor is subtracated from the signal, 
 * thus keeping the signal's sidebands inverted. The USB will become LSB.
 * We use this technique to switch sidebands. This is to avoid placing the lsbCarrier close to
 * 12 MHz where its fifth harmonic beats with the arduino's 16 Mhz oscillator's fourth harmonic
 */

 

/**
 * Our own delay. During any delay, the raduino should still be processing a few times. 
 */

void active_delay(int delay_by){
  unsigned long timeStart = millis();

  while (millis() - timeStart <= delay_by) {
      //Background Work      
    checkCAT();
  }
}

/**
 * Select the properly tx harmonic filters
 * The four harmonic filters use only three relays
 * the four LPFs cover 30-21 Mhz, 18 - 14 Mhz, 7-10 MHz and 3.5 to 5 Mhz
 * Briefly, it works like this, 
 * - When KT1 is OFF, the 'off' position routes the PA output through the 30 MHz LPF
 * - When KT1 is ON, it routes the PA output to KT2. Which is why you will see that
 *   the KT1 is on for the three other cases.
 * - When the KT1 is ON and KT2 is off, the off position of KT2 routes the PA output
 *   to 18 MHz LPF (That also works for 14 Mhz) 
 * - When KT1 is On, KT2 is On, it routes the PA output to KT3
 * - KT3, when switched on selects the 7-10 Mhz filter
 * - KT3 when switched off selects the 3.5-5 Mhz filter
 * See the circuit to understand this
 */

void setTXFilters(unsigned long freq){
  
  if (freq > 21000000L){  // the default filter is with 35 MHz cut-off
    digitalWrite(TX_LPF_A, 0);
    digitalWrite(TX_LPF_B, 0);
    digitalWrite(TX_LPF_C, 0);
  }
  else if (freq >= 14000000L){ //thrown the KT1 relay on, the 30 MHz LPF is bypassed and the 14-18 MHz LPF is allowd to go through
    digitalWrite(TX_LPF_A, 1);
    digitalWrite(TX_LPF_B, 0);
    digitalWrite(TX_LPF_C, 0);
  }
  else if (freq > 7000000L){
    digitalWrite(TX_LPF_A, 0);
    digitalWrite(TX_LPF_B, 1);
    digitalWrite(TX_LPF_C, 0);    
  }
  else {
    digitalWrite(TX_LPF_A, 0);
    digitalWrite(TX_LPF_B, 0);
    digitalWrite(TX_LPF_C, 1);    
  }
}

/**
 * This is the most frequently called function that configures the 
 * radio to a particular frequeny, sideband and sets up the transmit filters
 * 
 * The transmit filter relays are powered up only during the tx so they dont
 * draw any current during rx. 
 * 
 * The carrier oscillator of the detector/modulator is permanently fixed at
 * uppper sideband. The sideband selection is done by placing the second oscillator
 * either 12 Mhz below or above the 45 Mhz signal thereby inverting the sidebands 
 * through mixing of the second local oscillator.
 */
 
void setFrequency(unsigned long f){
  uint64_t osc_f, firstOscillator, secondOscillator;
 
  setTXFilters(f);
  if (isUSB){
    si5351bx_setfreq(2, FIRST_IF  + f);
    si5351bx_setfreq(1, FIRST_IF + usbCarrier);
  }
  else{
    si5351bx_setfreq(2, FIRST_IF + f);
    si5351bx_setfreq(1, FIRST_IF - usbCarrier);
  }
    
  frequency = f;
}

/**
 * startTx is called by the PTT, cw keyer and CAT protocol to
 * put the uBitx in tx mode. It takes care of rit settings, sideband settings
 * Note: In cw mode, doesnt key the radio, only puts it in tx mode
 * CW offest is calculated as lower than the operating frequency when in LSB mode, and vice versa in USB mode
 */
 
void startTx(byte txMode){
  unsigned long tx_freq = 0;  
    
  digitalWrite(TX_RX, 1);
  inTx = 1;
  
  if (ritOn){
    //save the current as the rx frequency
    ritRxFrequency = frequency;
    setFrequency(ritTxFrequency);
  }
  else 
  {
    setFrequency(frequency);
  }

  if (txMode == TX_CW){
    //turn off the second local oscillator and the bfo
    si5351bx_setfreq(0, 0);
    si5351bx_setfreq(1, 0);

    //shif the first oscillator to the tx frequency directly
    //the key up and key down will toggle the carrier unbalancing
    //the exact cw frequency is the tuned frequency + sidetone
    if (isUSB)
      si5351bx_setfreq(2, frequency + sideTone);
    else
      si5351bx_setfreq(2, frequency - sideTone); 
  }
  updateDisplay();
}

void stopTx(){
  inTx = 0;

  digitalWrite(TX_RX, 0);           //turn off the tx
  si5351bx_setfreq(0, usbCarrier);  //set back the cardrier oscillator anyway, cw tx switches it off

  if (ritOn)
    setFrequency(ritRxFrequency);
  else{
    setFrequency(frequency);
  }
  // reset encoder, so that frequency doesn't jump.
  enc1.write(0);
  updateDisplay();
}

/**
 * ritEnable is called with a frequency parameter that determines
 * what the tx frequency will be
 */
void ritEnable(unsigned long f){
  ritOn = 1;
  //save the non-rit frequency back into the VFO memory
  //as RIT is a temporary shift, this is not saved to EEPROM
  ritTxFrequency = f;
}

// this is called by the RIT menu routine
void ritDisable(){
  if (ritOn){
    ritOn = 0;
    setFrequency(ritTxFrequency);
    updateDisplay();
  }
}

/**
 * Basic User Interface Routines. These check the front panel for any activity
 */

/**
 * The PTT is checked only if we are not already in a cw transmit session
 * If the PTT is pressed, we shift to the ritbase if the rit was on
 * flip the T/R line to T and update the display to denote transmission
 */

void checkPTT(){	
  //we don't check for ptt when transmitting cw
  if (cwTimeout > 0)
    return;
    
  if (digitalRead(PTT) == 0 && inTx == 0){
    startTx(TX_SSB);
    active_delay(50); //debounce the PTT
  }
	
  if (digitalRead(PTT) == 1 && inTx == 1)
    stopTx();
}

void checkButton(){
  //only if the button is pressed
  if (!btnDown(FBUTTON))
    return;
  active_delay(50);
  if (!btnDown(FBUTTON)) //debounce
    return;
 
  doMenu();
  //wait for the button to go up again
  while(btnDown(FBUTTON))
    active_delay(10);
  active_delay(50);//debounce
}

void checkFlock(){
  //only if the button is pressed
  if (!btnDown(FLOCK))
    return;
  active_delay(50);
  if (!btnDown(FLOCK)) //debounce
    return;
  if (locked) {
    locked = false;
    enc1.write(0);  // clear any encoder input
  } else {
    locked = true;
  }
  updateDisplay();
  //wait for the button to go up again
  while(btnDown(FLOCK))
    active_delay(10);
  active_delay(50);//debounce
}

void doTuningStep() {
  if (!btnDown(TU_STEP))
    return;
    
  switch(tuningStep) {
    case 10:
      tuningStep = 100;
      break;
    case 100:
      tuningStep = 1000;
      break;
    case 1000:
      tuningStep = 10;
      break;
    default:
      break;
  }
  updateDisplay();
  while (btnDown(TU_STEP))
    active_delay(10);
  active_delay(50);//debounce  
}

void doBandUP() {
  if (!btnDown(BAND_UP))
    return;

  if (current_band < UBITX_B10) {
    switch_band_to(current_band + 1);
  } else {
    return;
  }

  setFrequency(frequency);
  updateDisplay();
  
  while (btnDown(BAND_UP))
    active_delay(10);
  active_delay(50);//debounce 
}

void doBandDN() {
  if (!btnDown(BAND_DN))
    return;

  if (current_band > UBITX_B160) {
    switch_band_to(current_band - 1);
  } else {
    return;
  }

  setFrequency(frequency);
  updateDisplay();
  
  while (btnDown(BAND_DN))
    active_delay(10);
  active_delay(50);//debounce
}

void doTuning(){
  long s;
  unsigned long new_freq;
  s = enc1.read();
  if (s != 0) {
    enc1.write(0);  // reset the encoder
    new_freq = ensure_band_limit(frequency + (s * tuningStep));
    if (new_freq == frequency) {
      return;    
    } else {
      frequency = new_freq;
    }
    setFrequency(frequency);
    updateDisplay();
  }
}


void doRIT(){
  unsigned long newFreq;
 
  int knob = enc_read();
  unsigned long old_freq = frequency;

  if (knob < 0)
    frequency -= 50;
  else if (knob > 0)
    frequency += 50;
 
  if (old_freq != frequency){
    setFrequency(frequency);
    updateDisplay();
  }
}

/**
 * The settings are read from EEPROM. The first time around, the values may not be 
 * present or out of range, in this case, some intelligent defaults are copied into the 
 * variables.
 */
void initSettings(){
  byte x;
  //read the settings from the eeprom and restore them
  //if the readings are off, then set defaults
  EEPROM.get(MASTER_CAL, calibration);
  EEPROM.get(USB_CAL, usbCarrier);
  EEPROM.get(VFO_A, vfoA);
  EEPROM.get(VFO_B, vfoB);
  EEPROM.get(CW_SIDETONE, sideTone);
  load_band_data();

  if (usbCarrier > 11060000l || usbCarrier < 11048000l)
    usbCarrier = 11059000l;
  if (vfoA > 35000000l || 3500000l > vfoA)
     vfoA = 7151000l;
  if (vfoB > 35000000l || 3500000l > vfoB)
     vfoB = 14150000l;  
  if (sideTone < 100 || 2000 < sideTone) 
    sideTone = 800;

  // initialize the encoder
  enc1.write(0);
  
}

void initPorts(){

  analogReference(DEFAULT);

  // buttons
  pinMode(BAND_UP, INPUT_PULLUP);
  pinMode(BAND_DN, INPUT_PULLUP);
  pinMode(TU_STEP, INPUT_PULLUP);
  pinMode(FBUTTON, INPUT_PULLUP);
  pinMode(FLOCK,   INPUT_PULLUP);
  
  pinMode(PTT, INPUT_PULLUP);
  pinMode(ANALOG_KEYER, INPUT_PULLUP);

  pinMode(CW_TONE, OUTPUT);  
  digitalWrite(CW_TONE, 0);
  
  pinMode(TX_RX,OUTPUT);
  digitalWrite(TX_RX, 0);

  pinMode(TX_LPF_A, OUTPUT);
  pinMode(TX_LPF_B, OUTPUT);
  pinMode(TX_LPF_C, OUTPUT);
  digitalWrite(TX_LPF_A, 0);
  digitalWrite(TX_LPF_B, 0);
  digitalWrite(TX_LPF_C, 0);

  pinMode(CW_KEY, OUTPUT);
  digitalWrite(CW_KEY, 0);
}

void setup()
{
  Serial.begin(38400);
  Serial.flush();  
  
  lcd.begin(16, 2);

  // initial spalsh screen
  printLine1((char*)"K9SUL Kihwal Lee");
  printLine2((char*)"uBITX v5.1"); 
  active_delay(2000);

  initSettings();
  initPorts();     
  initOscillators();

  // set the initial frequency.
  frequency = vfoA;
  set_band(get_band_from_freq(frequency));
  setFrequency(frequency);
  updateDisplay();

  if (btnDown(FBUTTON))
    factory_alignment();
}


/**
 * The loop checks for keydown, ptt, function button and tuning.
 */

void loop(){ 
  cwKeyer(); 
  if (!txCAT)
    checkPTT();
  checkFlock();
  if (!locked)
    checkButton();
 
  //tune only when not tranmsitting and not locked.
  if (!inTx && !locked){
    if (ritOn)
      doRIT();
    else {
      doTuningStep();
      doTuning();
      doBandDN();
      doBandUP();
    }
  }
  
  //we check CAT after the encoder as it might put the radio into TX
  checkCAT();
}
