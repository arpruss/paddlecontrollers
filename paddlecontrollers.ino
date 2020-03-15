#include <USBComposite.h>

#define JOYSTICK

uint32 analog1 = PA1;
uint32 analog2 = PA3;
uint32 button1 = PA2;
uint32 button2 = PA4;
uint32 down1 = 0;
uint32 down2 = 0;
uint32 a1 = 2048;
uint32 a2 = 2048;

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
  pinMode(analog1, INPUT_ANALOG);
  pinMode(analog2, INPUT_ANALOG);
  pinMode(button1, INPUT_PULLDOWN);
  pinMode(button2, INPUT_PULLDOWN);
  //Serial.begin();
#ifdef JOYSTICK
  HID.begin();
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
  a1 = (3*a1 + (4095-analogRead(analog1)))/4;
  a2 = (3*a2 + (4095-analogRead(analog2)))/4;
#ifdef JOYSTICK
  joy.button(1,digitalRead(button1));
  joy.button(2,digitalRead(button2));
  joy.X(a1*1023/4095);
  joy.Y(a2*1023/4095);
  joy.sendReport();
#else  
  if (down1 != digitalRead(button1)) {
    down1 = !down1;
    if (down1)
      mouse.press(MOUSE_LEFT);
    else
      mouse.release(MOUSE_LEFT);
  }
  if (down2 != digitalRead(button2)) {
    down2 = !down2;
    if (down2)
      mouse.press(MOUSE_RIGHT);
    else
      mouse.release(MOUSE_RIGHT);
  }
  mouse.move(32767*a1/4095,32767*a2/4095);
#endif
}
