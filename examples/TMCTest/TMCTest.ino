#include <USB_TMC.h>

USBFS usbDevice;
USBManager USB(usbDevice, 0xf055, 0x3827);
USB_TMC TMC;
CDCACM uSerial;

void gotMessage(uint8_t *msg, uint32_t len) {
    for (int i = 0; i < len; i++) {
        uSerial.write(msg[i]);
    }
}

void setup() {
    TMC.onMessage(gotMessage);
    USB.addDevice(TMC);
    USB.addDevice(uSerial);
    USB.begin();
}

void loop() {
    
}
