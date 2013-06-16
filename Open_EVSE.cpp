// Need file comment
#include <EEPROM.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <Time.h>
#include <pins_arduino.h>

#if defined(ARDUINO) && (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h" // shouldn't need this but arduino sometimes messes up and puts inside an #ifdef
#endif // ARDUINO

#include "Open_EVSE.h"
#include "OnboardDisplay.h"
extern OnboardDisplay g_OBD;


#ifdef SERIALCLI
#include "CLI.h"
extern CLI g_CLI;
#endif


#ifdef BTN_MENU
#include "ButtonMenu.h"
extern BtnHandler g_BtnHandler;
#endif


THRESH_DATA g_DefaultThreshData = {875,780,690,0,260};
J1772EVSEController g_EvseController;

#ifdef LCD16X2
#ifdef ADVPWR
extern prog_char g_psPwrOn[] PROGMEM;
extern prog_char g_psSelfTest[] PROGMEM;
extern prog_char g_psAutoDetect[] PROGMEM;
extern prog_char g_psLevel1[] PROGMEM;
extern prog_char g_psLevel2[] PROGMEM;
//prog_char g_psStuckRelay[] PROGMEM;
//prog_char g_psEarthGround[] PROGMEM;
//prog_char g_psTestPassed[] PROGMEM;
//prog_char g_psTestFailed[] PROGMEM;
#endif // ADVPWR
extern prog_char g_psEvseError[] PROGMEM;
extern prog_char g_psVentReq[] PROGMEM;
extern prog_char g_psDiodeChkFailed[] PROGMEM;
extern prog_char g_psGfciFault[] PROGMEM;
extern prog_char g_psNoGround[] PROGMEM;
extern prog_char g_psEStuckRelay[] PROGMEM;
extern prog_char g_psStopped[] PROGMEM;
extern prog_char g_psEvConnected[] PROGMEM;
extern prog_char g_psEvNotConnected[] PROGMEM;
#endif // LCD16X2

void SaveSettings()
{
  // n.b. should we add dirty bits so we only write the changed values? or should we just write them on the fly when necessary?
  EEPROM.write((g_EvseController.GetCurSvcLevel() == 1) ? EOFS_CURRENT_CAPACITY_L1 : EOFS_CURRENT_CAPACITY_L2,(byte)g_EvseController.GetCurrentCapacity());
  EEPROM.write(EOFS_FLAGS,g_EvseController.GetFlags());
}


#ifdef GFI
// interrupt service routing
void gfi_isr()
{
  g_EvseController.SetGfiTripped();
}
#endif // GFI


void EvseReset()
{
  g_OBD.Init();
  g_EvseController.Init();

#ifdef SERIALCLI
  g_CLI.Init();
#endif // SERIALCLI
}

void setup()
{
  wdt_disable();
  
#ifdef SERIALCLI
  g_CLI.setup();
#endif // SERIALCLI

#ifdef GFI
  // GFI triggers on rising edge
  attachInterrupt(GFI_INTERRUPT,gfi_isr,RISING);
#endif // GFI

  EvseReset();

#ifdef BTN_MENU
  g_BtnHandler.init();
#endif // BTN_MENU

#ifdef WATCHDOG
  // WARNING: ALL DELAYS *MUST* BE SHORTER THAN THIS TIMER OR WE WILL GET INTO
  // AN INFINITE RESET LOOP
  wdt_enable(WDTO_1S);   // enable watchdog timer
#endif // WATCHDOG
} 


void loop()
{ 
#ifdef WATCHDOG
  wdt_reset(); // pat the dog
#endif // WATCHDOG

#ifdef SERIALCLI
  g_CLI.getInput();
#endif // SERIALCLI

  g_EvseController.Update();
  g_OBD.Update();
  
#ifdef BTN_MENU
  g_BtnHandler.ChkBtn();
#endif // BTN_MENU

}


//-- begin J1772Pilot

void J1772Pilot::Init()
{
  pinMode(PILOT_PIN,OUTPUT);
  m_bit = digitalPinToBitMask(PILOT_PIN);
  m_port = digitalPinToPort(PILOT_PIN);

  SetState(PILOT_STATE_P12); // turns the pilot on 12V steady state
}


// no PWM pilot signal - steady state
// PILOT_STATE_P12 = steady +12V (EVSE_STATE_A - VEHICLE NOT CONNECTED)
// PILOT_STATE_N12 = steady -12V (EVSE_STATE_F - FAULT) 
void J1772Pilot::SetState(PILOT_STATE state)
{
  volatile uint8_t *out = portOutputRegister(m_port);

  uint8_t oldSREG = SREG;
  cli();
  TCCR1A = 0; //disable pwm by turning off COM1A1,COM1A0,COM1B1,COM1B0
  if (state == PILOT_STATE_P12) {
    *out |= m_bit;  // set pin high
  }
  else {
    *out &= ~m_bit;  // set pin low
  }
  SREG = oldSREG;

  m_State = state;
}


// set EVSE current capacity in Amperes
// duty cycle 
// outputting a 1KHz square wave to digital pin 10 via Timer 1
//
int J1772Pilot::SetPWM(int amps)
{
  uint8_t ocr1b = 0;
  if ((amps >= 6) && (amps <= 51)) {
    ocr1b = 25 * amps / 6 - 1;  // J1772 states "Available current = (duty cycle %) X 0.6"
   } else if ((amps > 51) && (amps <= 80)) {
     ocr1b = amps + 159;  // J1772 states "Available current = (duty cycle % - 64) X 2.5"
   }
  else {
    return 1; // error
  }

  if (ocr1b) {
    // Timer1 initialization:
    // 16MHz / 64 / (OCR1A+1) / 2 on digital 9
    // 16MHz / 64 / (OCR1A+1) on digital 10
    // 1KHz variable duty cycle on digital 10, 500Hz fixed 50% on digital 9
    // pin 10 duty cycle = (OCR1B+1)/(OCR1A+1)
    uint8_t oldSREG = SREG;
    cli();

    TCCR1A = _BV(COM1A0) | _BV(COM1B1) | _BV(WGM11) | _BV(WGM10);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11) | _BV(CS10);
    OCR1A = 249;

    // 10% = 24 , 96% = 239
    OCR1B = ocr1b;

    SREG = oldSREG;

    m_State = PILOT_STATE_PWM;
    return 0;
  }
  else { // !duty
    // invalid amps
    return 1;
  }
}

//-- end J1772Pilot

//-- begin J1772EVSEController

J1772EVSEController::J1772EVSEController()
{
}

void J1772EVSEController::chargingOn()
{  // turn on charging current
  digitalWrite(CHARGING_PIN,HIGH); 
  m_bVFlags |= ECVF_CHARGING_ON;
  
  m_ChargeStartTime = now();
  m_ChargeStartTimeMS = millis();
}

void J1772EVSEController::chargingOff()
{ // turn off charging current
  digitalWrite(CHARGING_PIN,LOW); 
  m_bVFlags &= ~ECVF_CHARGING_ON;

  m_ChargeOffTime = now();
  m_ChargeOffTimeMS = millis();
} 

#ifdef GFI
inline void J1772EVSEController::SetGfiTripped()
{
  m_bVFlags |= ECVF_GFI_TRIPPED;

  // this is repeated Update(), but we want to keep latency as low as possible
  // for safety so we do it here first anyway
  chargingOff(); // turn off charging current
  // turn off the pilot
  m_Pilot.SetState(PILOT_STATE_N12);

  m_Gfi.SetFault();
  // the rest of the logic will be handled in Update()
}
#endif // GFI

void J1772EVSEController::EnableDiodeCheck(uint8_t tf)
{
  if (tf) {
    m_bFlags &= ~ECF_DIODE_CHK_DISABLED;
  }
  else {
    m_bFlags |= ECF_DIODE_CHK_DISABLED;
  }
}

void J1772EVSEController::EnableVentReq(uint8_t tf)
{
  if (tf) {
    m_bFlags &= ~ECF_VENT_REQ_DISABLED;
  }
  else {
    m_bFlags |= ECF_VENT_REQ_DISABLED;
  }
}

#ifdef ADVPWR
void J1772EVSEController::EnableGndChk(uint8_t tf)
{
  if (tf) {
    m_bFlags &= ~ECF_GND_CHK_DISABLED;
  }
  else {
    m_bFlags |= ECF_GND_CHK_DISABLED;
  }
}

void J1772EVSEController::EnableStuckRelayChk(uint8_t tf)
{
  if (tf) {
    m_bFlags &= ~ECF_STUCK_RELAY_CHK_DISABLED;
  }
  else {
    m_bFlags |= ECF_STUCK_RELAY_CHK_DISABLED;
  }
}

void J1772EVSEController::EnableAutoSvcLevel(uint8_t tf)
{
  if (tf) {
    m_bFlags &= ~ECF_AUTO_SVC_LEVEL_DISABLED;
  }
  else {
    m_bFlags |= ECF_AUTO_SVC_LEVEL_DISABLED;
  }
}


#endif // ADVPWR

void J1772EVSEController::EnableSerDbg(uint8_t tf)
{
  if (tf) {
    m_bFlags |= ECF_SERIAL_DBG;
  }
  else {
    m_bFlags &= ~ECF_SERIAL_DBG;
  }
}

void J1772EVSEController::Enable()
{
  m_PrevEvseState = EVSE_STATE_DISABLED;
  m_EvseState = EVSE_STATE_UNKNOWN;
  m_Pilot.SetState(PILOT_STATE_P12);
}

void J1772EVSEController::Disable()
{
  m_EvseState = EVSE_STATE_DISABLED;
  chargingOff();
  m_Pilot.SetState(PILOT_STATE_N12);
  g_OBD.Update();
  // cancel state transition so g_OBD doesn't keep updating
  m_PrevEvseState = EVSE_STATE_DISABLED;
}

void J1772EVSEController::LoadThresholds()
{
    memcpy(&m_ThreshData,&g_DefaultThreshData,sizeof(m_ThreshData));
}

void J1772EVSEController::SetSvcLevel(uint8_t svclvl)
{
#ifdef SERIALCLI
  if (SerDbgEnabled()) {
    g_CLI.print_P(PSTR("SetSvcLevel: "));Serial.println((int)svclvl);
  }
#endif

  if (svclvl == 2) {
    m_bFlags |= ECF_L2; // set to Level 2
  }
  else {
    svclvl = 1;
    m_bFlags &= ~ECF_L2; // set to Level 1
  }

  uint8_t ampacity =  EEPROM.read((svclvl == 1) ? EOFS_CURRENT_CAPACITY_L1 : EOFS_CURRENT_CAPACITY_L2);

  if ((ampacity == 0xff) || (ampacity == 0)) {
    ampacity = (svclvl == 1) ? DEFAULT_CURRENT_CAPACITY_L1 : DEFAULT_CURRENT_CAPACITY_L2;
  }
  
  if (ampacity < MIN_CURRENT_CAPACITY) {
    ampacity = MIN_CURRENT_CAPACITY;
  }
  else {
    if (svclvl == 1) { // L1
      if (ampacity > MAX_CURRENT_CAPACITY_L1) {
        ampacity = MAX_CURRENT_CAPACITY_L1;
      }
    }
    else {
      if (ampacity > MAX_CURRENT_CAPACITY_L2) {
        ampacity = MAX_CURRENT_CAPACITY_L2;
      }
    }
  }


  LoadThresholds();

  SetCurrentCapacity(ampacity);
}

#ifdef ADVPWR
uint8_t J1772EVSEController::doPost()
{
  int PS1state,PS2state;
  uint8_t svclvl = 0;

  m_Pilot.SetState(PILOT_STATE_P12); //check to see if EV is plugged in - write early so it will stabilize before reading.
  g_OBD.SetRedLed(HIGH); 
#ifdef LCD16X2 //Adafruit RGB LCD
  g_OBD.LcdMsg_P(g_psPwrOn,g_psSelfTest);
  
#endif //Adafruit RGB LCD 

  //  PS1state = digitalRead(ACLINE1_PIN); //STUCK RELAY test read AC voltage with Relay Open 
  //  PS2state = digitalRead(ACLINE2_PIN); //STUCK RELAY test read AC voltage with Relay Open

//  if ((PS1state == LOW) || (PS2state == LOW)) {   // If AC voltage is present (LOW) than the relay is stuck
//    m_Pilot.SetState(PILOT_STATE_N12);
// #ifdef LCD16X2 //Adafruit RGB LCD
//    g_OBD.LcdSetBacklightColor(RED);
//    g_OBD.LcdMsg_P(g_psStuckRelay,g_psTestFailed);
// #endif  //Adafruit RGB LCD
//  } 
//  else {
// #ifdef LCD16X2 //Adafruit RGB LCD
//    g_OBD.LcdMsg_P(g_psStuckRelay,g_psTestPassed);
//    delay(1000);
// #endif //Adafruit RGB LCD
  
  if (AutoSvcLevelEnabled()) {
    int reading = analogRead(VOLT_PIN); //read pilot
    m_Pilot.SetState(PILOT_STATE_N12);
    if (reading > 0) {              // IF no EV is plugged in its Okay to open the relay the do the L1/L2 and ground Check
      digitalWrite(CHARGING_PIN, HIGH);
      delay(500);
      PS1state = digitalRead(ACLINE1_PIN);
      PS2state = digitalRead(ACLINE2_PIN);
      digitalWrite(CHARGING_PIN, LOW);
//      if ((PS1state == HIGH) && (PS2state == HIGH)) {     
	// m_EvseState = EVSE_STATE_NO_GROUND;
//  #ifdef LCD16X2 //Adafruit RGB LCD
//	g_OBD.LcdSetBacklightColor(RED); 
//	g_OBD.LcdMsg_P(g_psEarthGround,g_psTestFailed);
//  #endif  //Adafruit RGB LCD
//      } 
//      else {
//   #ifdef LCD16X2 //Adafruit RGB LCD
//	g_OBD.LcdMsg_P(g_psEarthGround,g_psTestPassed);
//	delay(1000);
//   #endif  //Adafruit RGB LCD

	if ((PS1state == LOW) && (PS2state == LOW)) {  //L2   
#ifdef LCD16X2 //Adafruit RGB LCD
	  g_OBD.LcdMsg_P(g_psAutoDetect,g_psLevel2);
	  delay(500);
#endif //Adafruit RGB LCD

	  svclvl = 2; // L2
	}  
	else { // L1
#ifdef LCD16X2 //Adafruit RGB LCD
	 g_OBD.LcdMsg_P(g_psAutoDetect,g_psLevel1);
	 delay(500);
#endif //Adafruit RGB LCD
	  svclvl = 1; // L1
	}
      }  
  }
//    } 
//  }
  
  g_OBD.SetRedLed(LOW); // Red LED off for ADVPWR

  m_Pilot.SetState(PILOT_STATE_P12);
  

  return svclvl;
}
#endif // ADVPWR

void J1772EVSEController::Init()
{
  pinMode(CHARGING_PIN,OUTPUT);

#ifdef ADVPWR
  pinMode(ACLINE1_PIN, INPUT);
  pinMode(ACLINE2_PIN, INPUT);
  digitalWrite(ACLINE1_PIN, HIGH); // enable pullup
  digitalWrite(ACLINE2_PIN, HIGH); // enable pullup
#endif // ADVPWR

  chargingOff();

  m_Pilot.Init(); // init the pilot

  uint8_t svclvl = (uint8_t)DEFAULT_SERVICE_LEVEL;

  uint8_t rflgs = EEPROM.read(EOFS_FLAGS);
  if (rflgs == 0xff) { // uninitialized EEPROM
    m_bFlags = ECF_DEFAULT;
  }
  else {
    m_bFlags = rflgs;
    svclvl = GetCurSvcLevel();
  }

  m_bVFlags = ECVF_DEFAULT;

#ifdef GFI
  m_GfiRetryCnt = 0;
  m_GfiTripCnt = 0;
#endif // GFI
#ifdef ADVPWR
  m_NoGndRetryCnt = 0;
  m_NoGndTripCnt = 0;
#endif // ADVPWR

  m_EvseState = EVSE_STATE_UNKNOWN;
  m_PrevEvseState = EVSE_STATE_UNKNOWN;


#ifdef ADVPWR  // Power on Self Test for Advanced Power Supply
  uint8_t psvclvl = doPost(); // auto detect service level overrides any saved values
  if (psvclvl != 0) {
    svclvl = psvclvl;
  }
#endif // ADVPWR  

  SetSvcLevel(svclvl);

#ifdef GFI
  m_Gfi.Init();
#endif // GFI


  g_OBD.SetGreenLed(LOW);
}

//TABLE A1 - PILOT LINE VOLTAGE RANGES (recommended.. adjust as necessary
//                           Minimum Nominal Maximum 
//Positive Voltage, State A  11.40 12.00 12.60 
//Positive Voltage, State B  8.36 9.00 9.56 
//Positive Voltage, State C  5.48 6.00 6.49 
//Positive Voltage, State D  2.62 3.00 3.25 
//Negative Voltage - States B, C, D, and F -11.40 -12.00 -12.60 
void J1772EVSEController::Update()
{
  if (m_EvseState == EVSE_STATE_DISABLED) {
    // n.b. don't know why, but if we don't have the delay below
    // then doEnableEvse will always have packetBuf[2] == 0
    // so we can't re-enable the EVSE
    delay(5);
    return;
  }

  uint8_t prevevsestate = m_EvseState;
  uint8_t tmpevsestate = EVSE_STATE_UNKNOWN;
  uint8_t nofault = 1;

  int plow;
  int phigh;

#ifdef ADVPWR
  int PS1state = digitalRead(ACLINE1_PIN);
  int PS2state = digitalRead(ACLINE2_PIN);

 if (chargingIsOn()) { // ground check - can only test when relay closed
    if (GndChkEnabled()) {
      if (((millis()-m_ChargeStartTimeMS) > GROUND_CHK_DELAY) && (PS1state == HIGH) && (PS2state == HIGH)) {
	// bad ground
	
	tmpevsestate = EVSE_STATE_NO_GROUND;
	m_EvseState = EVSE_STATE_NO_GROUND;
	
	chargingOff(); // open the relay

	if (m_NoGndTripCnt < 255) {
	  m_NoGndTripCnt++;
	}
	m_NoGndTimeout = millis() + GFI_TIMEOUT;

	nofault = 0;
      }
    }
  }
  else { // !chargingIsOn() - relay open
    if (prevevsestate == EVSE_STATE_NO_GROUND) {
      if ((m_NoGndRetryCnt < GFI_RETRY_COUNT) &&
	  (millis() >= m_NoGndTimeout)) {
	m_NoGndRetryCnt++;
      }
      else {
	tmpevsestate = EVSE_STATE_NO_GROUND;
	m_EvseState = EVSE_STATE_NO_GROUND;
	
	nofault = 0;
      }
    }
    else if (StuckRelayChkEnabled()) {    // stuck relay check - can test only when relay open
      if (((prevevsestate == EVSE_STATE_STUCK_RELAY) || ((millis()-m_ChargeOffTimeMS) > STUCK_RELAY_DELAY)) &&
	  ((PS1state == LOW) || (PS2state == LOW))) {
	// stuck relay
	
	tmpevsestate = EVSE_STATE_STUCK_RELAY;
	m_EvseState = EVSE_STATE_STUCK_RELAY;
	
	nofault = 0;
      }
    }
  }
#endif // ADVPWR
   
#ifdef GFI
  if (m_Gfi.Fault()) {
    tmpevsestate = EVSE_STATE_GFCI_FAULT;
    m_EvseState = EVSE_STATE_GFCI_FAULT;

    if (m_GfiRetryCnt < GFI_RETRY_COUNT) {
      if (prevevsestate != EVSE_STATE_GFCI_FAULT) {
	if (m_GfiTripCnt < 255) {
	  m_GfiTripCnt++;
	}
 	m_GfiRetryCnt = 0;
	m_GfiTimeout = millis() + GFI_TIMEOUT;
      }
      else if (millis() >= m_GfiTimeout) {
	m_Gfi.Reset();
	m_GfiRetryCnt++;
	m_GfiTimeout = millis() + GFI_TIMEOUT;
      }
    }

    nofault = 0;
  }
#endif // GFI

  if (nofault) {
    if ((prevevsestate == EVSE_STATE_GFCI_FAULT) ||
	(prevevsestate == EVSE_STATE_NO_GROUND)) {
      // just got out of GFCI fault state - pilot back on
      m_Pilot.SetState(PILOT_STATE_P12);
      prevevsestate = EVSE_STATE_UNKNOWN;
      m_EvseState = EVSE_STATE_UNKNOWN;
    }
    // Begin Sensor readings
    int reading;
    plow = 1023;
    phigh = 0;
    //  digitalWrite(3,HIGH);
    // 1x = 114us 20x = 2.3ms 100x = 11.3ms
    for (int i=0;i < 100;i++) {
      reading = analogRead(VOLT_PIN);  // measures pilot voltage

      if (reading > phigh) {
        phigh = reading;
      }
      else if (reading < plow) {
        plow = reading;
      }
    }
    if (DiodeCheckEnabled() && (m_Pilot.GetState() == PILOT_STATE_PWM) && (plow >= m_ThreshData.m_ThreshDS)) {
      // diode check failed
      tmpevsestate = EVSE_STATE_DIODE_CHK_FAILED;
    }
    else if (phigh >= m_ThreshData.m_ThreshAB) {
      // 12V EV not connected
      tmpevsestate = EVSE_STATE_A;
    }
    else if (phigh >= m_ThreshData.m_ThreshBC) {
      // 9V EV connected, waiting for ready to charge
      tmpevsestate = EVSE_STATE_B;
    }
    else if ((phigh  >= m_ThreshData.m_ThreshCD) ||
	     (!VentReqEnabled() && (phigh > m_ThreshData.m_ThreshD))) {
      // 6V ready to charge
      tmpevsestate = EVSE_STATE_C;
    }
    else if (phigh > m_ThreshData.m_ThreshD) {
      // 3V ready to charge vent required
      tmpevsestate = EVSE_STATE_D;
    }
    else {
      tmpevsestate = EVSE_STATE_UNKNOWN;
    }

    // debounce state transitions
    if (tmpevsestate != prevevsestate) {
      if (tmpevsestate != m_TmpEvseState) {
        m_TmpEvseStateStart = millis();
      }
      else if ((millis()-m_TmpEvseStateStart) >= ((tmpevsestate == EVSE_STATE_A) ? DELAY_STATE_TRANSITION_A : DELAY_STATE_TRANSITION)) {
        m_EvseState = tmpevsestate;
      }
    }
  }

  m_TmpEvseState = tmpevsestate;
  
  // state transition
  if (m_EvseState != prevevsestate) {
    if (m_EvseState == EVSE_STATE_A) { // connected, not ready to charge
      chargingOff(); // turn off charging current
      m_Pilot.SetState(PILOT_STATE_P12);
    }
    else if (m_EvseState == EVSE_STATE_B) { // connected 
      chargingOff(); // turn off charging current
      m_Pilot.SetPWM(m_CurrentCapacity);
    }
    else if (m_EvseState == EVSE_STATE_C) {
      m_Pilot.SetPWM(m_CurrentCapacity);
      chargingOn(); // turn on charging current
    }
    else if (m_EvseState == EVSE_STATE_D) {
      // vent required not supported
      chargingOff(); // turn off charging current
      m_Pilot.SetState(PILOT_STATE_P12);
    }
    else if (m_EvseState == EVSE_STATE_GFCI_FAULT) {
      // vehicle state F
      chargingOff(); // turn off charging current
      m_Pilot.SetState(PILOT_STATE_N12);
    }
    else if (m_EvseState == EVSE_STATE_DIODE_CHK_FAILED) {
      chargingOff(); // turn off charging current
      // must leave pilot on so we can keep checking
      m_Pilot.SetPWM(m_CurrentCapacity);
    }
    else if (m_EvseState == EVSE_STATE_NO_GROUND) {
      // Ground not detected
      chargingOff(); // turn off charging current
      m_Pilot.SetState(PILOT_STATE_N12);
    }
    else if (m_EvseState == EVSE_STATE_STUCK_RELAY) {
      // Stuck relay detected
      chargingOff(); // turn off charging current
      m_Pilot.SetState(PILOT_STATE_N12);
    }
    else {
      m_Pilot.SetState(PILOT_STATE_P12);
      chargingOff(); // turn off charging current
    }

#ifdef SERIALCLI
    if (SerDbgEnabled()) {
      g_CLI.print_P(PSTR("state: "));
      Serial.print((int)prevevsestate);
      g_CLI.print_P(PSTR("->"));
      Serial.print((int)m_EvseState);
      g_CLI.print_P(PSTR(" p "));
      Serial.print(plow);
      g_CLI.print_P(PSTR(" "));
      Serial.println(phigh);
    }
#endif
}

  m_PrevEvseState = prevevsestate;

  // End Sensor Readings

  if (m_EvseState == EVSE_STATE_C) {
    m_ElapsedChargeTimePrev = m_ElapsedChargeTime;
    m_ElapsedChargeTime = now() - m_ChargeStartTime;
  }
}

// read ADC values and get min/max/avg for pilot steady high/low states
void J1772EVSEController::Calibrate(PCALIB_DATA pcd)
{
  uint16_t pmax,pmin,pavg,nmax,nmin,navg;

  for (int l=0;l < 2;l++) {
    int reading;
    uint32_t tot = 0;
    uint16_t plow = 1023;
    uint16_t phigh = 0;
    uint16_t avg = 0;
    m_Pilot.SetState(l ? PILOT_STATE_N12 : PILOT_STATE_P12);

    delay(250); // wait for stabilization

    // 1x = 114us 20x = 2.3ms 100x = 11.3ms
    uint8_t pin = PILOT_PIN;
    int i;
    for (i=0;i < 1000;i++) {
      reading = analogRead(VOLT_PIN);  // measures pilot voltage

      if (reading > phigh) {
        phigh = reading;
      }
      else if (reading < plow) {
        plow = reading;
      }

      tot += reading;
    }
    avg = tot / i;

    if (l) {
      nmax = phigh;
      nmin = plow;
      navg = avg;
    }
    else {
      pmax = phigh;
      pmin = plow;
      pavg = avg;
    }
  }
  pcd->m_pMax = pmax;
  pcd->m_pAvg = pavg;
  pcd->m_pMin = pmin;
  pcd->m_nMax = nmax;
  pcd->m_nAvg = navg;
  pcd->m_nMin = nmin;
}


int J1772EVSEController::SetCurrentCapacity(uint8_t amps,uint8_t updatepwm)
{
  int rc = 0;
  if ((amps >= MIN_CURRENT_CAPACITY) && (amps <= ((GetCurSvcLevel() == 1) ? MAX_CURRENT_CAPACITY_L1 : MAX_CURRENT_CAPACITY_L2))) {
    m_CurrentCapacity = amps;
  }
  else {
    m_CurrentCapacity = MIN_CURRENT_CAPACITY;
    rc = 1;
  }

  if (updatepwm && (m_Pilot.GetState() == PILOT_STATE_PWM)) {
    m_Pilot.SetPWM(m_CurrentCapacity);
  }
  return rc;
}


//-- end J1772EVSEController


#ifdef GFI
void Gfi::Init()
{
  Reset();
}

//RESET GFI LOGIC
void Gfi::Reset()
{
#ifdef WATCHDOG
  wdt_reset(); // pat the dog
#endif // WATCHDOG

  m_GfiFault = 0;
}

#endif // GFI

