/** Menus
    The Radio menus are accessed by tapping on the function button.
    - The main loop() constantly looks for a button press and calls doMenu() when it detects
    a function button press.
    - As the encoder is rotated, at every 10th pulse, the next or the previous menu
    item is displayed. Each menu item is controlled by it's own function.
    - Eache menu function may be called to display itself
    - Each of these menu routines is called with a button parameter.
    - The btn flag denotes if the menu itme was clicked on or not.
    - If the menu item is clicked on, then it is selected,
    - If the menu item is NOT clicked on, then the menu's prompt is to be displayed
*/


/** A generic control to read variable values
*/
int getValueByKnob(int minimum, int maximum, int step_size,  int initial, char* prefix, char *postfix)
{
  int knob = 0;
  int knob_value;

  while (btnDown(FBUTTON))
    active_delay(100);

  active_delay(200);
  knob_value = initial;

  strcpy(b, prefix);
  itoa(knob_value, c, 10);
  strcat(b, c);
  strcat(b, postfix);
  printLine2(b);
  active_delay(300);

  while (!btnDown(FBUTTON) && digitalRead(PTT) == HIGH) {

    knob = enc_read();
    if (knob != 0) {
      if (knob_value > minimum && knob < 0)
        knob_value -= step_size;
      if (knob_value < maximum && knob > 0)
        knob_value += step_size;

      printLine2(prefix);
      itoa(knob_value, c, 10);
      strcpy(b, c);
      strcat(b, postfix);
      printLine1(b);
    }
    checkCAT();
  }

  return knob_value;
}

// Menu #1
void menuBand(int btn) {
  if (!btn) {
    printLine2((char*)"Save band data?");
  } else {
    persist_band_data();
    printLine2((char*)"Saved");
    menuOn = 0;
    active_delay(500);
    updateDisplay();
  }
}

// Menu #2
void menuRitToggle(int btn) {
  if (!btn) {
    if (ritOn == 1)
      printLine2((char*)"RIT On \x7E Off");
    else
      printLine2((char*)"RIT Off \x7E On");
  }
  else {
    if (ritOn == 0) {
      //enable RIT so the current frequency is used at transmit
      ritEnable(frequency);
      printLine2((char*)"RIT is On");

    }
    else {
      ritDisable();
      printLine2((char*)"RIT is Off");
    }
    menuOn = 0;
    active_delay(500);
    updateDisplay();
  }
}


//Menu #3
void menuVfoToggle(int btn) {

  if (!btn) {
    if (vfoActive == VFO_A)
      printLine2((char*)"VFO A \x7E B");
    else
      printLine2((char*)"VFO B \x7E A");
  }
  else {
    unsigned long new_freq;
    if (vfoActive == VFO_B) {
      // save current frequency and mode to VFO B
      vfoB = frequency;
      // switch to VFO A
      vfoActive = VFO_A;
      new_freq = vfoA;
    }
    else {
      vfoA = frequency;
      vfoActive = VFO_B;
      new_freq = vfoB;
    }

    ritDisable();
    switch_band_to(get_band_from_freq(new_freq));
    frequency = new_freq; // override the restored value;
    setFrequency(frequency);
    updateDisplay();
    menuOn = 0;
  }
}

// Menu #4
void menuSidebandToggle(int btn) {
  if (!btn) {
    if (isUSB == true)
      printLine2((char*)"USB \x7E LSB");
    else
      printLine2((char*)"LSB \x7E USB");
  }
  else {
    if (isUSB == true) {
      isUSB = false;
    }
    else {
      isUSB = true;
    }
    setFrequency(frequency);
    updateDisplay();
    menuOn = 0;
  }
}
int standard_channel = 0;
static int standards[8] = { 2500, 3330, 5000, 7850, 10000, 14670, 15000, 20000 };

void set_standard(byte chan) {
    setFrequency((unsigned long)standards[chan]*1000);
    itoa(standards[chan], b, DEC);
    strcat(b, " kHz");
    printLine1(b);
}

void menu_standard(int btn) {
  if (!btn) {
    printLine2((char*)"Standard bcast");
  } else {
    while (btnDown(FBUTTON))
      active_delay(50);
    long original_freq = frequency;
    set_standard(standard_channel);
    // Enter the submenu
    while(true) {
      if (btnDown(FBUTTON)) {
        while (btnDown(FBUTTON))
          active_delay(50);
        active_delay(50);
        standard_channel++;
        standard_channel %= 8;
        set_standard(standard_channel);
      }

      if (btnDown(FLOCK)) {
        while (btnDown(FLOCK))
          active_delay(50);
        active_delay(100);
        setFrequency(original_freq);
        updateDisplay();
        menuOn = 0;
        return;
      }
    }
  }
}

void menuExit(int btn) {

  if (!btn) {
    printLine2((char*)"Exit Menu      \x7E");
  }
  else {
    printLine2((char*)"Exiting...");
    setFrequency(frequency);
    active_delay(500);
    updateDisplay();
    menuOn = 0;
  }
}

/**
   The calibration routines are not normally shown in the menu as they are rarely used
   They can be enabled by choosing this menu option
*/
int menuSetup(int btn) {
  if (!btn) {
    if (!modeCalibrate)
      printLine2((char*)"Settings       \x7E");
    else
      printLine2((char*)"Settings \x7E Off");
  } else {
    if (!modeCalibrate) {
      modeCalibrate = true;
      printLine2((char*)"Settings On");
    }
    else {
      modeCalibrate = false;
      printLine2((char*)"Settings Off");
    }

    while (btnDown(FBUTTON))
      active_delay(100);
    active_delay(500);
    return 10;
  }
  return 0;
}

//this is used by the si5351 routines in the ubitx_5351 file
extern int32_t calibration;
extern uint32_t si5351bx_vcoa;

int calibrateClock() {
  int knob = 0;
  int32_t prev_calibration;


  //keep clear of any previous button press
  while (btnDown(FBUTTON))
    active_delay(100);
  active_delay(100);

  digitalWrite(TX_LPF_A, 0);
  digitalWrite(TX_LPF_B, 0);
  digitalWrite(TX_LPF_C, 0);

  prev_calibration = calibration;
  calibration = 0;

  isUSB = true;

  //turn off the second local oscillator and the bfo
  si5351_set_calibration(calibration);
  startTx(TX_CW);
  si5351bx_setfreq(2, 10000000l);

  strcpy(b, "#1 10 MHz cal:");
  ltoa(calibration / 8750, c, 10);
  strcat(b, c);
  printLine2(b);

  while (!btnDown(FBUTTON))
  {

    if (digitalRead(PTT) == LOW && !keyDown)
      cwKeydown();
    if (digitalRead(PTT)  == HIGH && keyDown)
      cwKeyUp();

    knob = enc_read();

    if (knob > 0)
      calibration += 875;
    else if (knob < 0)
      calibration -= 875;
    else
      continue; //don't update the frequency or the display

    si5351_set_calibration(calibration);
    si5351bx_setfreq(2, 10000000l);
    strcpy(b, "#1 10 MHz cal:");
    ltoa(calibration / 8750, c, 10);
    strcat(b, c);
    printLine2(b);
  }

  cwTimeout = 0;
  keyDown = 0;
  stopTx();

  printLine2((char*)"Calibration set!");
  EEPROM.put(MASTER_CAL, calibration);
  initOscillators();
  setFrequency(frequency);
  updateDisplay();

  while (btnDown(FBUTTON))
    active_delay(50);
  active_delay(100);
}

int menuSetupCalibration(int btn) {
  int knob = 0;
  int32_t prev_calibration;

  if (!btn) {
    printLine2((char*)"Setup:Calibrate\x7E");
    return 0;
  }

  printLine1((char*)"Press PTT & tune");
  printLine2((char*)"to exactly 10 MHz");
  active_delay(2000);
  calibrateClock();
}

void printCarrierFreq(unsigned long freq) {

  memset(c, 0, sizeof(c));
  memset(b, 0, sizeof(b));

  ultoa(freq, b, DEC);

  strncat(c, b, 2);
  strcat(c, ".");
  strncat(c, &b[2], 3);
  strcat(c, ".");
  strncat(c, &b[5], 1);
  printLine2(c);
}

void menuSetupCarrier(int btn) {
  int knob = 0;
  unsigned long prevCarrier;

  if (!btn) {
    printLine2((char*)"Setup:BFO      \x7E");
    return;
  }

  prevCarrier = usbCarrier;
  printLine1((char*)"Tune to best Signal");
  printLine2((char*)"Press to confirm. ");
  active_delay(1000);

  usbCarrier = 11053000l;
  si5351bx_setfreq(0, usbCarrier);
  printCarrierFreq(usbCarrier);

  //disable all clock 1 and clock 2
  while (!btnDown(FBUTTON)) {
    knob = enc_read();

    if (knob > 0)
      usbCarrier -= 50;
    else if (knob < 0)
      usbCarrier += 50;
    else
      continue; //don't update the frequency or the display

    si5351bx_setfreq(0, usbCarrier);
    printCarrierFreq(usbCarrier);

    active_delay(100);
  }

  printLine2((char*)"Carrier set!    ");
  EEPROM.put(USB_CAL, usbCarrier);
  active_delay(1000);

  si5351bx_setfreq(0, usbCarrier);
  setFrequency(frequency);
  updateDisplay();
  menuOn = 0;
}

void menuSetupCwTone(int btn) {
  int knob = 0;
  int prev_sideTone;

  if (!btn) {
    printLine2((char*)"Setup:CW Tone  \x7E");
    return;
  }

  prev_sideTone = sideTone;
  printLine1((char*)"Tune CW tone");
  printLine2((char*)"PTT to confirm. ");
  active_delay(1000);
  tone(CW_TONE, sideTone);

  //disable all clock 1 and clock 2
  while (digitalRead(PTT) == HIGH && !btnDown(FBUTTON))
  {
    knob = enc_read();

    if (knob > 0 && sideTone < 2000)
      sideTone += 10;
    else if (knob < 0 && sideTone > 100 )
      sideTone -= 10;
    else
      continue; //don't update the frequency or the display

    tone(CW_TONE, sideTone);
    itoa(sideTone, b, 10);
    printLine2(b);

    checkCAT();
    active_delay(20);
  }
  noTone(CW_TONE);
  //save the setting
  if (digitalRead(PTT) == LOW) {
    printLine2((char*)"Sidetone set!    ");
    EEPROM.put(CW_SIDETONE, sideTone);
    active_delay(2000);
  }
  else
    sideTone = prev_sideTone;

  updateDisplay();
  menuOn = 0;
}

void menuSetupCwDelay(int btn) {
  int knob = 0;
  int prev_cw_delay;

  if (!btn) {
    printLine2((char*)"Setup:CW Delay \x7E");
    return;
  }

  active_delay(500);
  prev_cw_delay = cwDelayTime;
  cwDelayTime = getValueByKnob(10, 1000, 50,  cwDelayTime, (char*)"CW Delay>", (char*)" msec");

  printLine1((char*)"CW Delay Set!");
  active_delay(500);
  updateDisplay();
  menuOn = 0;
}


void menuReadADC(int btn) {
  int adc;

  if (!btn) {
    printLine2((char*)"6:Setup>Read ADC>");
    return;
  }
  delay(500);

  while (!btnDown(FBUTTON)) {
    adc = analogRead(ANALOG_KEYER);
    itoa(adc, b, 10);
    printLine1(b);
  }
  updateDisplay();
}

void doMenu() {
  int select = 0, i, btnState;

  //wait for the button to be raised up
  while (btnDown(FBUTTON))
    active_delay(50);
  active_delay(50);  //debounce

  menuOn = 2;

  while (menuOn) {
    i = enc_read();
    // exit menu on lock/exit button
    if (btnDown(FLOCK)) {
      while (btnDown(FLOCK))
        active_delay(50);
      if (!ritOn)
        setFrequency(frequency);
      updateDisplay();
      menuOn = 0;
      return;
    }

    btnState = btnDown(FBUTTON);
    if (i > 0) {
      if (modeCalibrate && select + i < 150)
        select += i;
      if (!modeCalibrate && select + i < 80)
        select += i;
    }
    if (i < 0 && select - i >= 0)
      select += i;      //caught ya, i is already -ve here, so you add it

    if (select < 10)
      menuBand(btnState);
    else if (select < 20)
      menuRitToggle(btnState);
    else if (select < 30)
      menuVfoToggle(btnState);
    else if (select < 40)
      menuSidebandToggle(btnState);
    else if (select < 50)
      menu_standard(btnState);
    else if (select < 60)
      select += menuSetup(btnState);
    else if (select < 70 && !modeCalibrate)
      menuExit(btnState);
    else if (select < 80 && modeCalibrate)
      menuSetupCalibration(btnState);   //crystal
    else if (select < 90 && modeCalibrate)
      menuSetupCarrier(btnState);       //lsb
    else if (select < 100 && modeCalibrate)
      menuSetupCwTone(btnState);
    else if (select < 110 && modeCalibrate)
      menuSetupCwDelay(btnState);
    else if (select < 120 && modeCalibrate)
      menuReadADC(btnState);
    else
      menuExit(btnState);
  }

  //debounce the button
  while (btnDown(FBUTTON))
    active_delay(50);
  active_delay(50);
  enc1.write(0);

  checkCAT();
}
