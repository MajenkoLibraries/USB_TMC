/* Test of the TMC support
 *
 * Send the string ":AINx" where x is 0-5 to
 * request the analog input x value.  Value is
 * returned in the next read.
 *
 * Send the string ":TIME" and the millis() value
 * is returned in the next read.
 *
 * Unknown commands are dumped to the CDC/ACM port.
 */
 
#include <USB_TMC.h>

USBFS usbDevice;
USBManager USB(usbDevice, 0xf055, 0x3827);
USB_TMC TMC;
CDCACM uSerial;

void gotMessage(uint8_t *msg, uint32_t len) {
    if (msg[0] == ':') {
        if (!strncmp((char *)msg, ":AIN", 4)) {
            uint32_t chan = strtoul((char *)msg + 4, NULL, 10);
            if (chan < 6) {
                char temp[6];
                sprintf(temp, "%d\n", analogRead(chan));
                TMC.send((uint8_t *)temp, strlen(temp), TMC_REPLACE);
            }
            return;
        }
    
        if (!strncmp((char *)msg, ":TIME", 5)) {
            char temp[20];
            sprintf(temp, "%lu\n", millis());
            TMC.send((uint8_t *)temp, strlen(temp), TMC_REPLACE);
            return;
        }
    }
    
    for (size_t i = 0; i < len; i++) {
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
