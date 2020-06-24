/*
 * VanBus packet receiver for ESP8266 / ESP32
 *
 * Written by Erik Tromp
 *
 * Version 0.1 - June, 2020
 *
 * MIT license, all text above must be included in any redistribution.   
 */

#include "VanBus.h"

// Returns the IDEN field of a VAN packet
uint16_t TVanPacketRxDesc::Iden() const
{
    return bytes[1] << 4 | bytes[2] >> 4;
} // TVanPacketRxDesc::Iden

// Returns the Flags field of a VAN packet
uint16_t TVanPacketRxDesc::Flags() const
{
    // Bits:
    // 3 : always 1
    // 2 (Request AcK, RAK) : 1 = requesting ack; 0 = no ack requested
    // 1 (Read/Write, R/W) : 1 = read; 0 = write
    // 0 (Remote Transmission Request, RTR; only when R/W == 1) : 1 = request for in-frame response
    return bytes[2] & 0x0F;
} // TVanPacketRxDesc::Flags

// Returns a pointer to the data bytes of a VAN packet
const uint8_t* TVanPacketRxDesc::Data() const
{
    return bytes + 3;
} // TVanPacketRxDesc::Data

// Returns the data length of a VAN packet
int TVanPacketRxDesc::DataLen() const
{
    // Total size minus SOF (1 byte), IDEN (1.5 bytes), COM (0.5 bytes) and CRC + EOD (2 bytes)
    return size - 5;
} // TVanPacketRxDesc::DataLen

static const uint16_t CRC_POLYNOM = 0x0F9D;

// Calculates the CRC of a VAN packet
uint16_t TVanPacketRxDesc::Crc() const
{
    uint16_t crc16 = 0x7FFF;

    for (int i = 1; i < size - 2; i++) // Skip first byte (SOF, 0x0E) and last 2 (CRC)
    {
        // Update CRC
        uint8_t byte = bytes[i];

        for (int j = 0; j < 8; j++)
        {
            uint16_t bit = crc16 & 0x4000;
            if (byte & 0x80) bit ^= 0x4000;
            byte <<= 1;
            crc16 <<= 1;
            if (bit) crc16 ^= CRC_POLYNOM;
        } // for
    } // if

    crc16 ^= 0x7FFF;

    // Shift left 1 bit to turn 15 bit result into 16 bit representation
    crc16 <<= 1;

    return crc16;
} // TVanPacketRxDesc::Crc

// Checks the CRC value of a VAN packet
bool TVanPacketRxDesc::CheckCrc() const
{
    uint16_t crc16 = 0x7FFF;

    for (int i = 1; i < size; i++) // Skip first byte (SOF, 0x0E)
    {
        // Update CRC
        unsigned char byte = bytes[i];

        for (int j = 0; j < 8; j++)
        {
            uint16_t bit = crc16 & 0x4000;
            if (byte & 0x80) bit ^= 0x4000;
            byte <<= 1;
            crc16 <<= 1;
            if (bit) crc16 ^= CRC_POLYNOM;
        } // for
    } // if

    crc16 &= 0x7FFF;

    // Packet is OK if crc16 == 0x19B7
    return crc16 == 0x19B7;
} // TVanPacketRxDesc::CheckCrc

// Checks the CRC value of a VAN packet. If not, tries to repair it by flipping each bit.
// Note: let's keep the counters sane by calling this only once.
bool TVanPacketRxDesc::CheckCrcAndRepair()
{
    if (CheckCrc()) return true;

    VanBus.nCorrupt++;

    for (int atByte = 0; atByte < size; atByte++)
    {
        for (int atBit = 0; atBit < 8; atBit++)
        {
            uint8_t mask = 1 << atBit;
            bytes[atByte] ^= mask;  // Flip
            if (CheckCrc())
            {
                VanBus.nRepaired++;
                return true;
            } // if
            bytes[atByte] ^= mask;  // Flip back            
        } // for
    } // for

    return false;
} // TVanPacketRxDesc::CheckCrcAndRepair

// Dumps the raw packet bytes to a stream
void TVanPacketRxDesc::DumpRaw(Stream& s) const
{
    s.printf("Raw: #%04u (%u/%u) %d ", seqNo % 10000, isrDebugPacket.samples[0].slot, RX_QUEUE_SIZE, size);

    if (size >= 1) s.printf("%02X ", bytes[0]);  // SOF
    if (size >= 3)
    {
        s.printf("%03X %s ", Iden(), FlagsStr());
    } // if

    for (int i = 3; i < size; i++)
    {
        s.printf("%02X%c", bytes[i], i < size - 1 ? '-' : ' ');
    } // for

    s.print(AckStr());
    s.print(" ");
    s.print(ResultStr());
    s.printf(" %04X", Crc());
    s.printf(" %s", CheckCrc() ? "CRC_OK" : "CRC_ERROR");

    s.println();
} // TVanPacketRxDesc::DumpRaw

// Initializes the VAN packet receiver
void TVanPacketRxQueue::Setup(uint8_t rxPin)
{
    pin = rxPin;
    pinMode(rxPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pin), PinChangeIsr, CHANGE);
    timer1_isr_init();
    timer1_disable();
    timer1_attachInterrupt(WaitAckIsr);
} // TVanPacketRxQueue::Setup

// Receives a VAN bus packet by copying it into 'pkt'
bool TVanPacketRxQueue::Receive(TVanPacketRxDesc& pkt)
{
    if (! Available()) return false;

    // Copy the whole packet descriptor out (including the debug info)
    pkt = *tail;

    // Instead of copying out, we could also just pass the pointer to the descriptor. However, then we would have to
    // wait with freeing the descriptor, thus keeping one precious queue slot allocated. It is better to copy the
    // packet into the (usually stack-allocated) memory of 'pkt' and free the queue slot as soon as possible. The
    // caller can now keep the packet as long as needed.

    // Indicate packet buffer is available for next packet
    // TODO - not really necessary to do this
    tail->Init();

    // Free the descriptor
    noInterrupts();
    tail++;
    if (tail == end) tail = pool;  // roll over
    _full = false;  // One packet processed, so there is always space now
    interrupts();

    return true;
} // TVanPacketRxQueue::Receive

// Simple function to generate a string representation of a float value.
// Note: uses a statically allocated buffer, so don't call twice within the same printf invocation.
char* FloatToStr(float f, int prec)
{
    #define MAX_FLOAT_SIZE 12
    static char buffer[MAX_FLOAT_SIZE];

    dtostrf(f, MAX_FLOAT_SIZE - 1, prec, buffer);

    // Strip leading spaces
    char* strippedStr = buffer;
    while (isspace(*strippedStr)) strippedStr++;

    return strippedStr;
} // FloatToStr

// Dumps packet statistics
void TVanPacketRxQueue::DumpStats(Stream& s) const
{
    uint32_t pktCount = GetCount();

    // FloatToStr() uses a shared buffer, so only one invocation per printf
    s.printf_P(
        PSTR("received pkts: %lu, corrupt: %lu (%s%%)"),
        pktCount,
        nCorrupt,
        pktCount == 0
            ? "-.---"
            : FloatToStr(100.0 * nCorrupt / pktCount, 3));

    s.printf_P(
        PSTR(", repaired: %lu (%s%%)"),
        nRepaired,
        nCorrupt == 0
            ? "---" 
            : FloatToStr(100.0 * nRepaired / nCorrupt, 0));

    uint32_t overallCorrupt = nCorrupt - nRepaired;
    s.printf_P(
        PSTR(", overall: %lu (%s%%)\n"),
        overallCorrupt,
        pktCount == 0
            ? "-.---" 
            : FloatToStr(100.0 * overallCorrupt / pktCount, 3));
} // TVanPacketRxQueue::DumpStats

// F_CPU is set by the Arduino IDE option as chosen in menu Tools > CPU Frequency. It is always a multiple of 80000000.
#define CPU_F_FACTOR (F_CPU / 80000000)

// Calculate number of bits from a number of elapsed CPU cycles
// TODO - does ICACHE_RAM_ATTR have any effect on an inline function?
inline unsigned int ICACHE_RAM_ATTR nBitsFromCycles(uint32_t nCycles, uint32_t& jitter)
{
    // Here is the heart of the machine; lots of voodoo magic here...

    // Theory:
    // - VAN bus rate = 125 kbit/sec = 125 000 bits/sec
    //   1 bit = 1/125000 = 0.000008 sec = 8.0 usec
    // - CPU rate is 80 MHz
    //   1 cycle @ 80 MHz = 0.0000000125 sec = 0.0125 usec
    // --> So, 1 VAN-bus bit is 8.0 / 0.0125 = 640 cycles
    //
    // Real-world test #1:
    //   1 bit time varies between 636 and 892 cycles
    //   2 bit times varies between 1203 and 1443 cycles
    //   3 bit times varies between 1833 and 2345 cycles
    //   4 bit times varies between 2245 and 2786 cycles
    //   5 bit times varies between 3151 and 3160 cycles
    //   6 bit times varies between 4163 and 4206 cycles
    //
    // Real-world test #2:
    //   1 bit time varies between 612 and 800 cycles   
    //   2 bit times varies between 1222 and 1338 cycles
    //   3 bit times varies between 1863 and 1976 cycles
    //   4 bit times varies between 2510 and 2629 cycles
    //   5 bit times varies between 3161 and 3255 cycles
    //                                                  

    // Prevent calculations with roll-over (e.g. if nCycles = 2^32 - 1, adding 347 will roll over to 346)
    // Well no... do we really care?
    //if (nCycles > 999999 * 694 - 347) return 999999;

    // Sometimes, samples are stretched, because the ISR is called too late. If that happens,
    // we must compress the "sample time" for the next bit.
    nCycles += jitter;
    jitter = 0;
    if (nCycles < 1124 * CPU_F_FACTOR)
    {
        if (nCycles > 800 * CPU_F_FACTOR) jitter = nCycles - 800 * CPU_F_FACTOR;
        return 1;
    } // if
    if (nCycles < 1744 * CPU_F_FACTOR)
    {
        if (nCycles > 1380 * CPU_F_FACTOR) jitter = nCycles - 1380 * CPU_F_FACTOR;
        return 2;
    } // if
    if (nCycles < 2383 * CPU_F_FACTOR)
    {
        if (nCycles > 2100 * CPU_F_FACTOR) jitter = nCycles - 2100 * CPU_F_FACTOR;
        return 3;
    } // if
    if (nCycles < 3045 * CPU_F_FACTOR)
    {
        if (nCycles > 2655 * CPU_F_FACTOR) jitter = nCycles - 2655 * CPU_F_FACTOR;
        return 4;
    } // if
    if (nCycles < 3665 * CPU_F_FACTOR)
    {
        if (nCycles > 3300 * CPU_F_FACTOR) jitter = nCycles - 3300 * CPU_F_FACTOR;
        return 5;
    } // if

    // We hardly ever get here. And if we do, the "number of bits" is not so important.
    //unsigned int nBits = (nCycles + 347) / 694;
    //unsigned int nBits = (nCycles + 335) / 640; // ( 305...944==1; 945...1584==2; 1585...2224==3; 2225...2864==4, ...
    //unsigned int nBits = (nCycles + 347) / 660;
    //unsigned int nBits = (nCycles + 250) / 640;
    unsigned int nBits = (nCycles + 300 * CPU_F_FACTOR) / 640 * CPU_F_FACTOR;

    return nBits;
} // nBitsFromCycles

// If the timeout expires, the packet is DONE. 'ack' has already been initially set to VAN_NO_ACK,
// and then to VAN_ACK if a new bit was received within the time-out period.
void ICACHE_RAM_ATTR WaitAckIsr()
{
    timer1_disable();
    VanBus._AdvanceHead();
} // WaitAckIsr

// Pin level change interrupt handler
static void ICACHE_RAM_ATTR PinChangeIsr()
{
    // The logic is:
    // - if pinLevelChangedTo == LOGICAL_HIGH, we've just had a series of LOGICAL_LOW bits.
    // - if pinLevelChangedTo == LOGICAL_LOW, so we've just had a series of LOGICAL_HIGH bits.
    //int pinLevelChangedTo = digitalRead(VanBus.pin);
    int pinLevelChangedTo = GPIP(VanBus.pin);  // GPIP() is faster than digitalRead()?
    static int prevPinLevelChangedTo = LOW;
    
    static uint32_t prev = 0;
    uint32_t curr = ESP.getCycleCount();  // Store cycle counter value as soon as possible
    uint32_t nCycles = curr - prev;  // Arithmetic has safe roll-over

    // Return quickly when it is a spurious interrupt (pin level not changed).
    // If been away for a long time (~ 15 bit times) then skip this check.
    if (nCycles < 10000 && pinLevelChangedTo == prevPinLevelChangedTo) return;
    prevPinLevelChangedTo = pinLevelChangedTo;

    prev = curr;

    static uint32_t jitter = 0;
    unsigned int nBits = nBitsFromCycles(nCycles, jitter);
 
    // Is there space in the circular buffer?
    if (VanBus._IsFull()) return;

    TVanPacketRxDesc* head = VanBus._head;
    PacketReadState_t state = head->state;

//#if 0
    // Record some data to be used for debugging outside this ISR

    TIsrDebugPacket* isrDebugPacket = &head->isrDebugPacket;
    TIsrDebugData* debugIsr = isrDebugPacket->samples + isrDebugPacket->at;

    // Only write into buffer if there is space
    if (state != DONE && isrDebugPacket->at < ISR_DEBUG_BUFFER_SIZE)
    {
        debugIsr->pinLevel = pinLevelChangedTo;
        debugIsr->nCycles = nCycles;
        debugIsr->slot = head - VanBus.pool;
    } // if

    // Just before returning from this ISR, record some data for debugging
    #define return \
    { \
        if (state != DONE && isrDebugPacket->at < ISR_DEBUG_BUFFER_SIZE) \
        { \
            debugIsr->pinLevelAtReturnFromIsr = GPIP(VanBus.pin); \
            debugIsr->nCyclesProcessing = ESP.getCycleCount() - curr; \
            isrDebugPacket->at++; \
        } \
        return; \
    }
//#endif

    static unsigned int atBit = 0;
    static uint16_t readBits = 0;

    if (state == VACANT)
    {
        // Wait until we've seen a series of "1" bits
        if (pinLevelChangedTo == LOGICAL_LOW)
        {
            head->state = SEARCHING;
            head->ack = VAN_NO_ACK;
            atBit = 0;
            readBits = 0;
            head->size = 0;
        } // if

        return;
    } // if

    if (state == WAITING_ACK)
    {
        head->ack = VAN_ACK;

        // The timer ISR will do this
        //VanBus._AdvanceHead();

        return;
    } // if

    if (state != SEARCHING && state != LOADING) return;

    // During packet reception, the "Enhanced Manchester" encoding guarantees at most 5 bits are the same,
    // except during EOD when it can be 6.
    // However, sometimes the Manchester bit is missed (bug in driver chip?). Let's be tolerant with that, and just
    // pretend it was there, by accepting up to 9 equal bits.
    if (nBits > 9)
    {
        if (state == SEARCHING)
        {
            atBit = 0;
            readBits = 0;
            head->size = 0;
            return;
        } // if

        head->result = ERROR_NBITS;
        VanBus._AdvanceHead();

        return;
    } // if

    // Wait at most one extra bit time for the Manchester bit (5 --> 4, 10 --> 9)
    // But... Manchester bit error at bit 10 is needed to see EOD, so skip that.
    if (nBits > 1
        && (atBit + nBits == 5
            /*|| (head->size < 5 && atBit + nBits == 9)*/))
    {
        nBits--;
        jitter = 500;
    } // if

    atBit += nBits;
    readBits <<= nBits;

    // Remember: if pinLevelChangedTo == LOGICAL_LOW, we've just had a series of LOGICAL_HIGH bits
    uint16_t pattern = 0;
    if (pinLevelChangedTo == LOGICAL_LOW) pattern = (1 << nBits) - 1;
    readBits |= pattern;

    if (atBit >= 10)
    {
        atBit -= 10;

        // uint16_t, not uint8_t: we are reading 10 bits per byte ("Enhanced Manchester" encoding)
        uint16_t currentByte = readBits >> atBit;

        if (state == SEARCHING)
        {
            // First 10 bits must be 00 0011 1101 (0x03D) (SOF, Start Of Frame)
            if (currentByte != 0x03D)
            {
                head->state = VACANT;
                return;
            } // if

            head->state = LOADING;
        } // if

        // Get ready for next byte
        readBits &= (1 << atBit) - 1;

        // Remove the 2 Manchester bits 'm'; the relevant 8 bits are 'X':
        //   9 8 7 6 5 4 3 2 1 0
        //   X X X X m X X X X m
        uint8_t readByte = (currentByte >> 2 & 0xF0) | (currentByte >> 1 & 0x0F);

        head->bytes[head->size++] = readByte;

        // EOD detected?
        if ((currentByte & 0x003) == 0)
        {
            // Not really necessary to do this within the limited time there is inside this ISR
            #if 0
            if (   atBit != 0  // EOD must end with a transition 0 -> 1
                || (currentByte >> 1 & 0x20) == (currentByte & 0x20))
            {
                head->result = ERROR_MANCHESTER;
            } // if
            #endif

            head->state = WAITING_ACK;

            // Set a timeout for the ACK bit
            timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
            timer1_write(12 * 5); // 1.5 time slots = 1.5 * 8 us = 12 us. TODO - correct value?

            return;
        } // if

        if (head->size >= MAX_PACKET_SIZE)
        {
            head->result = ERROR_MAX_PACKET;
            VanBus._AdvanceHead();

            return;
        } // if

        // Not really necessary to do this within the limited time there is inside this ISR
        #if 0
        // Check "Enhanced Manchester" encoding: bit 5 must be inverse of bit 6, and bit 0 must be inverse of bit 1
        if (   (currentByte >> 1 & 0x20) == (currentByte & 0x20)
            || (currentByte >> 1 & 0x01) == (currentByte & 0x01) )
        {
            head->result = ERROR_MANCHESTER;
            return;
        } // if
        #endif
    } // if

    return;

#undef return

} // PinChangeIsr

void TIsrDebugPacket::Dump(Stream& s) const
{
    // Parse packet outside ISR

    unsigned int atBit = 0;
    unsigned int readBits = 0;
    boolean eodSeen = false;
    uint32_t totalCycles;
    uint32_t totalBits;
    int size = 0;
    int i = 0;

    #define resetBuffer() \
    { \
        atBit = 0; \
        readBits = 0; \
        eodSeen = false; \
        totalCycles = 0; \
        totalBits = 0; \
        size = 0; \
    }

    while (at > 2 && i < at)
    {
        const TIsrDebugData* tailBuffer = samples + i;
        if (i == 0)
        {
            s.printf_P(PSTR("%sSlot # CPU nCycles -> nBits pinLVLs data\n"), tailBuffer->slot >= 10 ? " " : "");
        } // if

        if (i <= 1) resetBuffer();

        s.printf("#%d", tailBuffer->slot);

        s.printf("%4u", i);

        uint32_t nCyclesProcessing = tailBuffer->nCyclesProcessing;
        if (nCyclesProcessing > 999) s.printf(">999 ");
        else s.printf("%4lu ", nCyclesProcessing);

        uint32_t nCycles = tailBuffer->nCycles;
        if (nCycles > 999999)
        {
            totalCycles = 0;
            //s.printf(">999999 (%7lu -> %5u)", totalCycles, 0);
            s.printf(">999999", totalCycles);
        }
        else
        {
            totalCycles += nCycles;
            // Note: nBitsFromCycles has state information ("static uint32_t jitter"), so calling
            // twice makes result different
            //s.printf("%7lu (%7lu -> %5u)", nCycles, totalCycles, nBitsFromCycles(totalCycles));
            s.printf("%7lu", nCycles);
        } // if
        s.print(" -> ");

        static uint32_t jitter = 0;
        unsigned int nBits = nBitsFromCycles(nCycles, jitter);

        if (nBits > 9999)
        {
            totalBits = 0;
            s.printf(">9999");
            //s.printf(">9999 (%5u)", totalBits);
        }
        else
        {
            totalBits += nBits;
            s.printf("%5u", nBits);
            //s.printf("%5u (%5u)", nBits, totalBits);
        } // if

        // Wait at most one extra bit time for the Manchester bit (5 --> 4, 10 --> 9)
        // But... Manchester bit error at bit 10 is needed to see EOD, so skip that.
        if (nBits > 1
            && (atBit + nBits == 5
                || (size < 5 && atBit + nBits == 10)))
        {
            nBits--;
            jitter = 500;
            s.printf("*%u ", nBits);
        } // if

        unsigned char pinLevelChangedTo = tailBuffer->pinLevel;
        unsigned char pinLevelAtReturnFromIsr = tailBuffer->pinLevelAtReturnFromIsr;
        s.printf(" \"%u\",\"%u\" ", pinLevelChangedTo, pinLevelAtReturnFromIsr);

        // Sometimes the ISR is called very late; this is recognized as difference in pin levels:
/*
        if (nBits > 1 && pinLevelChangedTo != pinLevelAtReturnFromIsr)
        {
            nBits--;
            s.printf("*%u ", nBits);
        } // if
*/

        if (nBits > 6)
        {
            // Show we just had a long series of 1's (shown as '1.....') or 0's (shown as '-.....')
            s.print(pinLevelChangedTo == LOGICAL_LOW ? "1....." : "-.....");
            s.println();

            resetBuffer();
            i++;
            continue;
        } // if

        // Print the read bits one by one, in a column of 6
        for (int i = 0; i < nBits; i++) s.print(pinLevelChangedTo == LOGICAL_LOW ? "1" : "-");
        for (int i = nBits; i < 6; i++) s.print(" ");

        // Print current value
        s.printf(" %04X << %1u", readBits, nBits);

        atBit += nBits;
        readBits <<= nBits;

        // Print new value
        s.printf(" = %04X", readBits);

        uint8_t pattern = 0;
        if (pinLevelChangedTo == LOGICAL_LOW) pattern = (1 << nBits) - 1;
        readBits |= pattern;

        s.printf(" | %2X = %04X", pattern, readBits);

        if (eodSeen)
        {
            if (pinLevelChangedTo == LOGICAL_LOW && nBits == 1)
            {
                s.print(" ACK");
                resetBuffer();
            }
        } // if

        else if (atBit >= 10)
        {
            atBit -= 10;

            // uint16_t, not uint8_t: we are reading 10 bits per byte ("Enhanced Manchester" encoding)
            uint16_t currentByte = readBits >> atBit;

            s.printf(" >> %u = %03X", atBit, currentByte);

            // Get ready for next byte
            readBits &= (1 << atBit) - 1;

            // Remove the 2 manchester bits 'm'; the relevant 8 bits are 'X':
            //   9 8 7 6 5 4 3 2 1 0
            //   X X X X m X X X X m
            uint8_t readByte = (currentByte >> 2 & 0xF0) | (currentByte >> 1 & 0x0F);

            s.printf(" --> %02X (#%d)", readByte, size);
            size++;

            // EOD detected?
            if ((currentByte & 0x003) == 0)
            {
                if (
                    atBit != 0  // EOD must end with a transition 0 -> 1
                    || (currentByte >> 1 & 0x20) == (currentByte & 0x20)
                   )
                {
                    s.print(" Manchester error");
                } // if

                eodSeen = true;
                s.print(" EOD");
            } // if

            // Check if bit 5 is inverse of bit 6, and if bit 0 is inverse of bit 1
            // TODO - keep this, or ignore?
            else if (   (currentByte >> 1 & 0x20) == (currentByte & 0x20)
                     || (currentByte >> 1 & 0x01) == (currentByte & 0x01) )
            {
                s.print(" Manchester error");
            } // if
        } // if

        s.println();

        i++;
    } // while
} // TIsrDebugPacket::Dump

TVanPacketRxQueue VanBus;
