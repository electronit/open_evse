#ifndef OnboardDisplay_h
#define OnboardDisplay_h

#include <stdint.h>
#include "Config.h"

// for RGBLCD
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define BLUE 0x6

#if defined(RGBLCD) || defined(I2CLCD)
// Using LiquidTWI2 for both types of I2C LCD's
// see http://blog.lincomatic.com/?p=956 for installation instructions
#include <Wire.h>
#include <LiquidTWI2.h>
#endif // RGBLCD || I2CLCD

#ifdef NHDLCD
#include <LCDi2cNHD.h>
#endif // NHDLCD

class OnboardDisplay
{
#if defined(RGBLCD) || defined(I2CLCD)
LiquidTWI2 m_Lcd;
#endif
#if defined(NHDLCD)
LCDi2cNHD m_Lcd;
#endif

char *m_strBuf;
time_t m_backlightTimer;

public:
  OnboardDisplay();
  
  void Init();
  void SetGreenLed(uint8_t state);
  void SetRedLed(uint8_t state);
#ifdef LCD16X2
  void LcdBegin(int x,int y);
  void LcdPrint(const char *s);
  void LcdPrint_P(const prog_char *s);
  void LcdPrint(int y,const char *s);
  void LcdPrint_P(int y,const prog_char *s);
  void LcdPrint(int x,int y,const char *s);
  void LcdPrint_P(int x,int y,const prog_char *s);
  void LcdPrint(int i) {   m_Lcd.print(i); }
  void LcdSetCursor(int x,int y);
  void LcdClearLine(int y);
  void LcdClear() {  m_Lcd.clear(); }
  void LcdMsg(const char *l1,const char *l2);
  void LcdMsg_P(const prog_char *l1,const prog_char *l2);
  void LcdSetBacklightColor(uint8_t c);
#ifdef RGBLCD
  uint8_t readButtons() { return m_Lcd.readButtons(); }
#endif // RGBLCD
#endif // LCD16X2

  void Update();
};

#endif // OnboardDisplay_h
