#ifndef Config_h
#define Config_h

#define VERSION "1.0.9"


//-- begin features

//Adafruit RGBLCD
//#define RGBLCD

// Adafruit LCD backpack in I2C mode
//#define I2CLCD

#if defined(RGBLCD) || defined(I2CLCD)
#define LCD_I2C_ADDR 0x20 // for adafruit shield or backpack
#endif // RGBLCD || I2CLCD

// NewHavenDisplay I2C LCD
#define NHDLCD
#ifdef NHDLCD
#define LCD_I2C_ADDR 0x50
#endif // NHDLCD

#if defined(RGBLCD) || defined(I2CLCD) || defined(NHDLCD)
#define LCD16X2
#endif // RGBLCD || I2CLCD || NHDLCD

// enable watchdog timer
//#define WATCHDOG

// GFI support
#define GFI

// for stability testing - shorter timeout/higher retry count
//#define GFI_TESTING

// Advanced Powersupply... Ground check, stuck relay, L1/L2 detection.
#define ADVPWR

// Serial interface
//#define SERIALCLI

// 1 button menu
#ifdef LCD16X2
#define BTN_MENU
#endif // LCD16X2

//-- end features

// n.b. DEFAULT_SERVICE_LEVEL is ignored if ADVPWR defined, since it's autodetected
#define DEFAULT_SERVICE_LEVEL 1 // 1=L1, 2=L2

// current capacity in amps
#define DEFAULT_CURRENT_CAPACITY_L1 12
#define DEFAULT_CURRENT_CAPACITY_L2 16

// minimum allowable current in amps
#define MIN_CURRENT_CAPACITY 6

// maximum allowable current in amps
#define MAX_CURRENT_CAPACITY_L1 16 // J1772 Max for L1 on a 20A circuit
#define MAX_CURRENT_CAPACITY_L2 80 // J1772 Max for L2

//J1772EVSEController
//#define CURRENT_PIN 0 // analog current reading pin A0
#define VOLT_PIN 1 // analog voltage reading pin A1
#define ACLINE1_PIN 3 // TEST PIN 1 for L1/L2, ground and stuck relay
#define ACLINE2_PIN 4 // TEST PIN 2 for L1/L2, ground and stuck relay
#define RED_LED_PIN 5 // Digital pin
#define CHARGING_PIN 8 // digital Charging LED and Relay Trigger pin
#define PILOT_PIN 10 // n.b. PILOT_PIN *MUST* be digital 10 because SetPWM() assumes it
#define GREEN_LED_PIN 13 // Digital pin


// EEPROM offsets for settings
#define EOFS_CURRENT_CAPACITY_L1 0 // 1 byte
#define EOFS_CURRENT_CAPACITY_L2 1 // 1 byte
#define EOFS_FLAGS               2 // 1 byte

// must stay within thresh for this time in ms before switching states
#define DELAY_STATE_TRANSITION 250
// must transition to state A from contacts closed in < 100ms according to spec
// but Leaf sometimes bounces from 3->1 so we will debounce it a little anyway
#define DELAY_STATE_TRANSITION_A 25

// for ADVPWR
#define GROUND_CHK_DELAY  1000 // delay after charging started to test, ms
#define STUCK_RELAY_DELAY 1000 // delay after charging opened to test, ms

#ifdef GFI
#define GFI_INTERRUPT 0 // interrupt number 0 = D2, 1 = D3

#ifdef GFI_TESTING
#define GFI_TIMEOUT ((unsigned long)(15*1000))
#define GFI_RETRY_COUNT  255
#else // !GFI_TESTING
#define GFI_TIMEOUT ((unsigned long)(15*60000)) // 15*60*1000 doesn't work. go figure
#define GFI_RETRY_COUNT  3
#endif // GFI_TESTING
#endif // GFI


#endif // Config_h


