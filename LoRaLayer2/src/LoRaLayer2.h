#ifndef LORALAYER2_H
#define LORALAYER2_H
#define RL_SX1276
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef ARDUINO_LORA
#include <Arduino.h>
#include <Layer1_LoRa.h>
#endif

#ifdef RL_SX1276
#include <Arduino.h>
#include <Layer1_SX1276.h>
#endif

#ifdef SIM
#include <Layer1_Sim.h>
#endif

#include <packetBuffer.h>

//#define DEBUG 0
// #define DATAGRAM_HEADER 5
// #define HEADER_LENGTH 17
// #define PACKET_LENGTH 256
// #define DATA_LENGTH 241
// #define MESSAGE_LENGTH 233
// #define SHA1_LENGTH 40
// #define ADDR_LENGTH 4
// #define MAX_ROUTES_PER_PACKET 40 
// #define ASYNC_TX 1
// #define DEFAULT_TTL 30

//edit
#define PACKET_LENGTH 256 // max packet size for LoRa transmissions
#define HEADER_LENGTH 17
#define DATA_LENGTH 239 // max datagram size = 256 - 17
#define DATAGRAM_HEADER 5
#define MESSAGE_LENGTH 234 // max message size = 239 - 5
#define SHA1_LENGTH 40
#define ADDR_LENGTH 4
#define MAX_ROUTES_PER_PACKET (int) (DATA_LENGTH / (2 * ADDR_LENGTH + 2)) //23 

#define ASYNC_TX 1
#define DEFAULT_TTL 30

extern uint8_t BROADCAST[ADDR_LENGTH];
extern uint8_t LOOPBACK[ADDR_LENGTH];
extern uint8_t ROUTING[ADDR_LENGTH];

struct Datagram {
    uint8_t destination[ADDR_LENGTH];
    uint8_t type;
    uint8_t message[MESSAGE_LENGTH];
};

struct Packet {
    uint8_t ttl;
    uint8_t totalLength;
    uint8_t sender[ADDR_LENGTH];
    uint8_t receiver[ADDR_LENGTH];
    uint8_t sequence; // message count of packets tx by sender
    uint8_t source[ADDR_LENGTH];
    uint8_t hopCount; // start 0, incremented with each retransmit
    uint8_t metric; // of source-receiver link
    Datagram datagram;
};

struct NeighborTableEntry{
    uint8_t address[ADDR_LENGTH];
    uint8_t lastReceived;
    uint8_t packet_success;
    uint8_t metric;
};

struct RoutingTableEntry{
    uint8_t destination[ADDR_LENGTH]; // 4
    uint8_t nextHop[ADDR_LENGTH]; // 4
    uint8_t distance; // 1
    uint8_t lastReceived; // 1
    uint8_t metric; // 1
};

class LL2Class {
public:
    // Constructor
    LL2Class(Layer1Class *lora_1, Layer1Class *lora_2 = NULL);

    // Public access to local variables
    uint8_t messageCount();

    uint8_t* localAddress();
    int getRouteEntry();

    // User configurable settings
    void setLocalAddress(const char* macString);
    long setInterval(long interval);
    void setDutyCycle(double dutyCycle);
    void setTimestamp(uint64_t timestamp, uint32_t cTime);
    uint64_t getTimestamp(uint32_t cTime);
    void setTimeFlag(bool flag);
    void setTimestampFlag(bool flag);

    // Layer 3 tx/rx wrappers
    int writeData(Datagram datagram, size_t length);
    Packet readData();

    // Print out functions
    void getNeighborTable(char *out);
    void getRoutingTable(char *out);
    void printPacketInfo(Packet packet);
    void clearNeigbourRoutingTables(void);
    
    Packet buildRoutingWithServiceGradePacket();

    // Main init and loop functions
    int init();
    int daemon();

private:
    // Wrappers for packetBuffers
    int writeToBuffer(packetBuffer *buffer, Packet packet);
    Packet readFromBuffer(packetBuffer *buffer);

    // General purpose utility functions
    uint8_t hexDigit(char ch);
    void setAddress(uint8_t* addr, const char* macString);
    void console_printf(const char* format, ...);

    // Routing utility functions
    Packet buildPacket(uint8_t ttl, uint8_t nextHop[ADDR_LENGTH], uint8_t source[ADDR_LENGTH], uint8_t hopCount, uint8_t metric, Datagram datagram, size_t length);
    Packet buildRoutingPacket();
    uint8_t calculatePacketLoss(int entry, uint8_t sequence);
    uint8_t calculateMetric(int entry);
    int checkNeighborTable(NeighborTableEntry neighbor);
    int checkRoutingTable(RoutingTableEntry route);
    int updateNeighborTable(NeighborTableEntry neighbor, int entry);
    int updateRouteTable(RoutingTableEntry route, int entry);
    int selectRoute(uint8_t destination[ADDR_LENGTH]);
    int parseNeighbor(Packet packet);
    int parseRoutingTable(Packet packet, int n_entry);

    // Main entry point functions
    void parseForRoutes(Packet packet);
    int route(uint8_t ttl, uint8_t source[ADDR_LENGTH], uint8_t hopCount, Datagram datagram, size_t length, int broadcast);
    void receive();

    // Fifo buffer objects
    packetBuffer *rxBuffer; // L2 sending to L3
    // NOTE: there is no L2 txBuffer (i.e. L3 sending to L2 a packet to be transmitted) because I have not found a need for one yet

    // Local members, variables and tables
    uint8_t _localAddress[ADDR_LENGTH];

    Layer1Class *LoRa1;
    Layer1Class *LoRa2;
    uint8_t _messageCount;
    float _packetSuccessWeight;
    int _neighborEntry;
    int _routeEntry;
    int _routingInterval;
    int _lTime;
    uint64_t _timestamp;
    bool _setTimestamp;
    bool _setTime;
    int _disableRoutingPackets;
    int _dutyInterval;
    double _dutyCycle;

    long _startTime;
    long _lastRoutingTime;
    long _lastTransmitTime;
    NeighborTableEntry _neighborTable[255];
    RoutingTableEntry _routeTable[255];
    union timestamp_
    {
        uint64_t time_;
        uint8_t timeArr[8];
        /* data */
    }time_stamp;
};

#endif
