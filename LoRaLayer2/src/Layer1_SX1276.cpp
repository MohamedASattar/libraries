#define RL_SX1276
#ifdef RL_SX1276
#include <Layer1_SX1276.h>
Layer1Class::Layer1Class(SX1276 *lora, int mode, int cs, int reset, int dio, uint8_t sf, uint32_t frequency, int power, int loraInitialized, uint32_t spiFrequency, float bandwidth, uint8_t codingRate, uint8_t syncWord, uint8_t currentLimit, uint8_t preambleLength, uint8_t gain)
    : _LoRa(lora),
      _mode(mode),
      _csPin(cs),
      _resetPin(reset),
      _DIOPin(dio),
      _spreadingFactor(sf),
      _loraFrequency(frequency),
      _txPower(power),
      _loraInitialized(loraInitialized),
      _spiFrequency(spiFrequency),
      _bandwidth(bandwidth),
      _codingRate(codingRate),
      _syncWord(syncWord),
      _currentLimit(currentLimit),
      _preambleLength(preambleLength),
      _gain(gain)
{
  txBuffer = new packetBuffer();
  rxBuffer = new packetBuffer();
};

bool _dioFlag = false;
bool _transmitFlag = false;
bool _enableInterrupt = true;

/* Public access to local variables
 */
int Layer1Class::getTime()
{
  return millis();
}

int Layer1Class::spreadingFactor()
{
  return _spreadingFactor;
}

/* Private functions
 */
// Send packet function
int Layer1Class::sendPacket(char *data, size_t len)
{
  int ret = 0;
#ifdef LL2_DEBUG
  Serial.printf("Layer1::sendPacket(): data = ");
  for (int i = 0; i < len; i++)
  {
    Serial.printf("%c", data[i]);
  }
  Serial.printf("\r\n");
#endif
  // data[len] = '\0';
  int state = _LoRa->startTransmit((uint8_t *)data, len);
#ifdef LL2_DEBUG
  Serial.printf("Layer1::sendPacket(): state = %d\r\n", state);
#endif
  if (state == RADIOLIB_ERR_NONE)
  {
    _transmitFlag = true;
  }
  else if (state == RADIOLIB_ERR_PACKET_TOO_LONG)
  {
    // packet longer than 256 bytes
    ret = 1;
  }
  else if (state == RADIOLIB_ERR_TX_TIMEOUT)
  {
    // timeout occurred while transmitting packet
    ret = 2;
  }
  else
  {
    // some other error occurred
    ret = 3;
  }
  return ret;
}

// Receive packet callback
void Layer1Class::setFlag(void)
{
  // check if the interrupt is enabled
  if (!_enableInterrupt)
  {
    return;
  }
  // we got a packet, set the flag
  _dioFlag = true;
}

/*Main public functions
 */
// Initialization
int Layer1Class::init()
{

  int state = _LoRa->begin(_loraFrequency, _bandwidth, _spreadingFactor, _codingRate, RADIOLIB_SX127X_SYNC_WORD, _txPower, _preambleLength, _gain);
#ifdef LL2_DEBUG
  Serial.printf("Layer1::init(): state = %d\r\n", state);
#endif
  if (state != RADIOLIB_ERR_NONE)
  {
    return _loraInitialized;
  }

  _LoRa->setDio0Action(Layer1Class::setFlag);

  state = _LoRa->startReceive();
  if (state != RADIOLIB_ERR_NONE)
  {
    return _loraInitialized;
  }

  _loraInitialized = 1;
  return _loraInitialized;
}

// Layer1Class* Layer1Class::getThis() {
//   return this;
// }

// Transmit polling function
int Layer1Class::transmit()
{
  BufferEntry entry = txBuffer->read();

  if (entry.length > 0)
  {
    // TODO check if routing packet and change the channel
    // uint8_t routing[4] = {0xaf, 0xff, 0xff, 0xff};
    // if (memcmp(&entry.data[6], routing, 4) == 0)
    // {
      // int16_t state = _LoRa->setFrequency(868.3);
    //   Serial.printf("setFrequency(868.3) -> %d\r\n", state);
    // }
    // else
    // {
    //   int16_t state = _LoRa->setFrequency(868);
    //   Serial.printf("setFrequency(868) -> %d\r\n", state);
    // }

    // Serial.printf("Layer1::transmit(): entry.length: %d\r\n", entry.length);
    sendPacket(entry.data, entry.length);
  }
  return entry.length;
}

// Receive polling function
int Layer1Class::receive()
{
  int ret = 0;
  if (_dioFlag)
  {
    if (_transmitFlag)
    {
// interrupt caused by transmit, clear flags and return 0
#ifdef LL2_DEBUG
      Serial.printf("Layer1::receive(): transmit complete\r\n");
#endif
      _transmitFlag = false;
      _dioFlag = false;
      _LoRa->startReceive();
    }
    else
    {
      // interrupt caused by reception
      _enableInterrupt = false;
      _dioFlag = false;
      size_t len = _LoRa->getPacketLength();
      byte data[len];
      int state = _LoRa->readData(data, len);
      if (state == RADIOLIB_ERR_NONE)
      {
        BufferEntry entry;
        memcpy(&entry.data[0], &data[0], len); // copy data to buffer, excluding null terminator
        entry.length = len;
        rxBuffer->write(entry);
#ifdef LL2_DEBUG
        Serial.printf("Layer1::receive(): data = ");
        for (int i = 0; i < len; i++)
        {
          Serial.printf("%c", data[i]);
        }
        Serial.printf("\r\n");
#endif
        ret = len;
      }
      else if (state == RADIOLIB_ERR_CRC_MISMATCH)
      {
        // packet was received, but is malformed
        ret = -1;
      }
      else
      {
        // some other error occurred
        ret = -2;
      }
      _LoRa->startReceive();
      _enableInterrupt = true;
    }
  }
  return ret;
}
#endif
