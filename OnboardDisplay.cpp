// File comment
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

#include "OnboardDisplay.h"
#include "Open_EVSE.h"

extern J1772EVSEController g_EvseController;

OnboardDisplay g_OBD;
prog_char VERSTR[] PROGMEM = VERSION;
char g_sTmp[64];

#ifdef LCD16X2
char *g_BlankLine = "                ";
#endif // LCD16X2

#ifdef LCD16X2
#ifdef ADVPWR
prog_char g_psPwrOn[] PROGMEM = "Power On";
prog_char g_psSelfTest[] PROGMEM = "Self Test";
prog_char g_psAutoDetect[] PROGMEM = "Auto Detect";
prog_char g_psLevel1[] PROGMEM = "Svc Level: L1";
prog_char g_psLevel2[] PROGMEM = "Svc Level: L2";
//prog_char g_psStuckRelay[] PROGMEM = "--Stuck Relay--";
//prog_char g_psEarthGround[] PROGMEM = "--Earth Ground--";
//prog_char g_psTestPassed[] PROGMEM = "Test Passed";
//prog_char g_psTestFailed[] PROGMEM = "TEST FAILED";
#endif // ADVPWR
prog_char g_psEvseError[] PROGMEM =  "EVSE ERROR";
prog_char g_psVentReq[] PROGMEM = "VENT REQUIRED";
prog_char g_psDiodeChkFailed[] PROGMEM = "DIODE CHK FAILED";
prog_char g_psGfciFault[] PROGMEM = "GFCI FAULT";
prog_char g_psNoGround[] PROGMEM = "NO GROUND";
prog_char g_psEStuckRelay[] PROGMEM = "STUCK RELAY";
prog_char g_psStopped[] PROGMEM = "Stopped";
prog_char g_psEvConnected[] PROGMEM = "EV Connected";
prog_char g_psEvNotConnected[] PROGMEM = "EV Not Connected";
#endif // LCD16X2



OnboardDisplay::OnboardDisplay()
#if defined(I2CLCD) || defined(RGBLCD)
  : m_Lcd(LCD_I2C_ADDR,1)
#endif
#ifdef NHDLCD
  : m_Lcd(2, 16, LCD_I2C_ADDR>>1, 0)
#endif
{
  m_strBuf = g_sTmp;
} 


void OnboardDisplay::Init()
{
  pinMode (GREEN_LED_PIN, OUTPUT);
  pinMode (RED_LED_PIN, OUTPUT);

  SetGreenLed(LOW);
  SetRedLed(LOW);
  
#ifdef LCD16X2
  LcdBegin(16, 2);
 
  LcdPrint_P(0,PSTR("Open EVSE       "));
  delay(500);
  LcdPrint_P(0,1,PSTR("Version "));
  LcdPrint_P(VERSTR);
  LcdPrint_P(PSTR("   "));
  delay(500);
#endif // LCD16X2
}


void OnboardDisplay::SetGreenLed(uint8_t state)
{
  digitalWrite(GREEN_LED_PIN,state);
}

void OnboardDisplay::SetRedLed(uint8_t state)
{
  digitalWrite(RED_LED_PIN,state);
}

#ifdef LCD16X2
void OnboardDisplay::LcdPrint(const char *s)
{
    m_Lcd.print(s);
}

void OnboardDisplay::LcdBegin(int x,int y)
{ 
#ifdef I2CLCD
    m_Lcd.setMCPType(LTI_TYPE_MCP23008);
    m_Lcd.begin(x,y); 
    m_Lcd.setBacklight(HIGH);
#endif // I2CLCD
#ifdef RGBLCD
    m_Lcd.setMCPType(LTI_TYPE_MCP23017);
    m_Lcd.begin(x,y); 
    m_Lcd.setBacklight(WHITE);
#endif // RGBLCD
#ifdef NHDLCD
    m_Lcd.init();
    m_Lcd.setBacklight(0);
#endif // NHDLCD 
}

void OnboardDisplay::LcdPrint(int x,int y,const char *s)
{ 
    LcdSetCursor(x,y);
    m_Lcd.print(s); 
}

void OnboardDisplay::LcdPrint_P(const prog_char *s)
{
  strcpy_P(m_strBuf,s);
  m_Lcd.print(m_strBuf);
}

void OnboardDisplay::LcdPrint_P(int y,const prog_char *s)
{
  strcpy_P(m_strBuf,s);
  LcdPrint(y,m_strBuf);
}

void OnboardDisplay::LcdPrint_P(int x,int y,const prog_char *s)
{
  strcpy_P(m_strBuf,s);
  LcdPrint(x,y,m_strBuf);
}

void OnboardDisplay::LcdSetCursor(int x,int y)
{
#ifdef NHDLCD
m_Lcd.setCursor(y,x);
#else
m_Lcd.setCursor(x,y);
#endif // NHDLCD
}

void OnboardDisplay::LcdMsg_P(const prog_char *l1,const prog_char *l2)
{
  LcdPrint_P(0,l1);
  LcdPrint_P(1,l2);
}

void OnboardDisplay::LcdSetBacklightColor(uint8_t c) {
#ifdef RGBLCD
    m_Lcd.setBacklight(c);
#endif // RGBLCD
  }

void OnboardDisplay::LcdClearLine(int y) {
  LcdSetCursor(0,y);
  m_Lcd.print(g_BlankLine);
}
  
  
// print at (0,y), filling out the line with trailing spaces
void OnboardDisplay::LcdPrint(int y,const char *s)
{
  LcdSetCursor(0,y);
  char ss[25];
  sprintf(ss,"%-16s",s);
  ss[16] = '\0';
  m_Lcd.print(ss);
}

void OnboardDisplay::LcdMsg(const char *l1,const char *l2)
{
  LcdPrint(0,l1);
  LcdPrint(1,l2);
}
#endif // LCD16X2

char g_sRdyLAstr[] = "Ready     L%d:%dA";
void OnboardDisplay::Update()
{
  uint8_t curstate = g_EvseController.GetState();
  uint8_t svclvl = g_EvseController.GetCurSvcLevel();
  uint8_t curcap = g_EvseController.GetCurrentCapacity();

  if (g_EvseController.StateTransition()) {
  switch(curstate) {
    case EVSE_STATE_A: // not connected
      SetGreenLed(HIGH);
      SetRedLed(LOW);
      #ifdef LCD16X2 //Adafruit RGB LCD
      LcdSetBacklightColor(GREEN);
      sprintf(g_sTmp,g_sRdyLAstr,(int)svclvl,(int)curcap);
      LcdPrint(0,g_sTmp);
      LcdPrint_P(1,g_psEvNotConnected);
      #endif //Adafruit RGB LCD
      // n.b. blue LED is off
      break;
    case EVSE_STATE_B: // connected/not charging
      SetGreenLed(HIGH);
      SetRedLed(HIGH);
      #ifdef LCD16X2 //Adafruit RGB LCD
      LcdSetBacklightColor(YELLOW);
      sprintf(g_sTmp,g_sRdyLAstr,(int)svclvl,(int)curcap);
      LcdPrint(0,g_sTmp);
      LcdPrint_P(1,g_psEvConnected);
      #endif //Adafruit RGB LCD
      // n.b. blue LED is off
      break;
    case EVSE_STATE_C: // charging
      SetGreenLed(LOW);
      SetRedLed(LOW);
      #ifdef LCD16X2 //Adafruit RGB LCD
      LcdSetBacklightColor(BLUE);
      sprintf(g_sTmp,"Charging  L%d:%dA",(int)svclvl,(int)curcap);
      LcdPrint(0,g_sTmp);
      #endif //Adafruit RGB LCD
      // n.b. blue LED is on
      break;
    case EVSE_STATE_D: // vent required
      SetGreenLed(LOW);
      SetRedLed(HIGH);
      #ifdef LCD16X2 //Adafruit RGB LCD
      LcdSetBacklightColor(RED);
      LcdMsg_P(g_psEvseError,g_psVentReq);
      #endif //Adafruit RGB LCD
      // n.b. blue LED is off
      break;
    case EVSE_STATE_DIODE_CHK_FAILED:
      SetGreenLed(LOW);
      SetRedLed(HIGH);
      #ifdef LCD16X2 //Adafruit RGB LCD
      LcdSetBacklightColor(RED);
      LcdMsg_P(g_psEvseError,g_psDiodeChkFailed);
      #endif //Adafruit RGB LCD
      // n.b. blue LED is off
      break;
    case EVSE_STATE_GFCI_FAULT:
      SetGreenLed(LOW);
      SetRedLed(HIGH);
      #ifdef LCD16X2 //Adafruit RGB LCD
      LcdSetBacklightColor(RED);
      LcdMsg_P(g_psEvseError,g_psGfciFault);
      #endif //Adafruit RGB LCD
      // n.b. blue LED is off
      break;
     case EVSE_STATE_NO_GROUND:
      SetGreenLed(LOW);
      SetRedLed(HIGH);
      #ifdef LCD16X2 //Adafruit RGB LCD
      LcdSetBacklightColor(RED);
      LcdMsg_P(g_psEvseError,g_psNoGround);
      #endif //Adafruit RGB LCD
      // n.b. blue LED is off
      break;
     case EVSE_STATE_STUCK_RELAY:
      SetGreenLed(LOW);
      SetRedLed(HIGH);
      #ifdef LCD16X2 //Adafruit RGB LCD
      LcdSetBacklightColor(RED);
      LcdMsg_P(g_psEvseError,g_psEStuckRelay);
      #endif //Adafruit RGB LCD
      // n.b. blue LED is off
      break;
    case EVSE_STATE_DISABLED:
      SetGreenLed(LOW);
      SetRedLed(HIGH);
#ifdef LCD16X2
      LcdPrint_P(0,g_psStopped);
#endif // LCD16X2
      break;
    default:
      SetGreenLed(LOW);
      SetRedLed(HIGH);
      // n.b. blue LED is off
  }
}
#ifdef LCD16X2
  if (curstate == EVSE_STATE_C) {
    time_t elapsedTime = g_EvseController.GetElapsedChargeTime();
    if (elapsedTime != g_EvseController.GetElapsedChargeTimePrev()) {   
      int h = hour(elapsedTime);
      int m = minute(elapsedTime);
      int s = second(elapsedTime);
      sprintf(g_sTmp,"%02d:%02d:%02d",h,m,s);
      LcdPrint(1,g_sTmp);
    }
  }
#endif
}



