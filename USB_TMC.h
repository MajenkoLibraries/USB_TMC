#ifndef _USB_TMC_H
#define _USB_TMC_H

#include <Arduino.h>

#define TMC_DEVICE_OUT          1
#define TMC_DEVICE_IN           2
#define TMC_VENDOR_OUT          126
#define TMC_VENDOR_IN           127

#define TMC_BLOCK               0
#define TMC_REPLACE             1
#define TMC_REJECT              2

struct tmcMessageHeader {
    uint8_t msgId;
    uint8_t tag;    
    uint8_t tagInverse;
    uint8_t reserved;
    uint32_t transferSize;
    uint8_t transferAttributes;
    uint8_t termChar;
    uint8_t unused[2];
} __attribute__((packed));
    

class USB_TMC : public USBDevice {
    private:
        uint8_t _ifBulk;
        uint8_t _epBulk;
        uint8_t _epInt;
        USBManager *_manager;

        void (*_onMessage)(uint8_t *, size_t);

        uint8_t _txBuffer[48];
        uint8_t _txBytes;
        uint8_t _rxBuffer[48];
        uint8_t _rxBytes;
        uint8_t _rxPos;

        uint8_t _intA[64];
        uint8_t _intB[64];
#if defined (__PIC32MX__)
    #define USB_TMC_BULKEP_SIZE 64
#elif defined(__PIC32MZ__)
    #define USB_TMC_BULKEP_SIZE 512
#endif

        uint8_t _bulkRxA[USB_TMC_BULKEP_SIZE];
        uint8_t _bulkRxB[USB_TMC_BULKEP_SIZE];
        uint8_t _bulkTxA[USB_TMC_BULKEP_SIZE];
        uint8_t _bulkTxB[USB_TMC_BULKEP_SIZE];

    public:
        USB_TMC() : _onMessage(NULL), _txBytes(0), _rxBytes(0), _rxPos(0) {}

        uint16_t getDescriptorLength();
        uint8_t getInterfaceCount();
        bool getStringDescriptor(uint8_t idx, uint16_t maxlen);
        uint32_t populateConfigurationDescriptor(uint8_t *buf);
        void initDevice(USBManager *manager);
        bool getDescriptor(uint8_t ep, uint8_t target, uint8_t id, uint8_t maxlen);
        bool getReportDescriptor(uint8_t ep, uint8_t target, uint8_t id, uint8_t maxlen);
        void configureEndpoints();
        bool onSetupPacket(uint8_t ep, uint8_t target, uint8_t *data, uint32_t l);
        bool onOutPacket(uint8_t ep, uint8_t target, uint8_t *data, uint32_t l);
        bool onInPacket(uint8_t ep, uint8_t target, uint8_t *data, uint32_t l);
        void onEnumerated();

        void onMessage(void (*func)(uint8_t *data, size_t len));
        bool send(const uint8_t *data, size_t len, uint8_t behaviour = TMC_REJECT);

        bool txBusy();
};

#endif
