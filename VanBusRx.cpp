/*
 * VanBus packet receiver for ESP8266
 *
 * Written by Erik Tromp
 *
 * Version 0.3.0 - June, 2022
 *
 * MIT license, all text above must be included in any redistribution.
 */

#include <limits.h>
#include "VanBusRx.h"

#ifdef ARDUINO_ARCH_ESP32
  #include <esp_task_wdt.h>
  #define wdt_reset() esp_task_wdt_reset()
#else
  #include <Esp.h>  // wdt_reset
#endif

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
// Optional parameter 'wantToCount' is a pointer-to-method of class TVanPacketRxDesc, returning a boolean.
// It can be used to limit the repair statistics to take only specific types of packets into account.
//
// Example of invocation:
//
//   if (! pkt.CheckCrcAndRepair(&TVanPacketRxDesc::IsSatnavPacket)) return -1; // Unrecoverable CRC error
//
// Note: let's keep the counters sane by calling this only once.
bool TVanPacketRxDesc::CheckCrcAndRepair(bool (TVanPacketRxDesc::*wantToCount)() const)
{
    // TODO - if this fixes the packet, VanBusRx.nCorrupt and VanBusRx.nRepaired are not increased
    bytes[size - 1] &= 0xFE;  // Last bit of last byte (LSB of CRC) is always 0

    if (CheckCrc()) return true;

    // One cycle without the uncertain bit flipped, plus (optionally) one cycle with the uncertain bit flipped
    for (int i = 0; i < (uncertainBit1 == NO_UNCERTAIN_BIT ? 1 : 2); i++)
    {
        int uncertainAtByte;
        uint8_t uncertainMask;

        // Second cycle?
        if (i == 1)
        {
            // Flip the bit which is at the position that is marked as "uncertain"

            uncertainAtByte = (uncertainBit1 - 1) >> 3;

            int uncertainAtBit = (uncertainBit1 - 1) & 0x07;  // 0 = MSB, 7 = LSB
            uncertainAtBit = 7 - uncertainAtBit;  // 0 = LSB, 7 = MSB

            uncertainMask = 1 << uncertainAtBit;
            bytes[uncertainAtByte] ^= uncertainMask;  // Flip
        } // if

        // Byte 0 can be skipped; it does not count for CRC
        for (int atByte = 1; atByte < size; atByte++)
        {
            for (int atBit = 0; atBit < 8; atBit++)
            {
                uint8_t mask = 1 << atBit;
                bytes[atByte] ^= mask;  // Flip

                // Is there a way to quickly re-calculate the CRC value when bit is flipped?
                if (CheckCrc())
                {
                    if (wantToCount == 0 || (this->*wantToCount)())
                    {
                        VanBusRx.nRepaired++;
                        VanBusRx.nOneBitErrors++;
                        if (i == 1) VanBusRx.nUncertainBitErrors++;
                        VanBusRx.nCorrupt++;
                    } // if
                    return true;
                } // if

                // Try also to flip the preceding bit
                if (atBit != 7)
                {
                    uint8_t mask2 = 1 << (atBit + 1);
                    bytes[atByte] ^= mask2;  // Flip
                    if (CheckCrc())
                    {
                        if (wantToCount == 0 || (this->*wantToCount)())
                        {
                            VanBusRx.nRepaired++;
                            VanBusRx.nTwoConsecutiveBitErrors++;
                            if (i == 1) VanBusRx.nUncertainBitErrors++;
                            VanBusRx.nCorrupt++;
                        } // if
                        return true;
                    } // if

                    bytes[atByte] ^= mask2;  // Flip back
                }
                else // atBit == 7
                {
                    // atByte > 0, so atByte - 1 is safe
                    bytes[atByte - 1] ^= 1 << 0;  // Flip
                    if (CheckCrc())
                    {
                        if (wantToCount == 0 || (this->*wantToCount)())
                        {
                            VanBusRx.nRepaired++;
                            VanBusRx.nTwoConsecutiveBitErrors++;
                            if (i == 1) VanBusRx.nUncertainBitErrors++;
                            VanBusRx.nCorrupt++;
                        } // if
                        return true;
                    } // if

                    bytes[atByte - 1] ^= 1 << 0;  // Flip back
                } // if

                bytes[atByte] ^= mask;  // Flip back
            } // for
        } // for

        if (i == 1) bytes[uncertainAtByte] ^= uncertainMask;  // Flip back (just to be tidy)
    } // for

    // Flip two bits. Getting to this point happens very rarely, luckily...
    for (int atByte1 = 0; atByte1 < size; atByte1++)
    {
        // This may take really long...
        wdt_reset();

        bool prevBit1 = false;

        for (int atBit1 = 0; atBit1 < 8; atBit1++)
        {
            // Only flip the last bit in a sequence of equal bits; take into account the Manchester bits

            uint8_t currMask1 = 1 << atBit1;
            bool currBit1 = (bytes[atByte1] & currMask1) != 0;
            if (prevBit1 != currBit1) continue;

            // After bit 3 or bit 7, there was the Manchester bit
            if (atBit1 == 3 || atBit1 == 7)
            {
                prevBit1 = ! currBit1;
            }
            else
            {
                prevBit1 = currBit1;
                uint8_t nextMask1 = 1 << (atBit1 + 1);
                bool nextBit1 = (bytes[atByte1] & nextMask1) != 0;
                if (currBit1 == nextBit1) continue;
            } // if

            bytes[atByte1] ^= currMask1;  // Flip

            // Flip second bit
            for (int atByte2 = atByte1; atByte2 < size; atByte2++)
            {
                bool prevBit2 = false;

                for (int atBit2 = 0; atBit2 < 8; atBit2++)
                {
                    // Only flip the last bit in a sequence of equal bits; take into account the Manchester bits

                    uint8_t currMask2 = 1 << atBit2;
                    bool currBit2 = (bytes[atByte2] & currMask2) != 0;
                    if (prevBit2 != currBit2) continue;
                    prevBit2 = currBit2;

                    // After bit 3 or bit 7, there was the Manchester bit
                    if (atBit2 == 3 || atBit2 == 7)
                    {
                        prevBit2 = ! currBit2;
                    }
                    else
                    {
                        prevBit2 = currBit2;
                        uint8_t nextMask2 = 1 << (atBit2 + 1);
                        bool nextBit2 = (bytes[atByte2] & nextMask2) != 0;
                        if (currBit2 == nextBit2) continue;
                    } // if

                    bytes[atByte2] ^= currMask2;  // Flip
                    if (CheckCrc())
                    {
                        if (wantToCount == 0 || (this->*wantToCount)())
                        {
                            VanBusRx.nRepaired++;
                            VanBusRx.nTwoSeparateBitErrors++;
                            VanBusRx.nCorrupt++;
                        } // if
                        return true;
                    } // if

                    bytes[atByte2] ^= currMask2;  // Flip back
                } // for
            } // for

            bytes[atByte1] ^= currMask1;  // Flip back
        } // for
    } // for

    if (wantToCount == 0 || (this->*wantToCount)()) VanBusRx.nCorrupt++;

    return false;
} // TVanPacketRxDesc::CheckCrcAndRepair

// Dumps the raw packet bytes to a stream (e.g. 'Serial').
// Optionally specify the last character; default is "\n" (newline).
// If the last character is "\n", will also print the ASCII representation of each byte (if possible).
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
    if (size >= 3) s.printf("%03X %1X (%s) ", Iden(), CommandFlags(), CommandFlagsStr());

    for (int i = 3; i < size; i++) s.printf("%02X%c", bytes[i], i == size - 3 ? ':' : i < size - 1 ? '-' : ' ');

    s.print(AckStr());
    s.print(" ");
    s.print(ResultStr());
    s.printf(" %04X", Crc());
    s.printf(" %s", CheckCrc() ? "CRC_OK" : "CRC_ERROR");

    if (uncertainBit1 != NO_UNCERTAIN_BIT) s.printf(" uBit=%u", uncertainBit1);

    if (last == '\n')
    {
        // Print also ASCII character representation of each byte, if possible, otherwise a small center-dot

        s.printf("\n%*s", VanBusRx.size > 100 ? 43 : VanBusRx.size > 10 ? 41 : 39, " ");
        for (int i = 3; i < size - 2; i++)
        {
            if (bytes[i] >= 0x20 && bytes[i] <= 0x7E) s.printf("%2c ", bytes[i]); else s.print(" \u00b7 ");
        } // for
    } // if

    s.print(last);
} // TVanPacketRxDesc::DumpRaw

// Normal bit time (8 microseconds), expressed as number of CPU cycles
#define VAN_NORMAL_BIT_TIME_CPU_CYCLES (CPU_CYCLES(667))

inline __attribute__((always_inline)) unsigned int nBits(uint32_t nCycles)
{
    return (nCycles + CPU_CYCLES(200)) / VAN_NORMAL_BIT_TIME_CPU_CYCLES;
} // nBits

// Calculate number of bits from a number of elapsed CPU cycles
inline __attribute__((always_inline)) unsigned int nBitsTakingIntoAccountJitter(uint32_t nCycles, uint32_t& jitter)
{
    // Here is the heart of the machine; lots of voodoo magic here...

    // Theory:
    // - VAN bus rate = 125 kbit/sec = 125 000 bits/sec
    //   1 bit = 1/125000 = 0.000008 sec = 8.0 usec
    // - CPU rate is 80 MHz
    //   1 cycle @ 80 MHz = 0.0000000125 sec = 0.0125 usec
    // --> So, 1 VAN-bus bit is 8.0 / 0.0125 = 640 cycles

    // Sometimes, samples are stretched, because the ISR is called too late: ESP8266 interrupt service latency can
    // vary. If that happens, we must compress the "sample time" for the next bit.

    // All timing values were found by trial and error
    jitter = 0;
    if (nCycles < CPU_CYCLES(482))
    {
        if (nCycles > CPU_CYCLES(106)) jitter = nCycles - CPU_CYCLES(106);
        return 0;
    }
    if (nCycles < CPU_CYCLES(1293))
    {
        if (nCycles > CPU_CYCLES(718)) jitter = nCycles - CPU_CYCLES(718);  // 718 --> 1293 = 575
        return 1;
    } // if
    if (nCycles < CPU_CYCLES(1893))
    {
        if (nCycles > CPU_CYCLES(1354)) jitter = nCycles - CPU_CYCLES(1354);  // 1354 --> 1893 = 539
        return 2;
    } // if
    if (nCycles < CPU_CYCLES(2470))
    {
        if (nCycles > CPU_CYCLES(2005)) jitter = nCycles - CPU_CYCLES(2005);  // 2005--> 2470 = 465
        return 3;
    } // if
    if (nCycles < CPU_CYCLES(3164))
    {
        if (nCycles > CPU_CYCLES(2639)) jitter = nCycles - CPU_CYCLES(2639);  // 2639 --> 3164 = 525
        return 4;
    } // if
    if (nCycles < CPU_CYCLES(3795))
    {
        if (nCycles > CPU_CYCLES(3272)) jitter = nCycles - CPU_CYCLES(3272);  // 3272 --> 3795 = 523
        return 5;
    } // if

    // We hardly ever get here
    const unsigned int _nBits = nBits(nCycles);
    if (nCycles > _nBits * VAN_NORMAL_BIT_TIME_CPU_CYCLES) jitter = nCycles - _nBits * VAN_NORMAL_BIT_TIME_CPU_CYCLES;

    return _nBits;
} // nBitsTakingIntoAccountJitter

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

    NO_INTERRUPTS;
    if (VanBusRx._head->state == VAN_RX_WAITING_ACK) VanBusRx._AdvanceHead();
    INTERRUPTS;
} // WaitAckIsr

#ifdef ARDUINO_ARCH_ESP32
hw_timer_t * timer = NULL;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#define EXIT_CRITICAL_ISR portEXIT_CRITICAL_ISR(&mux)
#else // ! ARDUINO_ARCH_ESP32
#define EXIT_CRITICAL_ISR
#endif // ARDUINO_ARCH_ESP32

// Pin level change interrupt handler
void ICACHE_RAM_ATTR RxPinChangeIsr()
{
    // Pin levels

    // The logic is:
    // - if pinLevel == VAN_LOGICAL_HIGH, we've just had a series of VAN_LOGICAL_LOW bits.
    // - if pinLevel == VAN_LOGICAL_LOW, we've just had a series of VAN_LOGICAL_HIGH bits.

  #ifdef ARDUINO_ARCH_ESP32
    #define GPIP(X_) digitalRead(X_)
  #endif // ARDUINO_ARCH_ESP32

    const int pinLevel = GPIP(VanBusRx.pin);
    static int prevPinLevel = VAN_BIT_RECESSIVE;
    static bool pinLevelChangedDuringInterruptHandling = false;

    // Number of elapsed CPU cycles
    static uint32_t prev = 0;
    const uint32_t curr = ESP.getCycleCount();  // Store CPU cycle counter value as soon as possible
    const uint32_t nCyclesMeasured = curr - prev;  // Arithmetic has safe roll-over
    prev = curr;

    // Retrieve context
    TVanPacketRxDesc* rxDesc = VanBusRx._head;
    const PacketReadState_t state = rxDesc->state;

    // Conversion from elapsed CPU cycles to number of bits, including built-up jitter
    static uint32_t jitter = 0;
    uint32_t nCycles = nCyclesMeasured + jitter;

    // During SOF, timing is slightly different. Timing values were found by trial and error.
    if (state == VAN_RX_SEARCHING)
    {
        if (nCycles > CPU_CYCLES(2240) && nCycles < CPU_CYCLES(2470)) nCycles += CPU_CYCLES(230);
        else if (nCycles > CPU_CYCLES(600) && nCycles < CPU_CYCLES(800)) nCycles -= CPU_CYCLES(30);
        else if (nCycles > CPU_CYCLES(1100) && nCycles < CPU_CYCLES(1290)) nCycles -= CPU_CYCLES(40);
    }
    else
    {
        if (nCyclesMeasured > CPU_CYCLES(1010) && nCyclesMeasured < CPU_CYCLES(1293) && jitter > 20) nCycles += CPU_CYCLES(60);
    } // if

  #ifdef VAN_RX_ISR_DEBUGGING
    const uint32_t prevJitter = jitter;
  #endif // VAN_RX_ISR_DEBUGGING

    unsigned int nBits = nBitsTakingIntoAccountJitter(nCycles, jitter);

    rxDesc->nIsrs++;

  #ifdef VAN_RX_ISR_DEBUGGING

    // Record some data to be used for debugging outside this ISR

    TIsrDebugPacket* isrDebugPacket = rxDesc->isrDebugPacket;

    isrDebugPacket->slot = rxDesc->slot;

    TIsrDebugData* debugIsr =
        isrDebugPacket->at < VAN_ISR_DEBUG_BUFFER_SIZE ?
            isrDebugPacket->samples + isrDebugPacket->at :
            NULL;

    // Only write into sample buffer if there is space
    if (debugIsr != NULL)
    {
        debugIsr->nIsrs = _min(rxDesc->nIsrs, UCHAR_MAX);
        debugIsr->nCycles = _min(nCyclesMeasured / CPU_F_FACTOR, USHRT_MAX);
        debugIsr->fromJitter = _min(prevJitter / CPU_F_FACTOR, (1 << 10) - 1);
        debugIsr->nBits = _min(nBits, UCHAR_MAX);
        debugIsr->prevPinLevel = prevPinLevel;
        debugIsr->pinLevel = pinLevel;
        debugIsr->fromState = state;
        debugIsr->readBits = 0;
    } // if

    // Macros useful for debugging

    // Just before returning from this ISR, record the pin level, plus some data for debugging
    #define RETURN \
    { \
        const int pinLevelAtReturnFromIsr = GPIP(VanBusRx.pin); \
        pinLevelChangedDuringInterruptHandling = jitter < CPU_CYCLES(100) && pinLevelAtReturnFromIsr != pinLevel; \
        \
        if (debugIsr != NULL) \
        { \
            debugIsr->toJitter = _min(jitter / CPU_F_FACTOR, (1 << 10) - 1); \
            debugIsr->flipBits = flipBits; \
            debugIsr->toState = rxDesc->state; \
            debugIsr->pinLevelAtReturnFromIsr = pinLevelAtReturnFromIsr; \
            debugIsr->atBit = atBit; \
            isrDebugPacket->at++; \
        } \
        EXIT_CRITICAL_ISR; \
        return; \
    }

    #define DEBUG_ISR(TO_, FROM_) if (debugIsr != NULL) debugIsr->TO_ = (FROM_);
    #define DEBUG_ISR_M(TO_, FROM_, MAX_) if (debugIsr != NULL) debugIsr->TO_ = _min((FROM_), (MAX_));

  #else

    // Just before returning from this ISR, record the pin level
    #define RETURN \
    { \
        const int pinLevelAtReturnFromIsr = GPIP(VanBusRx.pin); \
        pinLevelChangedDuringInterruptHandling = jitter < CPU_CYCLES(100) && pinLevelAtReturnFromIsr != pinLevel; \
        EXIT_CRITICAL_ISR; \
        return; \
    }

    #define DEBUG_ISR(TO_, FROM_)
    #define DEBUG_ISR_M(TO_, FROM_, MAX_)

  #endif // VAN_RX_ISR_DEBUGGING

    const bool samePinLevel = (pinLevel == prevPinLevel);
    prevPinLevel = pinLevel;

    uint16_t flipBits = 0;

  #ifdef ARDUINO_ARCH_ESP32
    portENTER_CRITICAL_ISR(&mux);
  #endif // ARDUINO_ARCH_ESP32

    // Media access detection for packet transmission
    if (pinLevel == VAN_BIT_RECESSIVE)
    {
        // Pin level just changed to 'recessive', so that was the end of the media access ('dominant')
        VanBusRx.lastMediaAccessAt = curr;
    } // if

    static unsigned int atBit = 0;
    static uint16_t readBits = 0;

  #ifdef VAN_RX_IFS_DEBUGGING

    TIfsDebugPacket* ifsDebugPacket = &rxDesc->ifsDebugPacket;

    TIfsDebugData* debugIfs =
        ifsDebugPacket->at < VAN_IFS_DEBUG_BUFFER_SIZE ?
            ifsDebugPacket->samples + ifsDebugPacket->at :
            NULL;

    if (state == VAN_RX_VACANT || state == VAN_RX_SEARCHING)
    {
        // Only write into sample buffer if there is space
        if (debugIfs != NULL)
        {
            debugIfs->nCycles = _min(nCyclesMeasured / CPU_F_FACTOR, USHRT_MAX);
            debugIfs->nBits = _min(nBits, UCHAR_MAX);
            debugIfs->pinLevel = pinLevel;
            debugIfs->fromState = state;
            debugIfs->toState = state;  // Can be overwritten later
            ifsDebugPacket->at++;
        } // if
    } // if

    // Macro useful for debugging
    #define DEBUG_IFS(TO_, FROM_) if (debugIfs != NULL) debugIfs->TO_ = (FROM_);

  #else

    #define DEBUG_IFS(TO, FROM)

  #endif // VAN_RX_IFS_DEBUGGING

    if (state == VAN_RX_WAITING_ACK)
    {
        // If the "ACK" came too soon or lasted more than 1 time slot, it is not an "ACK" but the first
        // "1" bit of the next byte
        if (pinLevelChangedDuringInterruptHandling
            || nCycles < CPU_CYCLES(650)
            || nCycles > CPU_CYCLES(1000))
        {
          #ifdef ARDUINO_ARCH_ESP32
            timerEnd(timer);
          #else // ! ARDUINO_ARCH_ESP32
            timer1_disable();
          #endif // ARDUINO_ARCH_ESP32

            rxDesc->state = VAN_RX_LOADING;
            DEBUG_IFS(toState, VAN_RX_LOADING);

            rxDesc->ack = VAN_NO_ACK;
        }
        else
        {
            // TODO - move (under condition) into timer ISR 'WaitAckIsr'?
            rxDesc->ack = VAN_ACK;

            // The timer ISR 'WaitAckIsr' will call 'VanBusRx._AdvanceHead()'
        } // if
    } // if

    if (state == VAN_RX_VACANT)
    {
        readBits = 0;

        if (pinLevel == VAN_LOGICAL_LOW)
        {
            // Normal detection: we've seen a series of VAN_LOGICAL_HIGH bits

            rxDesc->state = VAN_RX_SEARCHING;
            DEBUG_IFS(toState, VAN_RX_SEARCHING);

            if (nBits == 7 || nBits == 8) atBit = nBits; else atBit = 0;
            jitter = 0;

            pinLevelChangedDuringInterruptHandling = false;
        }
        else if (pinLevel == VAN_LOGICAL_HIGH)
        {
            if (nBits >= 2)
            {
                // Late detection

                rxDesc->state = VAN_RX_SEARCHING;
                DEBUG_IFS(toState, VAN_RX_SEARCHING);

                atBit = nBits;
                if (nBits > 5) jitter = 0;
            } // if
        } // if

        RETURN;
    } // if

    // If the current head packet is already VAN_RX_DONE, the circular buffer is completely full
    if (state == VAN_RX_DONE)
    {
        VanBusRx._overrun = true;

        RETURN;
    } // if

    // During packet reception, the "Enhanced Manchester" encoding guarantees at most 5 bits are the same,
    // except during EOD when it can be 6.
    // However, sometimes the Manchester bit is missed. Let's be tolerant with that, and just pretend it
    // was there, by accepting up to 10 equal bits.
    if (nBits > 10)
    {
        jitter = 0;

        if (state == VAN_RX_SEARCHING)
        {
            readBits = 0;
            atBit = 0;
            rxDesc->size = 0;

            RETURN;
        } // if

        rxDesc->result = VAN_RX_ERROR_NBITS;
        VanBusRx._AdvanceHead();

        RETURN;
    } // if

    // Experimental handling of special situations caused by a missed interrupt or a very late ISR invocation.
    // All cases were found by trial and error.
    if (nBits == 0)
    {
        if (state == VAN_RX_SEARCHING)
        {
            nBits = 1; // Seems to work best in vehicle
            DEBUG_ISR(nBits, 1);

            jitter = 0;
        }
        else
        {
            // Set or clear the last read bit
            readBits = pinLevel == VAN_LOGICAL_LOW ? readBits | 0x0001 : readBits & 0xFFFE;
        }
    }
    else if (samePinLevel)
    {
        if (nBits == 1)
        {
            flipBits = 0x0001;
        }
        else if (nBits == 2)
        {
            // Flip the last 'nBits' except the very last bit, e.g. flip the bits -- ---- --X-
            flipBits = 0x0002;
        }
        else if (nBits > 2)
        {
            // Flip the last 'nBits' except the very last bit, e.g. if nBits == 4 ==> flip the bits -- ---- XXX-
            flipBits = (1 << nBits) - 1 - 1;

            // If the interrupt was so late that the pin level has already changed again, then flip also the very
            // last bit
            if (jitter > CPU_CYCLES(318)) flipBits |= 0x0001;
        } // if

        if ((flipBits & 0x0001) == 0x0001) prevPinLevel = 2; // next ISR, samePinLevel must always be false.
    } // if

    readBits <<= nBits;
    atBit += nBits;

    // Calculate the position of the last received bit (in order of reception: MSB first)
    int bitPosition = rxDesc->size * 8 + atBit;

    // Count only the "real" bits, not the Manchester bits
    if (atBit > 4) bitPosition--;
    if (atBit > 9) bitPosition--;

    if (pinLevel == VAN_LOGICAL_LOW)
    {
        // Just had a series of VAN_LOGICAL_HIGH bits
        uint16_t pattern = (1 << nBits) - 1;
        readBits |= pattern;
    } // if

    if (flipBits == 0 && nBits == 3 && (atBit == 5 || atBit == 10) && rxDesc->uncertainBit1 == NO_UNCERTAIN_BIT)
    {
        // 4-th or 8-th bit same as Manchester bit? Then mark that bit position as candidate for
        // later repair by the CheckCrcAndRepair(...) method.
        rxDesc->uncertainBit1 = bitPosition;  // Position 1 = MSB, bit 8 = LSB
    } // if

    if (flipBits != 0)
    {
        readBits ^= flipBits;

        if (nBits > 1 && rxDesc->uncertainBit1 == NO_UNCERTAIN_BIT)
        {
            // The last bit is very uncertain: mark the bit position as candidate for later repair by the
            // CheckCrcAndRepair(...) method
            rxDesc->uncertainBit1 = bitPosition;  // Position 1 = MSB, bit 8 = LSB

            // Note: the one-but-last bit is also very uncertain, but for now we mark only the last bit.
            // In a later version, more than one "uncertain bit" marking may be implemented.
        } // if
    } // if

    if (state == VAN_RX_SEARCHING)
    {
        // The bit timing is slightly different during SOF: apply alternative jitter calculations
        if (nBits == 3)
        {
            // Decrease jitter value by 168, but don't go below 0
            if (jitter > CPU_CYCLES(168)) jitter = jitter - CPU_CYCLES(168); else jitter = 0;
        }
        else if (atBit == 4)
        {
            if (nBits == 4)
            {
                // Timing seems to be 2624 for the first 4-bit sequence during SOF (normally 2639)
                if (nCyclesMeasured > CPU_CYCLES(2624)) jitter = nCyclesMeasured - CPU_CYCLES(2624);
            }
        }
        else if (atBit == 7 || atBit == 8)
        {
            if (nBits == 1)
            {
                // Decrease jitter value by 130, but don't go below 0
                if (jitter > CPU_CYCLES(130)) jitter = jitter - CPU_CYCLES(130); else jitter = 0;
            }
            else if (nBits == 2)
            {
                // Decrease jitter value by 168, but don't go below 0
                if (jitter > CPU_CYCLES(168)) jitter = jitter - CPU_CYCLES(168); else jitter = 0;
            }
            else if (nBits == 4)
            {
                // Timing seems to be 2514 for the second 4-bit sequence during SOF (normally 2639)
                if (nCyclesMeasured > CPU_CYCLES(2514)) jitter = nCyclesMeasured - CPU_CYCLES(2514);
            } // if
        } // if

        // Be flexible in SOF detection. All cases were found by trial and error.
        if (atBit == 7 && readBits == 0x00D)  // e.g. --- 11-1
        {
            atBit = 10;
        }
        else if (atBit == 8 && (readBits & 0x00F) == 0x00D)  // e.g. ---1 11-1, --11 11-1, ---- 11-1
        {
            atBit = 10;
        }
        else if (atBit == 9 && (readBits & 0x00E) == 0x00A)  // e.g. - -111 1-11
        {
            atBit = 11;
        }
        else if (atBit == 9 && (readBits & 0x003) == 0x001)  // e.g. - --11 11-1, - ---- ---1, - ---- -1-1
        {
            atBit = 10;
        }
        else if (atBit == 10 && (readBits & 0x006) == 0x002) // e.g. -- -111 1-1-, -- -111 1-11, -- ---- 1-11
        {
            atBit = 11;
        } // if
        else if (atBit == 12 && (readBits & 0x018) == 0x008) // e.g. ---- -11- 1111
        {
            atBit = 13;
        } // if
        else if (atBit == 13 && readBits == 0x1FF) // e.g. - ---1 1111 1111
        {
            // This is not a SOF pattern
            readBits = 0x000; // Force to state VAN_RX_VACANT, below
        } // if
        else if (atBit == 14 && readBits == 0x3FF) // e.g. -- --11 1111 1111
        {
            // This is not a SOF pattern
            readBits = 0x000; // Force to state VAN_RX_VACANT, below
        } // if
    } // if

    DEBUG_ISR(readBits, readBits);

    if (atBit >= 10)
    {
        atBit -= 10;

        // uint16_t, not uint8_t: we are reading 10 bits per byte ("Enhanced Manchester" encoding)
        uint16_t currentByte = readBits >> atBit;

        // Get ready for next byte
        readBits &= (1 << atBit) - 1;

        if (state == VAN_RX_SEARCHING)
        {
            // Ideally, the first 10 bits are 00 0011 1101 (0x03D) (SOF, Start Of Frame)
            if (currentByte != 0x03D

                // Accept also (found through trial and error):
                && currentByte != 0x01D // 00 0001 1101
                && currentByte != 0x039 // 00 0011 1001
                && currentByte != 0x03B // 00 0011 1011
                && currentByte != 0x03C // 00 0011 1100
                && currentByte != 0x01E // 00 0001 1110
                && currentByte != 0x00D // 00 0000 1101
                && currentByte != 0x005 // 00 0000 0101
                && currentByte != 0x001 // 00 0000 0001
                && currentByte != 0x03F // 00 0011 1111
                && currentByte != 0x3FD // 11 1111 1101
                && currentByte != 0x07D // 00 0111 1101
               )
            {
                rxDesc->state = VAN_RX_VACANT;
                DEBUG_IFS(toState, VAN_RX_VACANT);

                jitter = 0;

                RETURN;
            } // if

            currentByte = 0x03D;

            rxDesc->state = VAN_RX_LOADING;
            DEBUG_IFS(toState, VAN_RX_LOADING);
        } // if

        // Remove the 2 Manchester bits 'm'; the relevant 8 bits are 'X':
        //   9 8 7 6 5 4 3 2 1 0
        //   X X X X m X X X X m
        uint8_t readByte = (currentByte >> 2 & 0xF0) | (currentByte >> 1 & 0x0F);

        rxDesc->bytes[rxDesc->size++] = readByte;

        // EOD detected if last two bits are 0 followed by a 1, but never in bytes 0...4
        if ((currentByte & 0x003) == 0 && atBit == 0 && rxDesc->size >= 5

            // Experiment for 3 last "0"-bits: too short means it is not EOD
            && (nBits != 3 || nCycles > CPU_CYCLES(1963)))
        {
            rxDesc->state = VAN_RX_WAITING_ACK;
            DEBUG_IFS(toState, VAN_RX_WAITING_ACK);

            // Set a timeout for the ACK bit

          #ifdef ARDUINO_ARCH_ESP32

            timerEnd(timer);
            timerAttachInterrupt(timer, WaitAckIsr, true);
            timerAlarmWrite(timer, 24 * 5, false); // 3 time slots = 3 * 8 us = 24 us
            timerAlarmEnable(timer);

          #else // ! ARDUINO_ARCH_ESP32

            timer1_disable();
            timer1_attachInterrupt(WaitAckIsr);

            // Clock to timer (prescaler) is always 80MHz, even F_CPU is 160 MHz
            timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
            timer1_write(40 * 5); // 5 time slots = 5 * 8 us = 40 us

          #endif // ARDUINO_ARCH_ESP32

        }
        else if (rxDesc->size >= VAN_MAX_PACKET_SIZE)
        {
            rxDesc->result = VAN_RX_ERROR_MAX_PACKET;
            VanBusRx._AdvanceHead();

            jitter = 0;
        } // if
    } // if

    RETURN;
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

  #ifdef VAN_RX_ISR_DEBUGGING
    _head->isrDebugPacket = isrDebugPacket;
  #endif // VAN_RX_ISR_DEBUGGING

    for (TVanPacketRxDesc* rxDesc = pool; rxDesc < end; rxDesc++) rxDesc->slot = rxDesc - pool;

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

    // Copy the whole packet descriptor out (including the debug info)
    // Note:
    // Instead of copying out, we could also just pass the pointer to the descriptor. However, then we would have to
    // wait with freeing the descriptor, thus keeping one precious queue slot allocated. It is better to copy the
    // packet into the (usually stack-allocated) memory of 'pkt' and free the queue slot as soon as possible. The
    // caller can now keep the packet as long as needed.
    pkt = *tail;

    if (isQueueOverrun) *isQueueOverrun = IsQueueOverrun();

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

        s.printf_P(PSTR(" [SB: %lu, DCB: %lu, DSB: %lu], UCB: %lu"),
            nOneBitErrors,
            nTwoConsecutiveBitErrors,
            nTwoSeparateBitErrors,
            nUncertainBitErrors);

        s.printf_P(
            PSTR(", overall: %lu (%s%%)"),
            overallCorrupt,
            pktCount == 0
                ? "-.---" 
                : FloatToStr(floatBuf, 100.0 * overallCorrupt / pktCount, 3));

        s.printf_P(PSTR(", maxQueued: %u/%u\n"), GetMaxQueued(), QueueSize());
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

#ifdef VAN_RX_IFS_DEBUGGING

bool TIfsDebugPacket::IsAbnormal() const
{
    // Normally, a packet is recognized after 5 interrupts (pin level changes)
    // The 'pkt.getIfsDebugPacket().Dump()' output looks e.g. like this:
    //
    //   # nCycles -> nBits pinLVL       state
    //   0  >65535 ->  >255    "0"      VACANT
    //   1    2607 ->     4    "1"   SEARCHING
    //   2    2512 ->     4    "0"   SEARCHING
    //   3     729 ->     1    "1"   SEARCHING
    //   4     711 ->     1    "0"   SEARCHING
    //
    // Alternatively, this can also happen:
    //
    //   # nCycles -> nBits pinLVL       state
    //   0    1151 ->     2    "1"      VACANT
    //   1    7913 ->    12    "0"      VACANT
    //   2    2594 ->     4    "1"   SEARCHING
    //   3    2526 ->     4    "0"   SEARCHING
    //   4     714 ->     1    "1"   SEARCHING
    //   5     703 ->     1    "0"   SEARCHING

    bool normal = at <= 5 || (at == 6 && samples[0].pinLevel == 1);
    return ! normal;
} // TIfsDebugPacket::IsAbnormal

// Dump data found in the inter-frame space
void TIfsDebugPacket::Dump(Stream& s) const
{
    int i = 0;

    while (i < at)
    {
        const TIfsDebugData* ifsData = samples + i;
        if (i == 0) s.printf_P(PSTR("  # nCycles -> nBits pinLVL   fromState     toState\n"));

        s.printf("%3u", i);

        const uint32_t nCycles = ifsData->nCycles;
        if (nCycles >= USHRT_MAX) s.printf("  >%5lu", USHRT_MAX); else s.printf(" %7lu", nCycles);

        s.print(" -> ");

        const uint16_t nBits = ifsData->nBits;
        if (nBits >= UCHAR_MAX) s.printf(" >%3u", UCHAR_MAX); else s.printf("%5u", nBits);

        const uint16_t pinLevel = ifsData->pinLevel;
        s.printf("    \"%u\"", pinLevel);

        s.printf(" %11.11s", TVanPacketRxDesc::StateStr(ifsData->fromState));
        s.printf(" %11.11s", TVanPacketRxDesc::StateStr(ifsData->toState));

        s.print(" ");

        if (nBits > 6)
        {
            s.print(pinLevel == VAN_LOGICAL_LOW ? F("1.....1") : F("-.....-"));
        }
        else
        {
            for (int i = 0; i < nBits; i++) s.print(pinLevel == VAN_LOGICAL_LOW ? "1" : "-");
            for (int i = nBits; i < 6; i++) s.print(" ");
        } // if

        s.println();

        i++;
    } // while
} // TIfsDebugPacket::Dump

#endif // VAN_RX_IFS_DEBUGGING

#ifdef VAN_RX_ISR_DEBUGGING

// Print ISR debug data
void TIsrDebugPacket::Dump(Stream& s) const
{
    NO_INTERRUPTS;
    if (wLock)
    {
        // Packet has not (yet) been written to, or is currently being written into
        INTERRUPTS;
        return;
    } // if

    rLock = true;
    INTERRUPTS;

    uint16_t prevAtBit = 0;
    boolean eodSeen = false;
    int size = 0;
    int i = 0;

    #define reset() \
    { \
        eodSeen = false; \
        size = 0; \
    }

    while (at > 2 && i < at)
    {
        // Printing all this can take really long...
        if (i % 100 == 0) wdt_reset();

        const TIsrDebugData* isrData = samples + i;
        if (i == 0)
        {
            // Print headings
            s.print(F("  # ISR nCycles+jitt = nTotal -> nBits atBit (nLate) pinLVLs        fromState     toState data  flip byte\n"));
        } // if

        if (i <= 1) reset();

        s.printf("%3u%4u", i, isrData->nIsrs);

        const uint32_t nCycles = isrData->nCycles;
        if (nCycles >= USHRT_MAX) s.printf("  >%5lu", USHRT_MAX); else s.printf(" %7lu", nCycles);

        const uint32_t jitter = isrData->fromJitter;
        if (jitter != 0)
        {
            s.printf("%+5d", jitter);
            s.printf(" =%7lu", nCycles + jitter);
        }
        else
        {
            s.print(F("              "));
        } // if

        s.print(" -> ");

        const uint16_t nBits = isrData->nBits;
        if (nBits >= UCHAR_MAX) s.printf(" >%3u", UCHAR_MAX); else s.printf("%5u", nBits);

        s.printf(" %5u", isrData->atBit);

        const uint32_t addedCycles = isrData->toJitter;
        if (addedCycles != 0)
        {
            #define MAX_UINT32_STR_SIZE 11
            static char buffer[MAX_UINT32_STR_SIZE];
            sprintf(buffer, "(%d)", addedCycles);
            s.printf("%8s", buffer);
        }
        else
        {
            s.print(F("        "));
        } // if

        const uint16_t pinLevel = isrData->pinLevel;
        s.printf(" \"%u\"->\"%u\",\"%u\"", isrData->prevPinLevel, pinLevel, isrData->pinLevelAtReturnFromIsr);

        s.printf(" %11.11s", TVanPacketRxDesc::StateStr(isrData->fromState));
        s.printf(" %11.11s ", TVanPacketRxDesc::StateStr(isrData->toState));

        if (nBits > 10)
        {
            // Show we just had a long series of 1's (shown as '1......') or 0's (shown as '-......')
            s.print(pinLevel == VAN_LOGICAL_LOW ? F("1......") : F("-......"));
            s.println();

            reset();
            i++;
            continue;
        } // if

        // Print the read bits one by one, in a column of 6
        if (nBits > 6)
        {
            s.print(pinLevel == VAN_LOGICAL_LOW ? F("1.....1") : F("-.....-"));
        }
        else
        {
            for (int i = 0; i < nBits; i++)
            {
                s.print(pinLevel == VAN_LOGICAL_LOW ? "1" : "-");
                if (prevAtBit + i == 9) s.print("|");  // End of byte marker
            } // for
            for (int i = nBits; i < 6; i++) s.print(" ");
        } // if

        bool sofSeen = isrData->fromState == VAN_RX_SEARCHING && isrData->toState == VAN_RX_LOADING;
        if (sofSeen && prevAtBit + isrData->nBits < 10) s.print("|");  // End of SOF byte marker

        const uint16_t flipBits = isrData->flipBits;
        if (flipBits == 0) s.print("    "); else s.printf(" %02X ", flipBits);

        if (eodSeen)
        {
            if (pinLevel == VAN_LOGICAL_LOW && nBits == 1)
            {
                s.print(" ACK");
                reset();
            } // if
        }
        else if (sofSeen || prevAtBit + isrData->nBits >= 10)
        {
            int shift = prevAtBit + isrData->nBits;
            if (shift > 10) shift -= 10; else shift = 0;

            // uint16_t, not uint8_t: we are reading 10 bits per byte ("Enhanced Manchester" encoding)
            uint16_t currentByte = isrData->readBits >> shift;

            // Print each bit. Use small (superscript) characters for Manchester bits.
            for (int i = 9; i >= 6; i--) s.print(currentByte & 1 << i ? "1" : "-");
            s.print(currentByte & 1 << 5 ? "\u00b9" : "\u00b0");
            for (int i = 4; i >= 1; i--) s.print(currentByte & 1 << i ? "1" : "-");
            s.print(currentByte & 1 << 0 ? "\u00b9" : "\u00b0");

            // Remove the 2 Manchester bits 'm'; the relevant 8 bits are 'X':
            //   9 8 7 6 5 4 3 2 1 0
            //   X X X X m X X X X m
            const uint8_t readByte = (currentByte >> 2 & 0xF0) | (currentByte >> 1 & 0x0F);

            // Print the read byte and its position in the frame
            s.printf_P(PSTR(" --> 0x%02X '%c' (#%d)"),
                readByte,
                readByte >= 0x20 && readByte <= 0x7E ? readByte : '?',
                size + 1
            );
            size++;

            // EOD detected if last two bits are 0 followed by a 1, but never in bytes 0...4
            if ((currentByte & 0x003) == 0 && isrData->atBit == 0 && size >= 5)
            {
                eodSeen = true;
                s.print(" EOD");
            } // if
        } // if

        prevAtBit = isrData->atBit;

        s.println();

        i++;
    } // while

    #undef reset()

    rLock = false;  // Assumed to be atomic

} // TIsrDebugPacket::Dump

#endif // VAN_RX_ISR_DEBUGGING

TVanPacketRxQueue VanBusRx;
