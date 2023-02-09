#ifndef LAYER1_H
#define LAYER1_H

#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include <packetBuffer.h>

class Layer1Class {

public:
Layer1Class(SX1276 *lora, int mode, int cs, int reset, int dio, uint8_t sf, uint32_t frequency, int power, int loraInitialized = 0, uint32_t spiFrequency = 100E3, float bandwidth = 125.0, uint8_t codingRate = 5,    uint8_t syncWord = RADIOLIB_SX127X_SYNC_WORD, uint8_t currentLimit = 100, uint8_t preambleLength = 8, uint8_t gain = 0) ; 
    
    // Public access to local variables
    static int getTime();
    int spreadingFactor();

    // Fifo buffers
    packetBuffer *txBuffer;
    packetBuffer *rxBuffer;

    // Main public functions
    int init();
    int transmit();
    int receive();
    SX1276 * const getLoRa(void){
        return _LoRa;
    }
    float getBW(void){
        return _bandwidth;
    }
    uint8_t getCR(void){
        return _codingRate;
    }
    
private:
    SX1276 *_LoRa;
    // Main private functions
    static void setFlag(void);
    int sendPacket(char *data, size_t len);

    // Local variables
    int _mode;
    int _csPin;
    int _resetPin;
    int _DIOPin;
    uint8_t _spreadingFactor;
    uint32_t _loraFrequency;
    int _txPower;
    int _loraInitialized;
    uint32_t _spiFrequency;
    float _bandwidth; 
    uint8_t _codingRate;
    uint8_t _syncWord; //SX127X_SYNC_WORD, 
    uint8_t _currentLimit;
    uint8_t _preambleLength;
    uint8_t _gain;
};

#endif
