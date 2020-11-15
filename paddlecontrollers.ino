#include <USBComposite.h>
#include "debounce.h"

#define PRODUCT_ID 0x4BA2
#define LED PC13
#define ANALOG1 PA1
#define ANALOG2 PA3
#undef SUPPORT_X360 // not fully supported
#undef SERIAL_DEBUG

// at READ_ITERATIONS=1, 12 ms
// 20: 12 ms
// 40: 12 ms
// 80: 12 ms
// 100: 16 ms
// 800: 36 ms

#define NUM_EXTRA 4  // number of extra keys that emulate keyboard presses
#if NUM_EXTRA
const uint16 extraKeys[NUM_EXTRA] = { KEY_F1, KEY_F2, '[', ']' };
#endif
#define NUM_PADDLES 2
#define HYSTERESIS 20 // shifts smaller than this are rejected
#define MAX_HYSTERESIS_REJECTIONS 8 // unless we've reached this many of them, and then we use an average
#define READ_ITERATIONS 80

#define NO_VALUE 0xDEADBEEFul


// modified from Stelladaptor
uint8 dualAxis_desc[] = {
  0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
  0x15, 0x00,                    // LOGICAL_MINIMUM (0)
  0x09, 0x04,                    // USAGE (Joystick)
  0xa1, 0x01,                    // COLLECTION (Application)
  0x85, 1,                       /*    REPORT_ID */ // not present in official Stelladaptor
  0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
  0x26, 0xff, 0x03,              //   LOGICAL_MAXIMUM (1023) // 255 for official Stelladaptor
  0x75, 0x0A,                    //   REPORT_SIZE (10) // 8 for official Stelladaptor
  //0x95, 0x01,                    //   REPORT_COUNT (1) /* byte 0 unused */ // official Stelladaptor
  //0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)  // official Stelladaptor
  0x05, 0x01,                    //   USAGE_PAGE (Generic Desktop)
  0x09, 0x01,                    //   USAGE (Pointer)
  0xa1, 0x00,                    //   COLLECTION (Physical)
  0x09, 0x30,                    //     USAGE (X)
  0x09, 0x31,                    //     USAGE (Y)
  0x95, 0x02,                    //     REPORT_COUNT (2)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0xc0,                          //     END_COLLECTION
  0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
  0x05, 0x09,                    //   USAGE_PAGE (Button)
  0x19, 0x01,                    //   USAGE_MINIMUM (Button 1)
  0x29, 0x02,                    //   USAGE_MAXIMUM (Button 2)
  0x55, 0x00,                    //   UNIT_EXPONENT (0)
  0x65, 0x00,                    //   UNIT (None)
  0x75, 0x01,                    //   REPORT_SIZE (1)
  0x95, 0x02,                    //   REPORT_COUNT (2)
  0x81, 0x02,                    //   INPUT (Data,Var,Abs)
  0x95, 0x02,                    //   REPORT_COUNT (2) // 6 for official Stelladaptor
  0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
  0xc0                           // END_COLLECTION
};

HIDReportDescriptor dualAxis = {
  dualAxis_desc,
  sizeof(dualAxis_desc)
};

typedef struct {
  uint8_t reportID;
  unsigned x: 10;
  unsigned y: 10;
  uint8_t button1: 1;
  uint8_t button2: 1;
  uint8_t padding: 2;
} __packed SimpleJoystickReport_t;

class HIDSimpleJoystick : public HIDReporter {
  public:
    SimpleJoystickReport_t joyReport;
    HIDSimpleJoystick(USBHID& HID, uint8_t reportID = HID_JOYSTICK_REPORT_ID)
      : HIDReporter(HID, &dualAxis, (uint8_t*) & joyReport, sizeof(joyReport), reportID) {
      joyReport.button1 = 0;
      joyReport.button2 = 0;
      joyReport.x = 512;
      joyReport.y = 512;
    }
};



enum {
  MODE_JOYSTICK = 0,
  MODE_DUAL_JOYSTICK,
  MODE_MOUSE,
#ifdef SUPPORT_X360
  MODE_X360
#endif
};

int mode;

uint16 analogRead2(uint8 pin) {
  return adc_read(pin == ANALOG1 ? &adc1 : &adc2, PIN_MAP[pin].adc_channel);
}

#ifdef SERIAL_DEBUG
USBCompositeSerial debug;
#define DEBUG(...) debug.println(__VA_ARGS__);
#else
#define DEBUG(...)
#endif

class AnalogPort {
  public:
    uint32 port;
    uint32 oldValue = NO_VALUE;
    uint32 rejectedCount = 0;
    uint32 rejectedSum = 0;
    uint32 getValue() {
      uint32 v = 0;

//      nvic_globalirq_disable();
      for (uint32 i = 0 ; i < READ_ITERATIONS ; i++)
        v += analogRead2(port);
//      nvic_globalirq_enable();
      v = (v + READ_ITERATIONS / 2) / READ_ITERATIONS;

      if (oldValue != NO_VALUE && v != oldValue && v < oldValue + HYSTERESIS && oldValue < v + HYSTERESIS) {
        if (rejectedCount > 0) {
          rejectedCount++;
          rejectedSum += v;
          if (rejectedCount >= MAX_HYSTERESIS_REJECTIONS) {
            v = (rejectedSum + MAX_HYSTERESIS_REJECTIONS / 2) / MAX_HYSTERESIS_REJECTIONS;
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
      return 4095 - v;
    };

    AnalogPort(uint32 _port) {
      port = _port;
    };
};

AnalogPort analog1(ANALOG1);
AnalogPort analog2(ANALOG2);
AnalogPort* analog[NUM_PADDLES] = { &analog1, &analog2 };
Debounce digital1(PA2);
Debounce digital2(PA4);
#define NUM_DIGITAL (NUM_PADDLES+NUM_EXTRA)
#if NUM_EXTRA
Debounce extra1(PA5);
Debounce extra2(PA6);
Debounce extra3(PA7);
Debounce extra4(PA8);
Debounce* digital[NUM_DIGITAL] = { &digital1, &digital2, &extra1, &extra2, &extra3, &extra4 };
#else
Debounce* digital[NUM_DIGITAL] = { &digital1, &digital2 };
#endif
const uint32 mouseButtons[2] = { MOUSE_LEFT, MOUSE_RIGHT };

USBHID HID;
HIDSimpleJoystick joy1(HID);
HIDSimpleJoystick joy2(HID);
HIDSimpleJoystick* joys[2] = { &joy1, &joy2 };
HIDAbsMouse mouse(HID);
#if NUM_EXTRA
HIDKeyboard keyboard(HID);
#endif
#ifdef SUPPORT_X360
USBXBox360 XBox360;
#endif

void setup() {
  // default is 55_5
  adc_set_sample_rate(ADC1, ADC_SMPR_239_5);
  adc_set_sample_rate(ADC2, ADC_SMPR_239_5);

#if 0
  for (uint32 i = 0 ; i < BOARD_NR_GPIO_PINS ; i++) {
    if (i != PA11 && i != PA12) {
      pinMode(i, INPUT_PULLDOWN);
    }
  }
#endif

  for (uint32 i = 0 ; i < NUM_PADDLES ; i++) 
    pinMode(analog[i]->port, INPUT_ANALOG);

  for (uint32 i = 0 ; i < NUM_DIGITAL ; i++) 
    pinMode(digital[i]->pin, INPUT_PULLDOWN);

#ifndef SERIAL_DEBUG
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
    EEPROM8_storeValue(0, mode);
  }
  else if (!a && b) {
    mode = MODE_MOUSE;
    EEPROM8_storeValue(0, mode);
  }
  else if (a && b) {
    mode = MODE_DUAL_JOYSTICK;
    EEPROM8_storeValue(0, mode);
  }
#endif
#ifdef SUPPORT_X360
  mode = MODE_X360;
#endif
#else
  mode = MODE_JOYSTICK;
#endif

  pinMode(LED, OUTPUT);
  digitalWrite(LED, mode != MODE_MOUSE);
  HID.clear();
  if (mode == MODE_JOYSTICK) {
    USBComposite.setVendorId(0x04d8);
    USBComposite.setProductId(0xbeef);
    USBComposite.setManufacturerString("Grand Idea Studio");
    USBComposite.setProductString("Stelladaptor 2600-to-USB Interface");
    joy1.registerProfile();
#if NUM_EXTRA    
    keyboard.registerProfile();
#endif    
#ifdef SERIAL_DEBUG
    HID.registerComponent();
    debug.registerComponent();
    USBComposite.begin();
#else
    HID.begin();
#endif
  }
  else if (mode == MODE_DUAL_JOYSTICK) {
    USBComposite.setProductId(PRODUCT_ID + mode);
    USBComposite.setProductString("Paddle Dual Joystick");
    for (uint32 i = 0; i < NUM_PADDLES; i++) {
      joys[i]->registerProfile();
    }
#if NUM_EXTRA    
    keyboard.registerProfile();
#endif    
    HID.begin();
  }
  else if (mode == MODE_MOUSE) {
    USBComposite.setProductId(PRODUCT_ID + mode);
    USBComposite.setProductString("Paddle Mouse");
    mouse.registerProfile();
#if NUM_EXTRA    
    keyboard.registerProfile();
#endif    
    HID.begin();
  }
#ifdef SUPPORT_X360
  else if (mode == MODE_X360) {
    USBComposite.setProductString("Paddle X360");
    XBox360.begin();
    XBox360.setManualReportMode(true);
  }
#endif
  while (!USBComposite);
}

#ifdef SERIAL_DEBUG
uint32 countStart = 0;
uint32 count = 0;
#endif

void loop() {
  if (!USBComposite.isReady())
    return;
    
  uint32 pots[NUM_PADDLES];

  for (uint32 i = 0 ; i < NUM_PADDLES ; i++ ) {
    pots[i] = analog[i]->getValue();
    uint32 b = digital[i]->getEvent();
    if (b != DEBOUNCE_NONE) {
      uint8 pressed = b == DEBOUNCE_PRESSED;
      if (mode == MODE_JOYSTICK) {
        if (i == 0)
          joy1.joyReport.button1 = pressed;
        else
          joy1.joyReport.button2 = pressed;
      }
      else if (mode == MODE_DUAL_JOYSTICK)
        joys[i]->joyReport.button1 = pressed;
#ifdef SUPPORT_X360
      else if (mode == MODE_X360)
        XBox360.button(i + 1, pressed);
#endif
      else {
        if (pressed)
          mouse.press(mouseButtons[i]);
        else
          mouse.release(mouseButtons[i]);
      }
    }
  }

#if NUM_EXTRA
  for (uint32 i = 0 ; i < NUM_EXTRA ; i++) {
    uint32 b = digital[NUM_PADDLES + i]->getEvent();
    if (b != DEBOUNCE_NONE) {
      if(b == DEBOUNCE_PRESSED)
        keyboard.press(extraKeys[i]);
      else
        keyboard.release(extraKeys[i]);
    }
  }
#endif  
  
  if (mode == MODE_JOYSTICK) {
    joy1.joyReport.x = pots[0] / 4;
#if NUM_PADDLES > 1
    joy1.joyReport.y = pots[1] / 4;
#endif
    joy1.sendReport();
  }
  else if (mode == MODE_DUAL_JOYSTICK) {
    for (uint32 i = 0 ; i < 2 ; i++) {
      uint16 v = pots[i] / 4;
      joys[i]->joyReport.x = v;
      joys[i]->joyReport.y = v;
      joys[i]->sendReport();
    }
  }
#ifdef SUPPORT_X360
  else if (mode == MODE_X360) {
    XBox360.X((int16)(pots[0] * 65535 / 4095) - 32767);
#if NUM_PADDLES > 1
    XBox360.Y((int16)(pots[1] * 65535 / 4095) - 32767);
#endif
    XBox360.send();
  }
#endif
  else if (mode == MODE_MOUSE) {
    mouse.move(32767 * pots[0] / 4095, 32767 * pots[1] / 4095);
  }
#ifdef SERIAL_DEBUG
  count++;
  if (count == 1000) {
    uint32 t = millis() - countStart;
    char out[10];
    out[9] = 0;
    char *p = out + 8;
    while (1) {
      *p = t % 10 + '0';
      t /= 10;
      if (t == 0) {
        debug.write(p);
        debug.write("\n");
        break;
      }
      p--;
    }
    count = 0;
    countStart = millis();
  }
#endif

}

