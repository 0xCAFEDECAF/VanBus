/*
 * VanBus packet receiver for ESP8266
 *
 * Written by Erik Tromp
 *
 * Version 0.2.4 - November, 2021
 *
 * MIT license, all text above must be included in any redistribution.
 */

#include "VanBusRx.h"

static const uint16_t VAN_CRC_POLYNOM = 0x0F9D;

uint16_t _crc(const uint8_t bytes[], int size)
{
    uint16_t crc16 = 0x7FFF;

    for (int i = 1; i < size - 2; i++)  // Skip first byte (SOF, 0x0E) and last 2 (CRC)
    {
        uint8_t byte = bytes[i];

        for (int j = 0; j < 8; j++)
        {
            uint16_t bit = crc16 & 0x4000;
            if (byte & 0x80) bit ^= 0x4000;
            byte <<= 1;
            crc16 <<= 1;
            if (bit) crc16 ^= VAN_CRC_POLYNOM;
        } // for
    } // if

    crc16 ^= 0x7FFF;
    crc16 <<= 1;  // Shift left 1 bit to turn 15 bit result into 16 bit representation

    return crc16;
} // _crc

// Returns the IDEN field of a VAN packet
uint16_t TVanPacketRxDesc::Iden() const
{
    return bytes[1] << 4 | bytes[2] >> 4;
} // TVanPacketRxDesc::Iden

// Returns the Flags field of a VAN packet
uint8_t TVanPacketRxDesc::CommandFlags() const
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

// Calculates the CRC of a VAN packet
uint16_t TVanPacketRxDesc::Crc() const
{
    return _crc(bytes, size);
} // TVanPacketRxDesc::Crc

// Checks the CRC value of a VAN packet
bool TVanPacketRxDesc::CheckCrc() const
{
    uint16_t crc16 = 0x7FFF;

    for (int i = 1; i < size; i++)  // Skip first byte (SOF, 0x0E)
    {
        unsigned char byte = bytes[i];

        for (int j = 0; j < 8; j++)
        {
            uint16_t bit = crc16 & 0x4000;
            if (byte & 0x80) bit ^= 0x4000;
            byte <<= 1;
            crc16 <<= 1;
            if (bit) crc16 ^= VAN_CRC_POLYNOM;
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

    VanBusRx.nCorrupt++;

    for (int atByte = 0; atByte < size; atByte++)
    {
        for (int atBit = 0; atBit < 8; atBit++)
        {
            uint8_t mask = 1 << atBit;
            bytes[atByte] ^= mask;  // Flip

            // Is there a way to quickly re-calculate the CRC value when bit is flipped?
            if (CheckCrc())
            {
                VanBusRx.nRepaired++;
                VanBusRx.nOneBitError++;
                return true;
            } // if

            if (atBit != 7)
            {
                // Try also to flip the next bit
                uint8_t mask2 = 1 << (atBit + 1);
                bytes[atByte] ^= mask2;  // Flip
                if (CheckCrc())
                {
                    VanBusRx.nRepaired++;
                    VanBusRx.nTwoConsecutiveBitErrors++;
                    return true;
                } // if

                bytes[atByte] ^= mask2;  // Flip back
            } // if

            bytes[atByte] ^= mask;  // Flip back
        } // for
    } // for

// 2021-04-12 - Commented out. Seems like "repairing" two separate bits is in fact increasing the probability of
// getting a CRC "OK" for a packet that is in fact corrupt.
#if 0
    // Flip two bits - is this pushing it? Maybe limit the following to the shorter packets, e.g. up to say 15 bytes?
    for (int atByte1 = 0; atByte1 < size; atByte1++)
    {
        // This may take really long...
        wdt_reset();
 
        for (int atBit1 = 0; atBit1 < 8; atBit1++)
        {
            uint8_t mask1 = 1 << atBit1;
            bytes[atByte1] ^= mask1;  // Flip

            for (int atByte2 = 0; atByte2 < size; atByte2++)
            {
                for (int atBit2 = 0; atBit2 < 8; atBit2++)
                {
                    uint8_t mask2 = 1 << atBit2;
                    bytes[atByte2] ^= mask2;  // Flip
                    if (CheckCrc())
                    {
                        VanBusRx.nRepaired++;
                        VanBusRx.nTwoSeparateBitErrors++;
                        return true;
                    } // if

                    bytes[atByte2] ^= mask2;  // Flip back
                } // for
            } // for

            bytes[atByte1] ^= mask1;  // Flip back
        } // for
    } // for
#endif

    return false;
} // TVanPacketRxDesc::CheckCrcAndRepair

// Dumps the raw packet bytes to a stream (e.g. 'Serial').
// Optionally specify the last character; default is "\n" (newline).
void TVanPacketRxDesc::DumpRaw(Stream& s, char last) const
{
    s.printf("Raw: #%04u (%*u/%u) %2d(%2d) ",
        seqNo % 10000,
        VanBusRx.size > 100 ? 3 : VanBusRx.size > 10 ? 2 : 1,
        slot + 1,
        VanBusRx.size,
        size - 5 < 0 ? 0 : size - 5,
        size);

    if (size >= 1) s.printf("%02X ", bytes[0]);  // SOF
    if (size >= 3) s.printf("%03X %s ", Iden(), CommandFlagsStr());

    for (int i = 3; i < size; i++) s.printf("%02X%c", bytes[i], i == size - 3 ? ':' : i < size - 1 ? '-' : ' ');

    s.print(AckStr());
    s.print(" ");
    s.print(ResultStr());
    s.printf(" %04X", Crc());
    s.printf(" %s", CheckCrc() ? "CRC_OK" : "CRC_ERROR");

    s.print(last);
} // TVanPacketRxDesc::DumpRaw

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

// Normal bit time (8 microseconds), expressed as number of CPU cycles
#define VAN_NORMAL_BIT_TIME_CPU_CYCLES (667 * CPU_F_FACTOR)

    // We hardly ever get here. And if we do, the "number of bits" is not so important.
    //unsigned int nBits = (nCycles + 300 * CPU_F_FACTOR) / (650 * CPU_F_FACTOR);
    unsigned int nBits = (nCycles + 200 * CPU_F_FACTOR) / VAN_NORMAL_BIT_TIME_CPU_CYCLES;

    return nBits;
} // nBitsFromCycles

void ICACHE_RAM_ATTR SetTxBitTimer()
{
#ifdef ARDUINO_ARCH_ESP32
    timerEnd(timer);
#else // ! ARDUINO_ARCH_ESP32
    timer1_disable(); 
#endif // ARDUINO_ARCH_ESP32

    if (VanBusRx.txTimerIsr)
    {
        // Turn on the Tx bit timer

    #ifdef ARDUINO_ARCH_ESP32

        timerAttachInterrupt(timer, VanBusRx.txTimerIsr, true);
        timerAlarmWrite(timer, VanBusRx.txTimerTicks, true);
        timerAlarmEnable(timer);

    #else // ! ARDUINO_ARCH_ESP32

        timer1_attachInterrupt(VanBusRx.txTimerIsr);

        // Clock to timer (prescaler) is always 80MHz, even F_CPU is 160 MHz
        timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);

        timer1_write(VanBusRx.txTimerTicks);

    #endif // ARDUINO_ARCH_ESP32

    } // if
} // SetTxBitTimer

// If the timeout expires, the packet is VAN_RX_DONE. 'ack' has already been initially set to VAN_NO_ACK,
// and then to VAN_ACK if a new bit was received within the time-out period.
void ICACHE_RAM_ATTR WaitAckIsr()
{
    SetTxBitTimer();

    VanBusRx._AdvanceHead();
} // WaitAckIsr

// TODO - remove
volatile uint16_t startOfFrameByte = 0x00;

#ifdef ARDUINO_ARCH_ESP32
hw_timer_t * timer = NULL;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#endif // ARDUINO_ARCH_ESP32

// Pin level change interrupt handler
void ICACHE_RAM_ATTR RxPinChangeIsr()
{
    // The logic is:
    // - if pinLevelChangedTo == VAN_LOGICAL_HIGH, we've just had a series of VAN_LOGICAL_LOW bits.
    // - if pinLevelChangedTo == VAN_LOGICAL_LOW, we've just had a series of VAN_LOGICAL_HIGH bits.

#ifdef ARDUINO_ARCH_ESP32
    int pinLevelChangedTo = digitalRead(VanBusRx.pin);
#else // ! ARDUINO_ARCH_ESP32
    int pinLevelChangedTo = GPIP(VanBusRx.pin);  // GPIP() is faster than digitalRead()?
#endif // ARDUINO_ARCH_ESP32

    static int prevPinLevelChangedTo = VAN_BIT_RECESSIVE;

    static uint32_t prev = 0;
    uint32_t curr = ESP.getCycleCount();  // Store CPU cycle counter value as soon as possible

    static uint32_t jitter = 0;

    // Return quickly when it is a spurious interrupt (shorter than a single bit time)
    uint32_t nCycles = curr - prev;  // Arithmetic has safe roll-over
    if (nCycles + jitter < 400 * CPU_F_FACTOR) return;

    // Return quickly when it is a spurious interrupt (pin level not changed).
    if (pinLevelChangedTo == prevPinLevelChangedTo) return;
    prevPinLevelChangedTo = pinLevelChangedTo;

#ifdef ARDUINO_ARCH_ESP32
    portENTER_CRITICAL_ISR(&mux);
#endif // ARDUINO_ARCH_ESP32

    // Media access detection for packet transmission
    if (pinLevelChangedTo == VAN_BIT_RECESSIVE)
    {
        // Pin level just changed to 'recessive', so that was the end of the media access ('dominant')
        VanBusRx.lastMediaAccessAt = curr;
    } // if

    prev = curr;

    TVanPacketRxDesc* rxDesc = VanBusRx._head;
    PacketReadState_t state = rxDesc->state;
    rxDesc->slot = rxDesc - VanBusRx.pool;

#ifdef VAN_RX_ISR_DEBUGGING
    // Record some data to be used for debugging outside this ISR

    TIsrDebugPacket* isrDebugPacket = &rxDesc->isrDebugPacket;
    isrDebugPacket->slot = rxDesc->slot;
    TIsrDebugData* debugIsr =
        isrDebugPacket->at < VAN_ISR_DEBUG_BUFFER_SIZE ?
            isrDebugPacket->samples + isrDebugPacket->at :
            NULL;

    // Only write into buffer if there is space
    if (debugIsr != NULL)
    {
        debugIsr->pinLevel = pinLevelChangedTo;
        debugIsr->nCycles = nCycles;
    } // if

    // Just before returning from this ISR, record some data for debugging
    #define return \
    { \
        if (debugIsr != NULL) \
        { \
            debugIsr->pinLevelAtReturnFromIsr = GPIP(VanBusRx.pin); \
            debugIsr->nCyclesProcessing = ESP.getCycleCount() - curr; \
            isrDebugPacket->at++; \
        } \
        return; \
    }
#endif // VAN_RX_ISR_DEBUGGING

    static unsigned int atBit = 0;
    static uint16_t readBits = 0;

    if (state == VAN_RX_VACANT)
    {
        // Wait until we've seen a series of VAN_LOGICAL_HIGH bits
        // TODO - wait until we've seen at least IFS bits?
        if (pinLevelChangedTo == VAN_LOGICAL_LOW)
        {
            rxDesc->state = VAN_RX_SEARCHING;
            rxDesc->ack = VAN_NO_ACK;
            atBit = 0;
            readBits = 0;
            rxDesc->size = 0;
            jitter = 0;

            //timer1_disable(); // TODO - necessary?
        } // if

    #ifdef ARDUINO_ARCH_ESP32
        portEXIT_CRITICAL_ISR(&mux);
    #endif // ARDUINO_ARCH_ESP32

        return;
    } // if

    if (state == VAN_RX_WAITING_ACK)
    {
        // If the "ACK" came too soon, it might have been a late manchester bit: continue
        if (nCycles < 500 * CPU_F_FACTOR)
        {
            atBit = 0;
            readBits = 0;
            rxDesc->state = VAN_RX_LOADING;

        #ifdef ARDUINO_ARCH_ESP32
            portEXIT_CRITICAL_ISR(&mux);
            timerEnd(timer);
        #else // ! ARDUINO_ARCH_ESP32
            timer1_disable();
        #endif // ARDUINO_ARCH_ESP32

            return;
        } // if

        rxDesc->ack = VAN_ACK;

        // The timer ISR 'WaitAckIsr' will call 'VanBusRx._AdvanceHead()'

    #ifdef ARDUINO_ARCH_ESP32
        portEXIT_CRITICAL_ISR(&mux);
    #endif // ARDUINO_ARCH_ESP32

        return;
    } // if

    // If the current head packet is already VAN_RX_DONE, the circular buffer is completely full
    if (state != VAN_RX_SEARCHING && state != VAN_RX_LOADING)
    {
        VanBusRx._overrun = true;
        //SetTxBitTimer();

    #ifdef ARDUINO_ARCH_ESP32
        portEXIT_CRITICAL_ISR(&mux);
    #endif // ARDUINO_ARCH_ESP32

        return;
    } // if

#ifdef VAN_RX_ISR_DEBUGGING
    if (debugIsr != NULL) debugIsr->jitter = jitter;
#endif // VAN_RX_ISR_DEBUGGING
    unsigned int nBits = nBitsFromCycles(nCycles, jitter);
#ifdef VAN_RX_ISR_DEBUGGING
    if (debugIsr != NULL) debugIsr->nBits = nBits;
#endif // VAN_RX_ISR_DEBUGGING

    // During packet reception, the "Enhanced Manchester" encoding guarantees at most 5 bits are the same,
    // except during EOD when it can be 6.
    // However, sometimes the Manchester bit is missed. Let's be tolerant with that, and just pretend it
    // was there, by accepting up to 10 equal bits.
    if (nBits > 10)
    {
        if (state == VAN_RX_SEARCHING)
        {
            atBit = 0;
            readBits = 0;
            rxDesc->size = 0;
            jitter = 0;

        #ifdef ARDUINO_ARCH_ESP32
            portEXIT_CRITICAL_ISR(&mux);
        #endif // ARDUINO_ARCH_ESP32

            return;
        } // if

        rxDesc->result = VAN_RX_ERROR_NBITS;
        VanBusRx._AdvanceHead();
        //WaitAckIsr();

    #ifdef ARDUINO_ARCH_ESP32
        portEXIT_CRITICAL_ISR(&mux);
    #endif // ARDUINO_ARCH_ESP32

        return;
    } // if

    // Wait at most one extra bit time for the Manchester bit (5 --> 4, 10 --> 9)
    // But... Manchester "low" bit error at bit 10 is needed to see EOD, so skip that.
    if (nBits > 1
        &&
        (
            atBit + nBits == 5
            ||
            (
                atBit + nBits == 10
                &&
                (
                    // pinLevelChangedTo == VAN_LOGICAL_LOW: just had a series of VAN_LOGICAL_HIGH bits
                    pinLevelChangedTo == VAN_LOGICAL_LOW
                    // ||
                    // // EOD is never in bytes 0...4
                    // rxDesc->size < 5
                )
            )
        )
       )
    {
        nBits--;
        jitter = 500 * CPU_F_FACTOR;
        if (nCycles > VAN_NORMAL_BIT_TIME_CPU_CYCLES * nBits ) jitter = nCycles - VAN_NORMAL_BIT_TIME_CPU_CYCLES * nBits;
    #ifdef VAN_RX_ISR_DEBUGGING
        if (debugIsr != NULL) debugIsr->nBits--;
    #endif // VAN_RX_ISR_DEBUGGING
/*
    }

    // Experiment: read the pin level again; if it changed during the processing of the current interrupt, then
    // apparently the interrupt service was called quite late, so the number of bits is in fact one less
    else if (pinLevelChangedTo != GPIP(VanBusRx.pin))
    {
        nBits--;
        jitter = 500 * CPU_F_FACTOR;
        if (nCycles > VAN_NORMAL_BIT_TIME_CPU_CYCLES * nBits ) jitter = nCycles - VAN_NORMAL_BIT_TIME_CPU_CYCLES * nBits;
    #ifdef VAN_RX_ISR_DEBUGGING
        if (debugIsr != NULL) debugIsr->nBits--;
    #endif // VAN_RX_ISR_DEBUGGING
*/
    } // if

    atBit += nBits;
    readBits <<= nBits;

    // Remember: if pinLevelChangedTo == VAN_LOGICAL_LOW, we've just had a series of VAN_LOGICAL_HIGH bits
    uint16_t pattern = 0;
    if (pinLevelChangedTo == VAN_LOGICAL_LOW) pattern = (1 << nBits) - 1;
    readBits |= pattern;

    if (atBit >= 10)
    {
        atBit -= 10;

        // uint16_t, not uint8_t: we are reading 10 bits per byte ("Enhanced Manchester" encoding)
        uint16_t currentByte = readBits >> atBit;

        if (state == VAN_RX_SEARCHING)
        {
            // First 10 bits must be 00 0011 1101 (0x03D) (SOF, Start Of Frame)
            //
            // Accept also:
            // - 00 0001 1101 (0x01D) : as with all other interrupts, even the first interrupt (0->1) might be
            //   slightly late.
            // - 01 0011 1101 (0x13D) : spurious bit in the '0' series
            //
            // TODO - test this. Maybe accept also other "slightly-off" patterns?
            //
            if (currentByte != 0x03D
                && currentByte != 0x01D && currentByte != 0x13D
                // && currentByte != 0x185 && currentByte != 0x042 && currentByte != 0x14D
                // && currentByte != 0x1C8 && currentByte != 0x126 && currentByte != 0x04A
                // && currentByte != 0x0CE && currentByte != 0x172 && currentByte != 0x146
                // && currentByte != 0x0AA
               )
            {
                rxDesc->state = VAN_RX_VACANT;

                // TODO - remove
                startOfFrameByte = currentByte;

                //SetTxBitTimer();

            #ifdef VAN_RX_ISR_DEBUGGING
                isrDebugPacket->Init();
            #endif // VAN_RX_ISR_DEBUGGING

            #ifdef ARDUINO_ARCH_ESP32
                portEXIT_CRITICAL_ISR(&mux);
            #endif // ARDUINO_ARCH_ESP32

                return;
            } // if

            currentByte = 0x03D;
            rxDesc->state = VAN_RX_LOADING;
        } // if

        // Get ready for next byte
        readBits &= (1 << atBit) - 1;

        // Remove the 2 Manchester bits 'm'; the relevant 8 bits are 'X':
        //   9 8 7 6 5 4 3 2 1 0
        //   X X X X m X X X X m
        uint8_t readByte = (currentByte >> 2 & 0xF0) | (currentByte >> 1 & 0x0F);

        rxDesc->bytes[rxDesc->size++] = readByte;

        // EOD detected if last two bits are 0 followed by a 1, but never in bytes 0...4
        if ((currentByte & 0x003) == 0 && atBit == 0 && rxDesc->size >= 5)
        {
            rxDesc->state = VAN_RX_WAITING_ACK;

            // Set a timeout for the ACK bit

        #ifdef ARDUINO_ARCH_ESP32

            portEXIT_CRITICAL_ISR(&mux);

            timerEnd(timer);
            timerAttachInterrupt(timer, WaitAckIsr, true);
            //timer1_write(12 * 5); // 1.5 time slots = 1.5 * 8 us = 12 us
            timerAlarmWrite(timer, 16 * 5, false); // 2 time slots = 2 * 8 us = 16 us
            timerAlarmEnable(timer);

        #else // ! ARDUINO_ARCH_ESP32

            timer1_disable();
            timer1_attachInterrupt(WaitAckIsr);

            // Clock to timer (prescaler) is always 80MHz, even F_CPU is 160 MHz
            timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);

            //timer1_write(12 * 5); // 1.5 time slots = 1.5 * 8 us = 12 us
            timer1_write(16 * 5); // 2 time slots = 2 * 8 us = 16 us

        #endif // ARDUINO_ARCH_ESP32

            return;
        } // if

        if (rxDesc->size >= VAN_MAX_PACKET_SIZE)
        {
            rxDesc->result = VAN_RX_ERROR_MAX_PACKET;
            VanBusRx._AdvanceHead();
            //WaitAckIsr();

        #ifdef ARDUINO_ARCH_ESP32
            portEXIT_CRITICAL_ISR(&mux);
        #endif // ARDUINO_ARCH_ESP32

            return;
        } // if
    } // if

#ifdef ARDUINO_ARCH_ESP32
    portEXIT_CRITICAL_ISR(&mux);
#endif // ARDUINO_ARCH_ESP32

    return;

    #undef return

} // RxPinChangeIsr

// Initializes the VAN packet receiver
bool TVanPacketRxQueue::Setup(uint8_t rxPin, int queueSize)
{
    if (pin != VAN_NO_PIN_ASSIGNED) return false; // Already setup

    pin = rxPin;
    pinMode(rxPin, INPUT_PULLUP);

    size = queueSize;
    pool = new TVanPacketRxDesc[queueSize];
    _head = pool;
    tail = pool;
    end = pool + queueSize;

    attachInterrupt(digitalPinToInterrupt(rxPin), RxPinChangeIsr, CHANGE);

#ifdef ARDUINO_ARCH_ESP32
    // Clock to timer (prescaler) is always 80MHz, even F_CPU is 160 MHz. We want 0.2 microsecond resolution.
    timer = timerBegin(0, 80 / 5, true);
#else // ! ARDUINO_ARCH_ESP32
    timer1_isr_init();
    timer1_disable();
#endif // ARDUINO_ARCH_ESP32

    return true;
} // TVanPacketRxQueue::Setup

// Copy a VAN packet out of the receive queue, if available. Otherwise, returns false.
// If a valid pointer is passed to 'isQueueOverrun', will report then clear any queue overrun condition.
bool TVanPacketRxQueue::Receive(TVanPacketRxDesc& pkt, bool* isQueueOverrun)
{
    if (pin == VAN_NO_PIN_ASSIGNED) return false; // Call Setup first!

    if (! Available()) return false;

    // TODO - remove
    if (startOfFrameByte != 0x00)
    {
        //Serial.printf("=====> SOF: 0x%02X <=====\n", startOfFrameByte);
        ISR_SAFE_SET(startOfFrameByte, 0x00);
    } // if

    // Copy the whole packet descriptor out (including the debug info)
    // Note:
    // Instead of copying out, we could also just pass the pointer to the descriptor. However, then we would have to
    // wait with freeing the descriptor, thus keeping one precious queue slot allocated. It is better to copy the
    // packet into the (usually stack-allocated) memory of 'pkt' and free the queue slot as soon as possible. The
    // caller can now keep the packet as long as needed.
    pkt = *tail;

    if (isQueueOverrun)
    {
        *isQueueOverrun = IsQueueOverrun();
        ClearQueueOverrun();
    } // if

    // Indicate packet buffer is available for next packet
    tail->Init();

    AdvanceTail();

    return true;
} // TVanPacketRxQueue::Receive

// Disable VAN packet receiver
void TVanPacketRxQueue::Disable()
{
    if (pin == VAN_NO_PIN_ASSIGNED) return; // Call Setup first!
    detachInterrupt(digitalPinToInterrupt(VanBusRx.pin));
} // TVanPacketRxQueue::Disable

// Enable VAN packet receiver
void TVanPacketRxQueue::Enable()
{
    if (pin == VAN_NO_PIN_ASSIGNED) return; // Call Setup first!
    attachInterrupt(digitalPinToInterrupt(VanBusRx.pin), RxPinChangeIsr, CHANGE);
} // TVanPacketRxQueue::Enable

// Simple function to generate a string representation of a float value.
// Note: passed buffer size must be (at least) MAX_FLOAT_SIZE bytes, e.g. declare like this:
//   char buffer[MAX_FLOAT_SIZE];
char* FloatToStr(char* buffer, float f, int prec)
{
    dtostrf(f, MAX_FLOAT_SIZE - 1, prec, buffer);

    // Strip leading spaces
    char* strippedStr = buffer;
    while (isspace(*strippedStr)) strippedStr++;

    return strippedStr;
} // FloatToStr

// Dumps packet statistics
void TVanPacketRxQueue::DumpStats(Stream& s, bool longForm) const
{
    uint32_t pktCount = GetCount();
    static const char PROGMEM formatter[] = "received pkts: %lu, corrupt: %lu (%s%%)";
    char floatBuf[MAX_FLOAT_SIZE];

    uint32_t overallCorrupt = nCorrupt - nRepaired;

    if (longForm)
    {
        // Long output format

        // Using shared buffer floatBuf, so only one invocation per printf
        s.printf_P(
            formatter,
            pktCount,
            nCorrupt,
            pktCount == 0
                ? "-.---"
                : FloatToStr(floatBuf, 100.0 * nCorrupt / pktCount, 3));

        s.printf_P(
            PSTR(", repaired: %lu (%s%%)"),
            nRepaired,
            nCorrupt == 0
                ? "---" 
                : FloatToStr(floatBuf, 100.0 * nRepaired / nCorrupt, 0));

        s.printf_P(PSTR(" [SB_err: %lu, DCB_err: %lu"), nOneBitError, nTwoConsecutiveBitErrors);
        if (nTwoSeparateBitErrors > 0) s.printf_P(PSTR(", DSB_err: %lu"), nTwoSeparateBitErrors);
        s.print(F("]"));

        s.printf_P(
            PSTR(", overall: %lu (%s%%)\n"),
            overallCorrupt,
            pktCount == 0
                ? "-.---" 
                : FloatToStr(floatBuf, 100.0 * overallCorrupt / pktCount, 3));
    }
    else
    {
        // Short output format

        s.printf_P(
            formatter,
            pktCount,
            overallCorrupt,
            pktCount == 0
                ? "-.---"
                : FloatToStr(floatBuf, 100.0 * overallCorrupt / pktCount, 3));
        s.print(F("\n"));
    } // if
} // TVanPacketRxQueue::DumpStats

#ifdef VAN_RX_ISR_DEBUGGING

void TIsrDebugPacket::Dump(Stream& s) const
{
    // Parse packet outside ISR

    unsigned int atBit = 0;
    unsigned int readBits = 0;
    boolean eodSeen = false;
    int size = 0;
    int i = 0;

    #define reset() \
    { \
        atBit = 0; \
        readBits = 0; \
        eodSeen = false; \
        size = 0; \
    }

    while (at > 2 && i < at)
    {
        const TIsrDebugData* isrData = samples + i;
        if (i == 0)
        {
            s.printf_P(PSTR("%sSlot # CPU nCycles jitt -> nBits pinLVLs data\n"), slot + 1 >= 10 ? " " : "");
        } // if

        if (i <= 1) reset();

        s.printf("#%d", slot + 1);

        s.printf("%4u", i);

        uint32_t nCyclesProcessing = isrData->nCyclesProcessing;
        if (nCyclesProcessing > 999) s.printf(">999 "); else s.printf("%4lu ", nCyclesProcessing);

        uint32_t nCycles = isrData->nCycles;
        if (nCycles > 999999) s.printf(">999999"); else s.printf("%7lu", nCycles);

        s.printf("%5d", isrData->jitter);

        s.print(" -> ");

        unsigned int nBits = isrData->nBits;
        if (nBits > 9999) s.printf(">9999"); else s.printf("%5u", nBits);

        unsigned char pinLevelChangedTo = isrData->pinLevel;
        unsigned char pinLevelAtReturnFromIsr = isrData->pinLevelAtReturnFromIsr;
        s.printf(" \"%u\",\"%u\" ", pinLevelChangedTo, pinLevelAtReturnFromIsr);

        if (nBits > 10)
        {
            // Show we just had a long series of 1's (shown as '1.....') or 0's (shown as '-.....')
            s.print(pinLevelChangedTo == VAN_LOGICAL_LOW ? "1....." : "-.....");
            s.println();

            reset();
            i++;
            continue;
        } // if

        // Print the read bits one by one, in a column of 6
        if (nBits > 6)
        {
            s.print(pinLevelChangedTo == VAN_LOGICAL_LOW ? "1....1" : "-....-");
        }
        else
        {
            for (int i = 0; i < nBits; i++) s.print(pinLevelChangedTo == VAN_LOGICAL_LOW ? "1" : "-");
            for (int i = nBits; i < 6; i++) s.print(" ");
        } // if

        // Print current value
        s.printf(" %04X << %1u", readBits, nBits);

        atBit += nBits;
        readBits <<= nBits;

        // Print new value
        s.printf(" = %04X", readBits);

        uint8_t pattern = 0;
        if (pinLevelChangedTo == VAN_LOGICAL_LOW) pattern = (1 << nBits) - 1;
        readBits |= pattern;

        s.printf(" | %2X = %04X", pattern, readBits);

        if (eodSeen)
        {
            if (pinLevelChangedTo == VAN_LOGICAL_LOW && nBits == 1)
            {
                s.print(" ACK");
                reset();
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

            // EOD detected if last two bits are 0 followed by a 1, but never in bytes 0...4
            if ((currentByte & 0x003) == 0 && atBit == 0 && size >= 5)
            {
                eodSeen = true;
                s.print(" EOD");
            } // if
        } // if

        s.println();

        i++;
    } // while

    #undef reset()

} // TIsrDebugPacket::Dump

#endif // VAN_RX_ISR_DEBUGGING

TVanPacketRxQueue VanBusRx;
