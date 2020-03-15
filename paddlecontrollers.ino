#include <USBComposite.h>
#include "debounce.h"

#define JOYSTICK
#define SERIAL_DEBUG

#define NUM_PADDLES 2
#define HYSTERESIS 4
#define READ_ITERATIONS 4

#define NO_VALUE 0xDEADBEEFul

class AnalogPort {
public:  
  uint32 port;
  uint32 oldValue;
  uint32 getValue() {
    
    uint32 v = 0;
    for (uint32 i = 0 ; i < READ_ITERATIONS ; i++) 
      v += analogRead(port);
    v = (v+READ_ITERATIONS/2) / READ_ITERATIONS;
    if (oldValue != NO_VALUE) {
      if (v < oldValue + HYSTERESIS && oldValue < v + HYSTERESIS)
        v = oldValue;
    }
    oldValue = v;
    return 4095-v;
  };
};

AnalogPort analog[NUM_PADDLES] = { { PA1, NO_VALUE }, { PA3, NO_VALUE } };
Debounce digital1(PA2);
Debounce digital2(PA4);
Debounce digital[NUM_PADDLES] = { digital1, digital2 };
const uint32 mouseButtons[2] = { MOUSE_LEFT, MOUSE_RIGHT };

const uint8_t reportDescription[] = {
  HID_ABS_MOUSE_REPORT_DESCRIPTOR(HID_MOUSE_REPORT_ID)
};

USBHID HID;
#ifdef JOYSTICK
HIDJoystick joy(HID);
#else
HIDAbsMouse mouse(HID);
#endif

void setup(){
  for (uint32 i = 0 ; i < NUM_PADDLES ; i++) {
    pinMode(analog[i].port, INPUT_ANALOG);
    pinMode(digital[i].pin, INPUT_PULLDOWN);
  }
#ifdef JOYSTICK
  HID.begin();
  joy.setManualReportMode(true);
#else  
  HID.begin(reportDescription, sizeof(reportDescription));
#endif  
  while (!USBComposite);
  //delay(1000);
/*  mouse.press(MOUSE_LEFT);
  mouse.move(500,500);
  mouse.release(MOUSE_ALL);
  mouse.click(MOUSE_RIGHT); */
}

void loop(){
  uint32 pots[NUM_PADDLES];

  for (uint32 i = 0 ; i < NUM_PADDLES ; i++ ) {
    pots[i] = analog[i].getValue();
    uint32 b = digital[i].getEvent();
    if (b != DEBOUNCE_NONE) {
#ifdef JOYSTICK    
      joy.button(i+1,b==DEBOUNCE_PRESSED);
#else
      if (b == DEBOUNCE_PRESSED)
        mouse.press(mouseButtons[i]);
      else
        mouse.release(mouseButtons[i]);
#endif      
    }
  }
#ifdef JOYSTICK
  joy.X(pots[0] / 4);
#if NUM_PADDLES > 1  
  joy.Y(pots[1] / 4);
#endif  
  joy.sendReport();
#else  
  mouse.move(32767*pots[0]/4095,32767*pots[1]/4095);
#endif
}

