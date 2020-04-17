#include <USB_TMC.h>

#define DSC_INTERFACE_LEN 0x09
#define DSC_INTERFACE(NIF, NEP, CL, SCL, PR, I) \
    buf[i++] = DSC_INTERFACE_LEN;   \
    buf[i++] = 0x04;    \
    buf[i++] = (NIF);   \
    buf[i++] = 0x00;    \
    buf[i++] = (NEP);   \
    buf[i++] = (CL);    \
    buf[i++] = (SCL);   \
    buf[i++] = (PR);    \
    buf[i++] = (I);

#define DSC_EP_LEN 0x07
#define DSC_EP(N, A, S, I) \
    buf[i++] = DSC_EP_LEN;  \
    buf[i++] = 0x05;    \
    buf[i++] = (N);     \
    buf[i++] = (A);     \
    buf[i++] = ((S) & 0xFF); \
    buf[i++] = ((S) >> 8); \
    buf[i++] = (I);

#define EP_DIR_OUT  0x00
#define EP_DIR_IN   0x80

#define EP_TYPE_CTRL 0x00
#define EP_TYPE_ISO  0x01
#define EP_TYPE_BULK 0x02
#define EP_TYPE_INT  0x03

uint16_t USB_TMC::getDescriptorLength() {
    return DSC_INTERFACE_LEN + DSC_EP_LEN + DSC_EP_LEN; // + DSC_EP_LEN;
}

uint8_t USB_TMC::getInterfaceCount() {
    return 1;
}

bool USB_TMC::getStringDescriptor(uint8_t __attribute__((unused)) idx, uint16_t __attribute__((unused)) maxlen) {
    return false;
}

uint32_t USB_TMC::populateConfigurationDescriptor(uint8_t *buf) {
    uint8_t i = 0;

    DSC_INTERFACE(_ifBulk, 2, 254, 3, 1, 0)
//    DSC_EP(_epInt | EP_DIR_IN, EP_TYPE_INT, 64, 11)
    DSC_EP(_epBulk | EP_DIR_IN, EP_TYPE_BULK, USB_TMC_BULKEP_SIZE, 0)
    DSC_EP(_epBulk | EP_DIR_OUT, EP_TYPE_BULK, USB_TMC_BULKEP_SIZE, 0)

    return i;
}

void USB_TMC::initDevice(USBManager *manager) {
    _manager = manager;
    _ifBulk = _manager->allocateInterface();
    _epInt  = _manager->allocateEndpoint();
    _epBulk = _manager->allocateEndpoint();
}

bool USB_TMC::getDescriptor(uint8_t __attribute__((unused)) ep, uint8_t __attribute__((unused)) target, uint8_t __attribute__((unused)) id, uint8_t __attribute__((unused)) maxlen) {
    return false;
}

bool USB_TMC::getReportDescriptor(uint8_t __attribute__((unused)) ep, uint8_t __attribute__((unused)) target, uint8_t __attribute__((unused)) id, uint8_t __attribute__((unused)) maxlen) {
    return false;
}

void USB_TMC::configureEndpoints() {
    _manager->addEndpoint(_epInt, EP_IN, EP_INT, 64, _intA, _intB);
    if (_manager->isHighSpeed()) {
        _manager->addEndpoint(_epBulk, EP_IN, EP_BLK, 512, _bulkRxA, _bulkRxB);
        _manager->addEndpoint(_epBulk, EP_OUT, EP_BLK, 512, _bulkTxA, _bulkTxB);
    } else {
        _manager->addEndpoint(_epBulk, EP_IN, EP_BLK, 64, _bulkRxA, _bulkRxB);
        _manager->addEndpoint(_epBulk, EP_OUT, EP_BLK, 64, _bulkTxA, _bulkTxB);
    }
}

#define TMC_INITIATE_ABORT_BULK_OUT         0x..01
#define TMC_CHECK_ABORT_BULK_OUT_STATUS     0x..02
#define TMC_INITIATE_ABORT_BULK_IN          0x..03
#define TMC_CHECK_ABORT_BULK_IN_STATUS      0x..04
#define TMC_INITIATE_CLEAR                  0xA105
#define TMC_CHECK_CLEAR_STATUS              0xA106
#define TMC_GET_CAPABILITIES                0xA107

bool USB_TMC::onSetupPacket(uint8_t ep __attribute__((unused)), uint8_t __attribute__((unused)) target, uint8_t __attribute__((unused)) *data, uint32_t __attribute__((unused)) l) {

    uint16_t wRequest = (data[0] << 8) | data[1];
    uint16_t wValue = (data[3] << 8) | data[2];
    uint16_t wIndex = (data[5] << 8) | data[4];
    uint16_t wLength = (data[7] << 8) | data[6];

    if (wIndex == _ifBulk) {
        switch (wRequest) {
            case TMC_GET_CAPABILITIES: {
                uint8_t resp[24] = {
                    0x01, 0x00, 0x01, 0x01, 0x04, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                };
                _manager->sendBuffer(0, resp, wLength);
                return true;
            }
            break;
            case TMC_INITIATE_CLEAR: {
                uint8_t status = 0x01;
                _manager->sendBuffer(0, &status, 1);
                return true;
            }
            break;
            case TMC_CHECK_CLEAR_STATUS: {
                uint8_t resp[2] = {
                    0x01, 0x00
                };
                _manager->sendBuffer(0, resp, 2);
                return true;
            }
        }
    }
    return false;
}

extern CDCACM uSerial;

uint32_t count = 0;

bool USB_TMC::onOutPacket(uint8_t ep, uint8_t __attribute__((unused)) target, uint8_t *data, uint32_t l) {
    if (ep == _epBulk) {
        if (_rxBytes > 0) { // In the middle of a transfer
            memcpy(_rxBuffer + _rxPos, data, min(l, _rxBytes - _rxPos));
            _rxPos += min(l, _rxBytes - _rxPos);
            if (_rxPos == _rxBytes) {
                _onMessage(_rxBuffer, _rxBytes);
                _rxPos = _rxBytes = 0;
            }
        }

        struct tmcMessageHeader *header = (struct tmcMessageHeader *)data;



        if (header->tag == (~(header->tagInverse) & 0xFF)) { // Valid
            switch (header->msgId) {
                case TMC_DEVICE_OUT: {
                    if (header->transferSize < 48) {
                        _rxBytes = header->transferSize;
                        _rxPos = 0;
                        int len = l - 12;
                        memcpy(_rxBuffer, data + 12, min(len, _rxBytes));
                        _rxPos += min(len, _rxBytes);
                        if (_rxPos == _rxBytes) {
                            _onMessage(_rxBuffer, _rxBytes);
                            _rxPos = _rxBytes = 0;
                        }
                    }
                } break;
                case TMC_DEVICE_IN: {
                    if (_txBytes == 0) {
                        struct tmcMessageHeader response;
                        response.msgId = header->msgId;
                        response.tag = header->tag;
                        response.tagInverse = header->tagInverse;
                        response.transferSize = 0;
                        response.transferAttributes = 1;
                        _manager->sendBuffer(_epBulk, (uint8_t *)&response, 12);
                    } else {
                        uint8_t toSend = _txBytes;
                        if (toSend & 0x03) {
                            toSend += (4 - (toSend & 0x03));
                        }
                        uint8_t buf[12 + toSend];
                        struct tmcMessageHeader *response = (struct tmcMessageHeader *)buf;
                        response->msgId = header->msgId;
                        response->tag = header->tag;
                        response->tagInverse = header->tagInverse;
                        response->transferSize = _txBytes;
                        response->transferAttributes = 1;

                        memcpy(buf + 12, _txBuffer, _txBytes);

                        _manager->sendBuffer(_epBulk, buf, toSend + 12);
                        _txBytes = 0;
                    }
                } break;
                case TMC_VENDOR_OUT:
uSerial.println("TMC_VENDOR_OUT");
                    break;
                case TMC_VENDOR_IN:
uSerial.println("TMC_VENDOR_IN");
                    break;
        
            }
        }
        return true;
    }
    return false;
}

bool USB_TMC::onInPacket(uint8_t ep, uint8_t __attribute__((unused)) target, uint8_t __attribute__((unused)) *data, uint32_t __attribute__((unused)) l) {

    if (ep == _epBulk) {
        return true;
    }

    return false;
}

void USB_TMC::onEnumerated() {
}

bool USB_TMC::send(const uint8_t *b, size_t len, uint8_t behaviour) {
    if (len > 48) return false;

    switch (behaviour) {
        case TMC_BLOCK:
            while (txBusy());
            break;
        case TMC_REJECT:
            if (txBusy()) return false;
            break;
        case TMC_REPLACE:
            break;
    } 

    memcpy(_txBuffer, b, len);
    _txBytes = len;
    return true;
}


void USB_TMC::onMessage(void (*func)(uint8_t *, size_t)) {
    _onMessage = func;
}

bool USB_TMC::txBusy() {
    return _txBytes > 0;
}
