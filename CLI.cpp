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

#include "Config.h"
#include "CLI.h"
#include "Open_EVSE.h"

extern J1772EVSEController g_EvseController;
extern char g_sTmp[64];
extern prog_char VERSTR[];
extern void SaveSettings();

#ifdef SERIALCLI
CLI g_CLI;
prog_char g_psEnabled[] PROGMEM = "enabled";
prog_char g_psDisabled[] PROGMEM = "disabled";

CLI::CLI()
{
  m_CLIstrCount = 0; 
  m_strBuf = g_sTmp;
  m_strBufLen = sizeof(g_sTmp);
}

void CLI::info()
{
  println_P(PSTR("Open EVSE")); // CLI print prompt when serial is ready
  println_P(PSTR("Hardware - Atmel ATMEGA328P-AU")); //CLI Info
  println_P(PSTR("Software - Open EVSE ")); //CLI info
  println_P(VERSTR);
  printlnn();
}

void CLI::setup()
{
  Serial.begin(SERIAL_BAUD);
}

void CLI::Init()
{
  info();
  println_P(PSTR("type ? or help for command list"));
  print_P(PSTR("Open_EVSE> ")); // CLI Prompt
  flush();

}

uint8_t CLI::getInt()
{
  uint8_t c;
  uint8_t num = 0;

  do {
    c = Serial.read(); // read the byte
    if ((c >= '0') && (c <= '9')) {
      num = (num * 10) + c - '0';
    }
  } while (c != 13);
  return num;
}

void CLI::printlnn()
{
  println("");
}

prog_char g_pson[] PROGMEM = "on";
void CLI::getInput()
{
  int currentreading;
  uint8_t amp;
  if (Serial.available()) { // if byte(s) are available to be read
    char inbyte = (char) Serial.read(); // read the byte
    Serial.print(inbyte);
    if (inbyte != 13) { // CR
      if (((inbyte >= 'a') && (inbyte <= 'z')) || ((inbyte >= '0') && (inbyte <= '9') || (inbyte == ' ')) ) {
	m_CLIinstr[m_CLIstrCount] = inbyte;
	m_CLIstrCount++;
      }
      else if (m_CLIstrCount && ((inbyte == 8) || (inbyte == 127))) {
	m_CLIstrCount--;
      }
    }

    if ((inbyte == 13) || (m_CLIstrCount == CLI_BUFLEN-1)) { // if enter was pressed or max chars reached
      m_CLIinstr[m_CLIstrCount] = '\0';
      printlnn(); // print a newline
      if (strcmp_P(m_CLIinstr, PSTR("show")) == 0){ //if match SHOW 
        info();
        
        println_P(PSTR("Settings"));
	print_P(PSTR("Service level: "));
	Serial.println((int)g_EvseController.GetCurSvcLevel()); 
        print_P(PSTR("Current capacity (Amps): "));
        Serial.println((int)g_EvseController.GetCurrentCapacity()); 
        print_P(PSTR("Min Current Capacity: "));
        Serial.println(MIN_CURRENT_CAPACITY);
        print_P(PSTR("Max Current Capacity: "));
        Serial.println(MAX_CURRENT_CAPACITY_L2);
	print_P(PSTR("Vent Required: "));
	println_P(g_EvseController.VentReqEnabled() ? g_psEnabled : g_psDisabled);
         print_P(PSTR("Diode Check: "));
	println_P(g_EvseController.DiodeCheckEnabled() ? g_psEnabled : g_psDisabled);

#ifdef ADVPWR
	print_P(PSTR("Ground Check: "));
	println_P(g_EvseController.GndChkEnabled() ? g_psEnabled : g_psDisabled);
	print_P(PSTR("Stuck Relay Check: "));
	println_P(g_EvseController.StuckRelayChkEnabled() ? g_psEnabled : g_psDisabled);
#endif // ADVPWR           
      } 
      else if ((strcmp_P(m_CLIinstr, PSTR("help")) == 0) || (strcmp_P(m_CLIinstr, PSTR("?")) == 0)){ // string compare
        println_P(PSTR("Help Commands"));
        printlnn();
        println_P(PSTR("help - Display commands")); // print to the terminal
        println_P(PSTR("set  - Change Settings"));
        println_P(PSTR("show - Display settings and values"));
        println_P(PSTR("save - Write settings to EEPROM"));
      } 
      else if (strcmp_P(m_CLIinstr, PSTR("set")) == 0) { // string compare
        println_P(PSTR("Set Commands - Usage: set amp"));
        printlnn();
        println_P(PSTR("amp  - Set EVSE Current Capacity")); // print to the terminal
	println_P(PSTR("vntreq on/off - enable/disable vent required state"));
        println_P(PSTR("diochk on/off - enable/disable diode check"));

#ifdef ADVPWR
	println_P(PSTR("gndchk on/off - turn ground check on/off"));
	println_P(PSTR("rlychk on/off - turn stuck relay check on/off"));
#endif // ADVPWR
	println_P(PSTR("sdbg on/off - turn serial debugging on/off"));
      }
      else if (strncmp_P(m_CLIinstr, PSTR("set "),4) == 0) {
	char *p = m_CLIinstr + 4;
	if (!strncmp_P(p,PSTR("sdbg "),5)) {
	  p += 5;
	  print_P(PSTR("serial debugging "));
	  if (!strcmp_P(p,g_pson)) {
	    g_EvseController.EnableSerDbg(1);
	    println_P(g_psEnabled);
	  }
	  else {
	    g_EvseController.EnableSerDbg(0);
	    println_P(g_psDisabled);
	  }
	}
	else if (!strncmp_P(p,PSTR("vntreq "),7)) {
	  p += 7;
	  print_P(PSTR("vent required "));
	  if (!strcmp_P(p,g_pson)) {
	    g_EvseController.EnableVentReq(1);
	    println_P(g_psEnabled);
	  }
	  else {
	    g_EvseController.EnableVentReq(0);
	    println_P(g_psDisabled);
	  }
	}
            else if (!strncmp_P(p,PSTR("diochk "),7)) {
	  p += 7;
	  print_P(PSTR("diode check "));
	  if (!strcmp_P(p,g_pson)) {
	    g_EvseController.EnableDiodeCheck(1);
	    println_P(g_psEnabled);
	  }
	  else {
	    g_EvseController.EnableDiodeCheck(0);
	    println_P(g_psDisabled);
	  }
	}
#ifdef ADVPWR
	else if (!strncmp_P(p,PSTR("gndchk "),7)) {
	  p += 7;
	  print_P(PSTR("ground check "));
	  if (!strcmp_P(p,g_pson)) {
	    g_EvseController.EnableGndChk(1);
	    println_P(g_psEnabled);
	  }
	  else {
	    g_EvseController.EnableGndChk(0);
	    println_P(g_psDisabled);
	  }
	}
	else if (!strncmp_P(p,PSTR("rlychk "),7)) {
	  p += 7;
	  print_P(PSTR("stuck relay check "));
	  if (!strcmp_P(p,g_pson)) {
	    g_EvseController.EnableStuckRelayChk(1);
	    println_P(g_psEnabled);
	  }
	  else {
	    g_EvseController.EnableStuckRelayChk(0);
	    println_P(g_psDisabled);
	  }
	}
#endif // ADVPWR
	else if (!strcmp_P(p,PSTR("amp"))){ // string compare
	  println_P(PSTR("WARNING - DO NOT SET CURRENT HIGHER THAN 80%"));
	  println_P(PSTR("OF YOUR CIRCUIT BREAKER OR")); 
	  println_P(PSTR("GREATER THAN THE RATED VALUE OF THE EVSE"));
	  printlnn();
	  print_P(PSTR("Enter amps ("));
	  Serial.print(MIN_CURRENT_CAPACITY);
	  print_P(PSTR("-"));
	  Serial.print((g_EvseController.GetCurSvcLevel()  == 1) ? MAX_CURRENT_CAPACITY_L1 : MAX_CURRENT_CAPACITY_L2);
	  print_P(PSTR("): "));
	  amp = getInt();
	  Serial.println((int)amp);
	  if(g_EvseController.SetCurrentCapacity(amp,1)) {
	    println_P(PSTR("Invalid Current Capacity"));
	  }
	  
	  print_P(PSTR("Current Capacity now: ")); // print to the terminal
	  Serial.print((int)g_EvseController.GetCurrentCapacity());
	  print_P(PSTR("A"));
	} 
	else {
	  goto unknown;
	}
      }
      else if (strcmp_P(m_CLIinstr, PSTR("save")) == 0){ // string compare
        println_P(PSTR("Saving Settings to EEPROM")); // print to the terminal
        SaveSettings();
      } 
      else { // if the input text doesn't match any defined above
      unknown:
        println_P(PSTR("Unknown Command -- type ? or help for command list")); // echo back to the terminal
      } 
      printlnn();
      printlnn();
      print_P(PSTR("Open_EVSE> "));
      flush();
      m_CLIstrCount = 0; // get ready for new input... reset strCount
      m_CLIinstr[0] = '\0'; // set to null to erase it
    }
  }
}

void CLI::println_P(prog_char *s)
{
  strncpy_P(m_strBuf,s,m_strBufLen);
  println(m_strBuf);
}

void CLI::print_P(prog_char *s)
{
  strncpy_P(m_strBuf,s,m_strBufLen);
  print(m_strBuf);
}

#endif // SERIALCLI

