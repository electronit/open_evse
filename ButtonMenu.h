// single button menus (needs LCD enabled)
// connect an SPST button between BTN_PIN and GND or enable ADAFRUIT_BTN to use the 
// select button of the Adafruit RGB LCD 
// How to use 1-button menu
// Long press activates menu
// When within menus, short press cycles menu items, long press selects and exits current submenu
#ifndef ButtonMenu_h
#define ButtonMenu_h

#define BTN_PIN A3 // button sensing pin
#define BTN_PRESS_SHORT 100  // ms
#define BTN_PRESS_LONG 500 // ms
#define BTN_STATE_OFF   0
#define BTN_STATE_SHORT 1 // short press
#define BTN_STATE_LONG  2 // long press


// use Adafruit RGB LCD select button
#ifdef RGBLCD
#define ADAFRUIT_BTN
#endif // RGBLCD


// When not in menus, short press instantly stops the EVSE - another short press resumes.  Long press activates menus
// also allows menus to be manipulated even when in State B/C
//#define BTN_ENABLE_TOGGLE

class Btn {
  uint8_t buttonState;
  long lastDebounceTime;  // the last time the output pin was toggled
  
public:
  Btn();
  void init();

  void read();
  uint8_t shortPress();
  uint8_t longPress();
};


typedef enum {MS_NONE,MS_SETUP,MS_SVC_LEVEL,MS_MAX_CURRENT,MS_RESET} MENU_STATE;

class Menu {
public:
  prog_char *m_Title;
  uint8_t m_CurIdx;
  
  void init(const char *firstitem);

  Menu();

  virtual void Init() = 0;
  virtual void Next() = 0;
  virtual Menu *Select() = 0;
};

class SetupMenu : public Menu {
public:
  SetupMenu();
  void Init();
  void Next();
  Menu *Select();
};

class SvcLevelMenu : public Menu {
public:
  SvcLevelMenu();
  void Init();
  void Next();
  Menu *Select();
};

class MaxCurrentMenu  : public Menu {
  uint8_t m_MaxCurrent;
  uint8_t m_MaxIdx;
  uint8_t *m_MaxAmpsList;
public:
  MaxCurrentMenu();
  void Init();
  void Next();
  Menu *Select();
};

class DiodeChkMenu : public Menu {
public:
  DiodeChkMenu();
  void Init();
  void Next();
  Menu *Select();
};


class VentReqMenu : public Menu {
public:
  VentReqMenu();
  void Init();
  void Next();
  Menu *Select();
};


#ifdef ADVPWR
class GndChkMenu : public Menu {
public:
  GndChkMenu();
  void Init();
  void Next();
  Menu *Select();
};
#endif // ADVPWR


class ResetMenu : public Menu {
public:
  ResetMenu();
  void Init();
  void Next();
  Menu *Select();
};


class BtnHandler {
  Btn m_Btn;
  Menu *m_CurMenu;
public:
  BtnHandler();
  void init() { m_Btn.init(); }
  void ChkBtn();
};

#endif

