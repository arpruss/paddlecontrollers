#include <USBComposite.h>
#include "debounce.h"

#define PRODUCT_ID 0x4BA2
#define LED PC13

enum {
  MODE_JOYSTICK = 0,
  MODE_DUAL_JOYSTICK,
  MODE_MOUSE
};

int mode;

#define SERIAL_DEBUG

#define NUM_PADDLES 2
#define HYSTERESIS 16 // shifts smaller than this are rejected
#define MAX_HYSTERESIS_REJECTIONS 8 // unless we've reached this many of them, and then we use an average
#define READ_ITERATIONS 4

#define NO_VALUE 0xDEADBEEFul

class AnalogPort {
public:  
  uint32 port;
  uint32 oldValue = NO_VALUE;
  uint32 rejectedCount = 0;
  uint32 rejectedSum = 0;
  uint32 getValue() {
    
    uint32 v = 0;
    for (uint32 i = 0 ; i < READ_ITERATIONS ; i++) 
      v += analogRead(port);
    v = (v+READ_ITERATIONS/2) / READ_ITERATIONS;
    if (oldValue != NO_VALUE && v != oldValue && v < oldValue + HYSTERESIS && oldValue < v + HYSTERESIS) {
        if (rejectedCount > 0) {
          rejectedCount++;
          rejectedSum += v;
          if (rejectedCount >= MAX_HYSTERESIS_REJECTIONS) {
            v = (rejectedSum + MAX_HYSTERESIS_REJECTIONS/2) / MAX_HYSTERESIS_REJECTIONS;
            rejectedCount = 0;
          }
          else {
            v = oldValue;
          }
        }
        else {
          rejectedCount = 1;
          rejectedSum = v;
          v = oldValue;
        }
    }
    else {
      rejectedCount = 0;
    }
    
    oldValue = v;
    return 4095-v;
  };

  AnalogPort(uint32 _port) {
    port = _port;    
  };
};

AnalogPort analog1(PA1);
AnalogPort analog2(PA3);
AnalogPort* analog[NUM_PADDLES] = { &analog1, &analog2 };
Debounce digital1(PA2);
Debounce digital2(PA4);
Debounce* digital[NUM_PADDLES] = { &digital1, &digital2 };
const uint32 mouseButtons[2] = { MOUSE_LEFT, MOUSE_RIGHT };

USBHID HID;
HIDJoystick joy1(HID);
HIDJoystick joy2(HID);
HIDJoystick* joys[2] = { &joy1, &joy2 };
HIDAbsMouse mouse(HID);

void setup(){
  for (uint32 i = 0 ; i < NUM_PADDLES ; i++) {
    pinMode(analog[i]->port, INPUT_ANALOG);
    pinMode(digital[i]->pin, INPUT_PULLDOWN);
  }
#if NUM_PADDLES == 1
  mode = digitalRead(digital[0]->pin) ? MODE_MOUSE : MODE_JOYSTICK;
#else  
  EEPROM8_init();
  mode = EEPROM8_getValue(0);
  if (mode < 0)
    mode = MODE_JOYSTICK;  
  uint32 a = digitalRead(digital[0]->pin);
  uint32 b = digitalRead(digital[1]->pin);
  if (a && !b) {
    mode = MODE_JOYSTICK;
    EEPROM8_storeValue(0,mode);
  }
  else if (!a && b) {
    mode = MODE_MOUSE;
    EEPROM8_storeValue(0,mode);
  }
  else if (a && b) {
    mode = MODE_DUAL_JOYSTICK;
    EEPROM8_storeValue(0,mode);
  }
#endif
  pinMode(LED, OUTPUT);
  digitalWrite(LED, mode != MODE_MOUSE);
  HID.clear();
  USBComposite.setProductId(PRODUCT_ID+mode);
  if (mode == MODE_JOYSTICK) {
    USBComposite.setProductString("Paddle Joystick");
    joy1.registerProfile();
    joy1.setManualReportMode(true);
  }
  else if (mode == MODE_DUAL_JOYSTICK) {
    USBComposite.setProductString("Paddle Dual Joystick");
    for (uint32 i=0; i<NUM_PADDLES; i++) {
      joys[i]->registerProfile();
      joys[i]->setManualReportMode(true);
    }
  }
  else {
    USBComposite.setProductString("Paddle Mouse");
    mouse.registerProfile();
  }
  HID.begin();
  while (!USBComposite);
}

void loop(){
  uint32 pots[NUM_PADDLES];

  for (uint32 i = 0 ; i < NUM_PADDLES ; i++ ) {
    pots[i] = analog[i]->getValue();
    uint32 b = digital[i]->getEvent();
    if (b != DEBOUNCE_NONE) {
      if (mode == MODE_JOYSTICK)
        joy1.button(i+1,b==DEBOUNCE_PRESSED);
      else if (mode == MODE_DUAL_JOYSTICK) {
        joys[i]->button(1,b==DEBOUNCE_PRESSED);
      }
      else {
        if (b == DEBOUNCE_PRESSED)
          mouse.press(mouseButtons[i]);
        else
          mouse.release(mouseButtons[i]);
      }
    }
  }
  if (mode == MODE_JOYSTICK) {
    joy1.X(pots[0] / 4);
#if NUM_PADDLES > 1  
    joy1.Y(pots[1] / 4);
#endif  
    joy1.sendReport();
  }
  else if (mode == MODE_DUAL_JOYSTICK) {
    for(uint32 i=0 ; i<2 ; i++) {
      uint16 v = pots[i] / 4;
      joys[i]->X(v);
      joys[i]->Y(v);
      joys[i]->sendReport();
    }
  }
  else {
    mouse.move(32767*pots[0]/4095,32767*pots[1]/4095);
  }
}

