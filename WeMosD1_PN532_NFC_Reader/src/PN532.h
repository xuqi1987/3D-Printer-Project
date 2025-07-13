#ifndef PN532_H
#define PN532_H

#include <Arduino.h>
#include <Wire.h>

// PN532 I2C地址
#define PN532_I2C_ADDRESS (0x24)

// PN532命令
#define PN532_COMMAND_GETFIRMWAREVERSION (0x02)
#define PN532_COMMAND_SAMCONFIGURATION (0x14)
#define PN532_COMMAND_INLISTPASSIVETARGET (0x4A)
#define PN532_COMMAND_INDATAEXCHANGE (0x40)

// PN532响应
#define PN532_RESPONSE_INDATAEXCHANGE (0x41)
#define PN532_RESPONSE_INLISTPASSIVETARGET (0x4B)

// PN532状态
#define PN532_STATUS_READY (0x01)
#define PN532_STATUS_BUSY (0x00)

class PN532 {
public:
    PN532(uint8_t sda_pin, uint8_t scl_pin);
    bool begin();
    bool SAMConfig();
    bool readPassiveTargetID(uint8_t cardbaudrate, uint8_t* uid, uint8_t* uidLength);
    bool readNFC();

private:
    uint8_t _sda_pin;
    uint8_t _scl_pin;
    bool sendCommand(uint8_t* command, uint8_t length);
    bool readResponse(uint8_t* response, uint8_t* length);
};

#endif // PN532_H 