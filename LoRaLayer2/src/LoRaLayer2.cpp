#include <LoRaLayer2.h>

uint8_t BROADCAST[ADDR_LENGTH] = {0xff, 0xff, 0xff, 0xff};
uint8_t LOOPBACK[ADDR_LENGTH] = {0x00, 0x00, 0x00, 0x00};
uint8_t ROUTING[ADDR_LENGTH] = {0xaf, 0xff, 0xff, 0xff};

/* LoRaLayer2 Class
 */
LL2Class::LL2Class(Layer1Class *lora_1, Layer1Class *lora_2)
    : LoRa1{lora_1},
      LoRa2{lora_2},
      _messageCount(0),
      _packetSuccessWeight(1),
      _neighborEntry(0),
      _routeEntry(0),
      _routingInterval(15000),
      _disableRoutingPackets(0),
      _dutyInterval(0),
      _dutyCycle(1),
      _lTime(0),
      _timestamp(0),
      _setTime(false),
      _setTimestamp(false)
{
    rxBuffer = new packetBuffer;
};

/* Public access to local variables
 */
uint8_t LL2Class::messageCount()
{
    return _messageCount;
}

uint8_t *LL2Class::localAddress()
{
    return _localAddress;
}

int LL2Class::getRouteEntry()
{
    return _routeEntry;
}

/* General purpose utility functions
 */
uint8_t LL2Class::hexDigit(char ch)
{
    if (('0' <= ch) && (ch <= '9'))
    {
        ch -= '0';
    }
    else
    {
        if (('a' <= ch) && (ch <= 'f'))
        {
            ch += 10 - 'a';
        }
        else
        {
            if (('A' <= ch) && (ch <= 'F'))
            {
                ch += 10 - 'A';
            }
            else
            {
                ch = 16;
            }
        }
    }
    return ch;
}

void LL2Class::setAddress(uint8_t *addr, const char *macString)
{
    for (int i = 0; i < ADDR_LENGTH; ++i)
    {
        addr[i] = hexDigit(macString[2 * i]) << 4;
        addr[i] |= hexDigit(macString[2 * i + 1]);
    }
}

void LL2Class::console_printf(const char *format, ...)
{
    char *str = (char *)malloc(255);
    va_list args;
    va_start(args, format);
    size_t len = vsprintf(str, format, args);
    va_end(args);

    uint8_t ttl = 30;
    uint8_t totalLength = len + 5 + HEADER_LENGTH;
    Packet packet = {ttl, totalLength};
    Datagram datagram = {0xff, 0xff, 0xff, 0xff, 'i'};
    memcpy(datagram.message, str, len);
    memcpy(&packet.datagram, &datagram, len + 5);
    writeToBuffer(rxBuffer, packet);
    free(str);
}

/* User configurable settings
 */
void LL2Class::setLocalAddress(const char *macString)
{
    setAddress(_localAddress, macString);
}

long LL2Class::setInterval(long interval)
{
    if (interval == 0)
    {
        _disableRoutingPackets = 1; // ReAcive
    }
    else
    {
        _disableRoutingPackets = 0; // Proacive
        _routingInterval = interval;
    }
    return _routingInterval;
}

void LL2Class::setDutyCycle(double dutyCycle)
{
    _dutyCycle = dutyCycle;
}

/* private wrappers for packetBuffers
 */
int LL2Class::writeToBuffer(packetBuffer *buffer, Packet packet)
{
    BufferEntry entry;
    memcpy(&entry, &packet, packet.totalLength);
    entry.length = packet.totalLength;
    return buffer->write(entry);
}

Packet LL2Class::readFromBuffer(packetBuffer *buffer)
{
    Packet packet;
    BufferEntry entry = buffer->read();
    memcpy(&packet, &entry.data[0], sizeof(packet));
    return packet;
}

/* Layer 3 tx/rx wrappers
 */
int LL2Class::writeData(Datagram datagram, size_t length)
{
#ifdef LL2_DEBUG
    Serial.printf("LoRaLayer2::writeData(): datagram.message = ");
    for (int i = 0; i < length - 5; i++)
    {
        Serial.printf("%c", datagram.message[i]);
    }
    Serial.printf("\r\n");
#endif
    uint8_t hopCount = 0; // inital hopCount of zero
    int broadcast = 1;    // allow broadcasts to be sent
    return route(DEFAULT_TTL, _localAddress, hopCount, datagram, length, broadcast);
}

Packet LL2Class::readData()
{
    Packet packet = readFromBuffer(rxBuffer);
#ifdef LL2_DEBUG
    if (packet.totalLength > 0)
    {
        Serial.printf("LoRaLayer2::readData(): packet.datagram.message = ");
        for (int i = 0; i < packet.totalLength - HEADER_LENGTH - 5; i++)
        {
            Serial.printf("%c", packet.datagram.message[i]);
        }
        Serial.printf("\r\n");
    }
#endif
    return packet;
}

/* Print out functions, for convenience
 */
void LL2Class::getNeighborTable(char *out)
{
    char *buf = out;
    buf += sprintf(buf, "Neighbor Table:\n");
    for (int i = 0; i < _neighborEntry; i++)
    {
        for (int j = 0; j < ADDR_LENGTH; j++)
        {
            buf += sprintf(buf, "%02x", _neighborTable[i].address[j]);
        }
        buf += sprintf(buf, " %3d ", _neighborTable[i].metric);
        buf += sprintf(buf, "\r\n");
    }
}

void LL2Class::getRoutingTable(char *out)
{
    char *buf = out;
    buf += sprintf(buf, "Routing Table: total routes %d\r\n", _routeEntry);
    for (int i = 0; i < _routeEntry; i++)
    {
        buf += sprintf(buf, "%d hops from ", _routeTable[i].distance);
        for (int j = 0; j < ADDR_LENGTH; j++)
        {
            buf += sprintf(buf, "%02x", _routeTable[i].destination[j]);
        }
        buf += sprintf(buf, " via ");
        for (int j = 0; j < ADDR_LENGTH; j++)
        {
            buf += sprintf(buf, "%02x", _routeTable[i].nextHop[j]);
        }
        buf += sprintf(buf, " metric %3d ", _routeTable[i].metric);
        buf += sprintf(buf, "\r\n");
    }
}

void LL2Class::printPacketInfo(Packet packet)
{

    // TODO: LL2 shouldn't use Serial.printf
    /*
    Serial.printf("ttl: %d\r\n", packet.ttl);
    Serial.printf("length: %d\r\n", packet.totalLength);
    Serial.printf("source: ");
    for(int i = 0 ; i < ADDR_LENGTH ; i++){
        Serial.printf("%x", packet.sender[i]);
    }
    Serial.printf("\r\n");
    Serial.printf("destination: ");
    for(int i = 0 ; i < ADDR_LENGTH ; i++){
        Serial.printf("%x", packet.receiver[i]);
    }
    Serial.printf("\r\n");
    Serial.printf("sequence: %02x\r\n", packet.sequence);
    Serial.printf("data: ");
    for(int i = 0 ; i < packet.totalLength-HEADER_LENGTH ; i++){
        Serial.printf("%02x", packet.datagram[i]);
    }
    Serial.printf("\r\n");
    */
}

/* Routing utility functions
 */

Packet LL2Class::buildPacket(uint8_t ttl, uint8_t nextHop[ADDR_LENGTH], uint8_t source[ADDR_LENGTH], uint8_t hopCount, uint8_t metric, Datagram datagram, size_t length)
{
    uint8_t totalLength = HEADER_LENGTH + length;
    Packet packet = {ttl, totalLength};
    memcpy(packet.sender, _localAddress, ADDR_LENGTH);
    memcpy(packet.receiver, nextHop, ADDR_LENGTH);
    packet.sequence = messageCount();
    memcpy(packet.source, source, ADDR_LENGTH);
    packet.hopCount = hopCount;
    packet.metric = metric;
    memcpy(&packet.datagram, &datagram, length);
    return packet;
}

void LL2Class::setTimeFlag(bool flag){
    _setTime = flag;
}

void LL2Class::setTimestampFlag(bool flag){
    _setTimestamp = flag;
}

void LL2Class::setTimestamp(uint64_t timestamp, uint32_t cTime)
{
    if (_setTime)
    {
        _lTime = cTime;
        _timestamp = timestamp;
    }
}

uint64_t LL2Class::getTimestamp(uint32_t cTime)
{
    if (_timestamp > 0 && _lTime > 0)
    {
        return _timestamp + (uint64_t)cTime - (uint64_t)_lTime;
    }
    
    return 0;
}

Packet LL2Class::buildRoutingPacket()
{
    uint8_t data[DATA_LENGTH];
    int dataLength = 0;
    if (_setTimestamp)
    {
        dataLength = 8;
        time_stamp.time_ = getTimestamp(millis());
        //  Add time stamp to every routing Packet
        memcpy(data, time_stamp.timeArr, 8);
    }
    int routesPerPacket = _routeEntry;
    //TODO add the entire routing table into N routing packets
    if (_routeEntry >= MAX_ROUTES_PER_PACKET - 1)
    {
        routesPerPacket = MAX_ROUTES_PER_PACKET - 1;
    }
    for (int i = 0; i < routesPerPacket; i++)
    {
        for (int j = 0; j < ADDR_LENGTH; j++)
        {
            data[dataLength] = _routeTable[i].destination[j];
            dataLength++;
        }
        data[dataLength] = _routeTable[i].distance;
        dataLength++;
        data[dataLength] = _routeTable[i].metric;
        dataLength++;
        for (int j = 0; j < ADDR_LENGTH; j++)
        {
            data[dataLength] = _routeTable[i].nextHop[j];
            dataLength++;
        }
    }
    uint8_t nextHop[ADDR_LENGTH] = {0xaf, 0xff, 0xff, 0xff};
    // copy raw data into datagram to create packet
    Datagram datagram; //= { 0xaf, 0xff, 0xff, 0xff, 'r' };
    memcpy(&datagram, &data, dataLength);

    Packet packet = buildPacket(1, nextHop, localAddress(), 0, 0, datagram, dataLength);
    return packet;
}

double calculateAirtime(double length, double spreadingFactor, double explicitHeader, double lowDR, double codingRate, double bandwidth)
{
    double timePerSymbol = pow(2, spreadingFactor) / (bandwidth);
    double arg = ceil(((8 * length) - (4 * spreadingFactor) + 28 + 16 - (20 * (1 - explicitHeader))) / (4 * (spreadingFactor - 2 * lowDR))) * (codingRate);
    double symbolsPerPayload = 8 + (fmax(arg, 0.0));
    double timePerPayload = timePerSymbol * symbolsPerPayload;
    return timePerPayload;
}

uint8_t LL2Class::calculatePacketLoss(int entry, uint8_t sequence)
{
    uint8_t packet_loss = 0xFF;
    uint8_t sequence_diff = sequence - _neighborTable[entry].lastReceived;
    if (sequence_diff == 0)
    {
        // this is first packet received from neighbor
        // assume perfect packet success
        _neighborTable[entry].packet_success = 0xFF;
        packet_loss = 0x00;
    }
    else if (sequence_diff == 1)
    {
        // do not decrease packet success rate
        packet_loss = 0x00;
    }
    else if (sequence_diff > 1 && sequence_diff < 16)
    {
        // decrease packet success rate by difference
        packet_loss = 0x10 * sequence_diff;
    }
    // no packet received recently
    // assume complete packet loss
    return packet_loss;
}

uint8_t LL2Class::calculateMetric(int entry)
{
    // other metric values could be introduced here, currently only packet success is considered
    float weightedPacketSuccess = ((float)_neighborTable[entry].packet_success) * _packetSuccessWeight;
    uint8_t metric = weightedPacketSuccess;
    return metric;
}

int LL2Class::checkNeighborTable(NeighborTableEntry neighbor)
{
    int entry = _neighborEntry;
    for (int i = 0; i < _neighborEntry; i++)
    {
        // had to use memcmp instead of strcmp?
        if (memcmp(neighbor.address, _neighborTable[i].address, sizeof(neighbor.address)) == 0)
        {
            entry = i;
        }
    }
    return entry;
}

int LL2Class::checkRoutingTable(RoutingTableEntry route)
{
    int entry = _routeEntry; // assume this is a new route
    for (int i = 0; i < _routeEntry; i++)
    {
        if (memcmp(route.destination, localAddress(), sizeof(route.destination)) == 0)
        {
            // this is me don't add to routing table
            entry = -1;
            return entry;
        }
        else if (memcmp(route.destination, _routeTable[i].destination, sizeof(route.destination)) == 0)
        {
            if (memcmp(route.nextHop, _routeTable[i].nextHop, sizeof(route.nextHop)) == 0)
            {
                // already have this exact route, update metric
                entry = i;
                return entry;
            }
            else
            {
                // already have this destination, but via a different neighbor
                if (route.distance < _routeTable[i].distance)
                { // TODO: this assumes shortest route is best, which may not be true.
                    // replace route if distance is better
                    entry = i;
                }
                else if (route.distance == _routeTable[i].distance)
                {
                    if (route.metric > _routeTable[i].metric)
                    {
                        // replace route if distance is equal and metric is better
                        entry = i;
                    }
                    else
                    {
                        entry = -1;
                    }
                }
                else
                {
                    // ignore route if distance and metric are worse
                    entry = -1;
                }
                return entry;
            }
        }
    }
    return entry;
}
void LL2Class::clearNeigbourRoutingTables(void)
{
    // // NeighborTableEntry _neighborTable1[255];
    // // RoutingTableEntry _routeTable1[255];
    // memset(_neighborTable, {}, sizeof(_neighborTable));
    // memset(_routeTable, {}, sizeof(_routeTable));
    // _routeEntry = 0;
    // _neighborEntry = 0;

    // // _neighborTable = {};
    // // _routeTable = {};
    for (int i = 0; i < _neighborEntry; i++)
    {
        _neighborTable[i].packet_success = 0;
        _neighborTable[i].metric = 0;
        _neighborTable[i].lastReceived = 0;
    }
    _messageCount = 0;
    for (int i = 0; i < _routeEntry; i++)
    {
        _routeTable[i].distance = 255;
        _routeTable[i].metric = 0;
        _routeTable[i].lastReceived = 0;
    }

    //TODO update 
    // RoutingTableEntry route;
    // memcpy(route.destination, packet.sender, ADDR_LENGTH);
    // memcpy(route.nextHop, packet.sender, ADDR_LENGTH);
    // route.distance = 1;
    // route.metric = _neighborTable[n_entry].metric;
    // int r_entry = checkRoutingTable(route);
    // if (r_entry >= 0)
    // {
    //     updateRouteTable(route, r_entry);
    // }
    // return n_entry;
}
int LL2Class::updateNeighborTable(NeighborTableEntry neighbor, int entry)
{
    // copy neighbor into specified entry in neighbor table
    memcpy(&_neighborTable[entry], &neighbor, sizeof(_neighborTable[entry]));
    if (entry == _neighborEntry)
    {
        // if specified entry is the same as current count of neighbors
        // this is a new neighbor, increment neighbor count
        _neighborEntry++;
    }
    // else neighbor is just updated
    return entry;
}

int LL2Class::updateRouteTable(RoutingTableEntry route, int entry)
{
    // copy route into specified entry in routing table
    memcpy(&_routeTable[entry], &route, sizeof(_routeTable[entry]));
    if (entry == _routeEntry)
    {
        // if specified entry is the same as current count of routes
        // this is a new route, increment route count
        _routeEntry++;
    }
    // else route is just updated
    return entry;
}

int LL2Class::selectRoute(uint8_t destination[ADDR_LENGTH])
{
    int entry = -1;
    for (int i = 0; i < _routeEntry; i++)
    {
        if (memcmp(destination, _routeTable[i].destination, ADDR_LENGTH) == 0)
        {
            entry = i;
        }
    }
    return entry;
}

int LL2Class::parseNeighbor(Packet packet)
{
    // Create neighbor table entry with sender address
    NeighborTableEntry neighbor;
    memcpy(neighbor.address, packet.sender, sizeof(neighbor.address));
    // Find neighbor table entry for sender
    int n_entry = checkNeighborTable(neighbor);
    // Calculate packet loss to find metric of link
    uint8_t packet_loss = calculatePacketLoss(n_entry, packet.sequence);
    neighbor.packet_success = _neighborTable[n_entry].packet_success - packet_loss;
    neighbor.lastReceived = packet.sequence;
    neighbor.metric = calculateMetric(n_entry);
    // update neighbor table with neighbor entry
    updateNeighborTable(neighbor, n_entry);
    // update routing table with neighbor entry also
    RoutingTableEntry route;
    memcpy(route.destination, packet.sender, ADDR_LENGTH);
    memcpy(route.nextHop, packet.sender, ADDR_LENGTH);
    route.distance = 1;
    route.metric = _neighborTable[n_entry].metric;
    int r_entry = checkRoutingTable(route);
    if (r_entry >= 0)
    {
        updateRouteTable(route, r_entry);
    }
    return n_entry;
}

int LL2Class::parseRoutingTable(Packet packet, int n_entry)
{
    int entry = -1;
    int numberOfRoutes = (packet.totalLength - HEADER_LENGTH) / (2 * ADDR_LENGTH + 2);
    uint8_t data[packet.totalLength - HEADER_LENGTH];
    memcpy(data, &packet.datagram, sizeof(data));
    if (_setTimestamp)
    {
        numberOfRoutes = (packet.totalLength - HEADER_LENGTH - 8) / (2 * ADDR_LENGTH + 2);
        memcpy(data, 8 + &packet.datagram,sizeof(data) - 8);
    }
    
    for (int i = 0; i < numberOfRoutes; i++)
    {
        RoutingTableEntry route;
        NeighborTableEntry neighbor;

        memcpy(route.destination, data + (2 * ADDR_LENGTH + 2) * i, ADDR_LENGTH);
        memcpy(route.nextHop, packet.source, ADDR_LENGTH);
        route.distance = data[(2 * ADDR_LENGTH + 2) * i + ADDR_LENGTH];
        route.distance++; // add a hop to distance
        float metric = (float)data[(2 * ADDR_LENGTH + 2) * i + ADDR_LENGTH + 1];
        float hopRatio = 1 / ((float)route.distance);
        // average neighbor metric with rest of route metric
        route.metric = (uint8_t)(((float)_neighborTable[n_entry].metric) * (hopRatio) + ((float)metric) * (1 - hopRatio));
        
        memcpy(neighbor.address, route.destination, sizeof(neighbor.address));

        if ( metric == 0 && route.distance == 0)
        {
            /* if routing packet contains faraway dropped node route; 
            update routing table for this by metric = 0 and distance = 255 */
            route.metric = 0;
            route.distance = 255;
        }
        else if (memcmp(_localAddress, data + (2 * ADDR_LENGTH + 2) * i + ADDR_LENGTH + 2, ADDR_LENGTH) == 0 && checkNeighborTable(neighbor) != _neighborEntry)
        {
            /* if routing packet contains my neighbor dropped node route; 
            update table for this by metric = 0 and distance = 255 */
            memcpy(route.nextHop, _localAddress, ADDR_LENGTH);
            route.metric = 0;
            route.distance = 255;
        }
        entry = checkRoutingTable(route);
        if (entry > 0)
        {
            if (getRouteEntry() <= MAX_ROUTES_PER_PACKET-1)
            {
                updateRouteTable(route, entry);
            }
        }
    }
    return numberOfRoutes;
}

/* Entry point to build routing table
 */
void LL2Class::parseForRoutes(Packet packet)
{
    // Parse for sender address (i.e. your neighbor)
    int n_entry = parseNeighbor(packet);
    int r_entry = -1;

    //if incomming packet is routing packet
    if (memcmp(packet.receiver, ROUTING, ADDR_LENGTH) == 0)
    {
        // get timestampe from the routing packet
        if (_setTimestamp)
        {
            memcpy(time_stamp.timeArr, &packet.datagram, sizeof(time_stamp.timeArr));
            setTimestamp(time_stamp.time_, millis());
        }
        
        // packet contains routing table info, parse for routes in datagram only
        parseRoutingTable(packet, n_entry);
        return;
    }

    // Parse for receiver address route
    //if i'm a relay and not broadcaster 
    if (memcmp(packet.receiver, _localAddress, ADDR_LENGTH) != 0 &&
        memcmp(packet.receiver, BROADCAST, ADDR_LENGTH) != 0)
    {
        // packet was meant for someone else, but still parse for receiver route
        RoutingTableEntry rcv_route;
        memcpy(rcv_route.destination, packet.receiver, ADDR_LENGTH);
        memcpy(rcv_route.nextHop, packet.sender, ADDR_LENGTH);
        // assumes the node is still alive and has not moved since the sender discovered this neighbor
        rcv_route.distance = 2;
        // metric unknown until you hear a routed packet from node?
        rcv_route.metric = 0;
        r_entry = checkRoutingTable(rcv_route);
        if (r_entry >= 0)
        {
            updateRouteTable(rcv_route, r_entry);
        }
    }

    // Parse for source address route
    //if this packet from relay update the route metric
    if (memcmp(packet.sender, packet.source, ADDR_LENGTH) != 0)
    {
        // source is different from sender
        RoutingTableEntry src_route;
        memcpy(src_route.destination, packet.source, ADDR_LENGTH);
        memcpy(src_route.nextHop, packet.sender, ADDR_LENGTH);
        src_route.distance = packet.hopCount + 1;
        // average neighbor metric with rest of route metric
        float hopRatio = 1 / ((float)src_route.distance);
        float metric = ((float)_neighborTable[n_entry].metric) * (hopRatio) + ((float)packet.metric) * (1 - hopRatio);
        src_route.metric = (uint8_t)metric;
        r_entry = checkRoutingTable(src_route);
        if (r_entry >= 0)
        {
            updateRouteTable(src_route, r_entry);
        }
    }

    // Parse for destination address route
    // if i'm not broadcaster and the packet is not meant for me
    if (memcmp(packet.datagram.destination, _localAddress, ADDR_LENGTH) != 0 &&
        memcmp(packet.datagram.destination, packet.receiver, ADDR_LENGTH) != 0 &&
        memcmp(packet.datagram.destination, BROADCAST, ADDR_LENGTH) != 0)
    {
        // datagram destination is not my address, the receiver address, or the broadcast address
        RoutingTableEntry dst_route;
        memcpy(dst_route.destination, packet.datagram.destination, ADDR_LENGTH);
        memcpy(dst_route.nextHop, packet.sender, ADDR_LENGTH);
        // distance and metric are unknown until you receive packet from this destination
        dst_route.distance = 255;
        dst_route.metric = 0;
        r_entry = checkRoutingTable(dst_route);
        if (r_entry >= 0)
        {
            updateRouteTable(dst_route, r_entry);
        }
    }
    return;
}

/* Entry point to route data
 */
int LL2Class::route(uint8_t ttl, uint8_t source[ADDR_LENGTH], uint8_t hopCount, Datagram datagram, size_t length, int broadcast)
{
    int ret = -1;
    uint8_t metric = 0;
    if (ttl > 0)
    {
        if (memcmp(source, _localAddress, ADDR_LENGTH) != 0)
        {
            int src_entry = selectRoute(source);
            if (src_entry >= 0)
            {
                metric = _routeTable[src_entry].metric;
            }
            // else ERROR source route has not been added yet
            // leave metric empty?
        }

        if (memcmp(datagram.destination, LOOPBACK, ADDR_LENGTH) == 0)
        {
            // Loopback
            //  this should push back into the L3 buffer
        }
        else if (memcmp(datagram.destination, BROADCAST, ADDR_LENGTH) == 0)
        {
            // Broadcast packet, only forward if explicity told to
            if (broadcast == 1)
            {
                Packet packet = buildPacket(ttl, BROADCAST, source, hopCount, metric, datagram, length);
                ret = writeToBuffer(LoRa1->txBuffer, packet);
            }
        }
        else
        {
            int dst_entry = selectRoute(datagram.destination);
            if (dst_entry >= 0)
            {
                // Route found
                // build packet with new ttl, nextHop, and route metric
                Packet packet = buildPacket(ttl, _routeTable[dst_entry].nextHop, source, hopCount, metric, datagram, length);
                // return packet's position in buffer
                ret = writeToBuffer(LoRa1->txBuffer, packet);
            }
        }
    }
    return ret;
}

/* Receive and decide function
 */
void LL2Class::receive()
{
    Packet packet = readFromBuffer(LoRa1->rxBuffer);
    // BufferEntry entry = LoRa1->rxBuffer.read();
    // memcpy(&packet, entry.data, entry.length);

#ifdef LL2_DEBUG
    Serial.printf("LoRaLayer2::receive(): packet.datagram.message = ");
    for (int i = 0; i < packet.totalLength - HEADER_LENGTH - 5; i++)
    {
        Serial.printf("%c", packet.datagram.message[i]);
    }
    Serial.printf("\r\n");
#endif

    // check if hearing myself //memcmp(packet.sender, _localAddress, ADDR_LENGTH) == 0
    if ((packet.totalLength > 0) && (memcmp(packet.sender, _localAddress, ADDR_LENGTH) != 0))
    {
        parseForRoutes(packet);
        if (memcmp(packet.receiver, ROUTING, ADDR_LENGTH) == 0)
        {
            // packet contains routing table info, do nothing besides parse
            return;
        }
        else if (memcmp(packet.datagram.destination, _localAddress, ADDR_LENGTH) == 0 ||
                 memcmp(packet.receiver, BROADCAST, ADDR_LENGTH) == 0)
        {
            // packet is meant for me (or everyone)
            writeToBuffer(rxBuffer, packet);
        }
        else if (memcmp(packet.receiver, _localAddress, ADDR_LENGTH) == 0)
        {
            // packet is meant for someone else
            // but I am the intended forwarder
            // strip header and route new packet
            packet.ttl--;
            packet.hopCount++;
            route(packet.ttl, packet.source, packet.hopCount, packet.datagram, packet.totalLength - HEADER_LENGTH, 0);
        }
    }
    // else buffer is empty, do nothing;
    return;
}

/* Initialization function
 */
int LL2Class::init()
{
    _startTime = Layer1Class::getTime();
    _lastRoutingTime = _startTime;
    _lastTransmitTime = _startTime;
    return 0;
}

/* Main loop function
 */
int LL2Class::daemon()
{
    int ret = -1;
    // try adding a routing packet to L2toL1 buffer, if interval is up and routing is enabled
    if (Layer1Class::getTime() - _lastRoutingTime > _routingInterval && _disableRoutingPackets == 0)
    {
        Packet routingPacket = buildRoutingPacket();
        // need to init with broadcast addr and then loop through routing table to build routing table
        ret = writeToBuffer(LoRa1->txBuffer, routingPacket);
        _lastRoutingTime = Layer1Class::getTime();
    }

    // try transmitting a packet
    if (Layer1Class::getTime() - _lastTransmitTime > _dutyInterval)
    {
        int length = LoRa1->transmit();
        if (length > 0)
        {
#ifdef LL2_DEBUG
            Serial.printf("LL2::daemon(): transmitted packet of length: %d\r\n", length);
#endif
            _lastTransmitTime = Layer1Class::getTime();
            // double airtime = LoRa1->getLoRa()->getTimeOnAir(length)/1000;
            double airtime = calculateAirtime((double)length, (double)LoRa1->spreadingFactor(), 1, 0, LoRa1->getCR(), LoRa1->getBW());
            _dutyInterval = (int)ceil(airtime / _dutyCycle);
#ifdef LL2_DEBUG
            Serial.printf("airtime = %f\n _dutyInterval = %d\n", airtime, _dutyInterval);
#endif
            _messageCount = (_messageCount + 1) % 256;
        }
    }

    // see if there are any packets to be received
    // first check if any interrupts have been set,
    // then check if any packets have been added to Layer1 rxbuffer
    if (LoRa1->receive() > 0)
    {
#ifdef LL2_DEBUG
        Serial.printf("LL2Class::daemon: received packet\r\n");
#endif
        receive();
    }

    // returns sequence number of transmitted packet,
    // if no packet transmitted, will return -1
    return ret;
}
