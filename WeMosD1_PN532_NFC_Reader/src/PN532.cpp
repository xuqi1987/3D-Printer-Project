#include "PN532.h"

PN532::PN532(uint8_t sda_pin, uint8_t scl_pin) {
    _sda_pin = sda_pin;
    _scl_pin = scl_pin;
}

bool PN532::begin() {
    Serial.println("初始化I2C...");
    Wire.begin(_sda_pin, _scl_pin);
    Wire.setClock(100000); // 降低I2C时钟频率以提高稳定性
    Serial.println("I2C初始化完成");
    
    // 等待PN532启动
    delay(100);
    
    // 发送唤醒命令
    uint8_t wakeup[] = {0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Wire.beginTransmission(PN532_I2C_ADDRESS);
    for (uint8_t i = 0; i < sizeof(wakeup); i++) {
        Wire.write(wakeup[i]);
    }
    Wire.endTransmission();
    delay(100);
    
    return true;
}

bool PN532::sendCommand(uint8_t* command, uint8_t length) {
    Serial.println("发送命令到PN532...");
    Serial.print("I2C地址: 0x");
    Serial.println(PN532_I2C_ADDRESS, HEX);
    
    // 发送命令头
    Wire.beginTransmission(PN532_I2C_ADDRESS);
    if (Wire.endTransmission() != 0) {
        Serial.println("无法访问PN532设备！");
        return false;
    }
    
    // 发送命令
    Wire.beginTransmission(PN532_I2C_ADDRESS);
    Wire.write(0x00); // 命令
    Wire.write(0x00); // 命令
    Wire.write(0xFF); // 开始码
    Wire.write(length + 1); // 长度
    Wire.write(~(length + 1) + 1); // 长度校验
    Wire.write(0xD4); // 命令标识
    for (uint8_t i = 0; i < length; i++) {
        Wire.write(command[i]);
        Serial.print("发送字节: 0x");
        Serial.println(command[i], HEX);
    }
    Wire.write(0x00); // 结束码
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
        Serial.println("命令发送成功");
        return true;
    } else {
        Serial.print("命令发送失败，错误代码: ");
        Serial.println(error);
        return false;
    }
}

bool PN532::readResponse(uint8_t* response, uint8_t* length) {
    Serial.println("等待PN532响应...");
    uint8_t timeout = 0;
    while (timeout < 100) {
        Wire.requestFrom(PN532_I2C_ADDRESS, 1);
        if (Wire.available()) {
            uint8_t status = Wire.read();
            Serial.print("状态字节: 0x");
            Serial.println(status, HEX);
            
            if (status == PN532_STATUS_READY) {
                Serial.println("PN532准备就绪");
                Wire.requestFrom(PN532_I2C_ADDRESS, 32);
                if (Wire.available()) {
                    uint8_t len = Wire.read();
                    Serial.print("响应长度: ");
                    Serial.println(len);
                    
                    for (uint8_t i = 0; i < len; i++) {
                        if (Wire.available()) {
                            response[i] = Wire.read();
                            Serial.print("响应字节 ");
                            Serial.print(i);
                            Serial.print(": 0x");
                            Serial.println(response[i], HEX);
                        }
                    }
                    *length = len;
                    Serial.println("成功读取响应");
                    return true;
                }
            }
        }
        delay(10);
        timeout++;
    }
    Serial.println("读取响应超时");
    return false;
}

bool PN532::SAMConfig() {
    Serial.println("配置PN532 SAM...");
    uint8_t command[] = {PN532_COMMAND_SAMCONFIGURATION, 0x01, 0x00, 0x01, 0x00};
    uint8_t response[32];
    uint8_t responseLength;
    
    if (!sendCommand(command, sizeof(command))) {
        Serial.println("发送SAM配置命令失败");
        return false;
    }
    
    if (!readResponse(response, &responseLength)) {
        Serial.println("读取SAM配置响应失败");
        return false;
    }
    
    if (response[0] != PN532_COMMAND_SAMCONFIGURATION + 1) {
        Serial.println("无效的SAM配置响应");
        return false;
    }
    
    return true;
}

bool PN532::readPassiveTargetID(uint8_t cardbaudrate, uint8_t* uid, uint8_t* uidLength) {
    Serial.println("开始读取NFC标签...");
    uint8_t command[] = {PN532_COMMAND_INLISTPASSIVETARGET, 0x01, cardbaudrate};
    uint8_t response[32];
    uint8_t responseLength;
    
    if (!sendCommand(command, sizeof(command))) {
        Serial.println("发送读取命令失败");
        return false;
    }
    
    if (!readResponse(response, &responseLength)) {
        Serial.println("读取响应失败");
        return false;
    }
    
    if (response[0] != PN532_RESPONSE_INLISTPASSIVETARGET) {
        Serial.println("无效的响应类型");
        return false;
    }
    
    if (response[1] != 0x01) {
        Serial.println("未检测到标签");
        return false;
    }
    
    *uidLength = response[5];
    for (uint8_t i = 0; i < *uidLength; i++) {
        uid[i] = response[6 + i];
    }
    
    Serial.println("成功读取标签ID");
    return true;
}

bool PN532::readNFC() {
    uint8_t uid[7];
    uint8_t uidLength;
    
    if (readPassiveTargetID(0x00, uid, &uidLength)) {
        String uidString = "";
        for (uint8_t i = 0; i < uidLength; i++) {
            if (uid[i] < 0x10) {
                uidString += "0";
            }
            uidString += String(uid[i], HEX);
        }
        
        Serial.print("检测到NFC标签，UID: ");
        Serial.println(uidString);
        return true;
    }
    
    return false;
} 