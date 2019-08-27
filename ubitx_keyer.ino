/**
 CW Keyer
 CW Key logic change with ron's code (ubitx_keyer.cpp)
 Ron's logic has been modified to work with the original uBITX by KD8CEC

 * 
 * Generating CW
 * The CW is cleanly generated by unbalancing the front-end mixer
 * and putting the local oscillator directly at the CW transmit frequency.
 * The sidetone, generated by the Arduino is injected into the volume control
 */

/**
 * Starts transmitting the carrier with the sidetone
 * It assumes that we have called cwTxStart and not called cwTxStop
 * each time it is called, the cwTimeOut is pushed further into the future
 */
void cwKeydown(){
  keyDown = 1;                  //tracks the CW_KEY
  tone(CW_TONE, (int)sideTone); 
  digitalWrite(CW_KEY, 1);     

  cwTimeout = millis() + cwDelayTime * 10;  
}

/**
 * Stops the cw carrier transmission along with the sidetone
 * Pushes the cwTimeout further into the future
 */
void cwKeyUp(){
  keyDown = 0;    //tracks the CW_KEY
  noTone(CW_TONE);
  digitalWrite(CW_KEY, 0);    
  
  //Modified by KD8CEC, for CW Delay Time save to eeprom
  //cwTimeout = millis() + CW_TIMEOUT;
  cwTimeout = millis() + cwDelayTime * 10;
}

char keyed(void) {
  unsigned char tmpKeyerControl = 0;
  
  int paddle = analogRead(ANALOG_KEYER);
  if (paddle >= 0 && paddle <= 100) {
    return 1;
  } else {
    return 0;
  }
}

void cwKeyer(void){
    while(1){
      if (keyed() == 1) {
        // if we are here, it is only because the key is pressed
        if (!inTx){
          //DelayTime Option
          active_delay(100);
          
          keyDown = 0;
          cwTimeout = millis() + cwDelayTime * 10;  //+ CW_TIMEOUT; 
          startTx(TX_CW);
        }
        cwKeydown();
        
        while (keyed() == 1) 
          active_delay(1);
          
        cwKeyUp();
      }
      else{
        if (0 < cwTimeout && cwTimeout < millis()){
          cwTimeout = 0;
          keyDown = 0;
          stopTx();
        }
        return;
      }

      checkCAT();
    } //end of while

}
