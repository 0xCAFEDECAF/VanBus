/*
 * VanBus packet receiver for ESP8266
 *
 * Written by Erik Tromp
 *
 * Version 0.2.5 - January, 2022
 *
 * MIT license, all text above must be included in any redistribution.
 */

#include <limits.h>
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
    bytes[size - 1] &= 0xFE;  // Last bit of last byte (LSB of CRC) is always 0

    if (CheckCrc()) return true;

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
                    VanBusRx.nOneBitError++;
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
                        VanBusRx.nCorrupt++;
                    } // if
                    return true;
                } // if

                bytes[atByte] ^= mask2;  // Flip back
            }
            else // atByte > 0
            {
                // atBit == 7
                bytes[atByte - 1] ^= 1 << 0;  // Flip
                if (CheckCrc())
                {
                    if (wantToCount == 0 || (this->*wantToCount)())
                    {
                        VanBusRx.nRepaired++;
                        VanBusRx.nTwoConsecutiveBitErrors++;
                        VanBusRx.nCorrupt++;
                    } // if
                    return true;
                } // if

                bytes[atByte - 1] ^= 1 << 0;  // Flip back
            } // if

            bytes[atByte] ^= mask;  // Flip back
        } // for
    } // for

    if (wantToCount == 0 || (this->*wantToCount)()) VanBusRx.nCorrupt++;

    return false;
} // TVanPacketRxDesc::CheckCrcAndRepair

// Dumps the raw packet bytes to a stream (e.g. 'Serial').
// Optionally specify the last character; default is "\n" (newline).
// If the last character is "\n", will also print the ASCII representation of each byte, if possible.
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

    if (last == '\n')
    {
        // Print also ASCII character representation of each byte, if possible, otherwise a small center-dot
        s.print("\n                                         ");
        for (int i = 3; i < size - 2; i++)
        {
            if (bytes[i] >= 0x20 && bytes[i] <= 0x7E) s.printf("%2c ", bytes[i]); else s.print(" \u00b7 ");
        } // for
    } // if

    s.print(last);
} // TVanPacketRxDesc::DumpRaw

// Normal bit time (8 microseconds), expressed as number of CPU cycles
//#define VAN_NORMAL_BIT_TIME_CPU_CYCLES (650 * CPU_F_FACTOR)
#define VAN_NORMAL_BIT_TIME_CPU_CYCLES (667 * CPU_F_FACTOR)

inline __attribute__((always_inline)) unsigned int nBits(uint32_t nCycles)
{
    // return (nCycles + 300 * CPU_F_FACTOR) / VAN_NORMAL_BIT_TIME_CPU_CYCLES;
    return (nCycles + 200 * CPU_F_FACTOR) / VAN_NORMAL_BIT_TIME_CPU_CYCLES;
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
    if (nCycles < 432 * CPU_F_FACTOR)
    {
        return 0;
    }
    //if (nCycles < 1124 * CPU_F_FACTOR)
    if (nCycles < 1134 * CPU_F_FACTOR)
    {
        //if (nCycles > 800 * CPU_F_FACTOR) jitter = nCycles - 800 * CPU_F_FACTOR;  // 800 --> 1124 = 324
        if (nCycles > 786 * CPU_F_FACTOR) jitter = nCycles - 786 * CPU_F_FACTOR;  // 786 --> 1134 = 348
        return 1;
    } // if
    if (nCycles < 1744 * CPU_F_FACTOR)
    //if (nCycles < 1819 * CPU_F_FACTOR)
    {
        //if (nCycles > 1380 * CPU_F_FACTOR) jitter = nCycles - 1380 * CPU_F_FACTOR;  // 1380 --> 1744 = 364
        if (nCycles > 1377 * CPU_F_FACTOR) jitter = nCycles - 1377 * CPU_F_FACTOR;  // 1380 --> 1744 = 364
        return 2;
    } // if
    //if (nCycles < 2383 * CPU_F_FACTOR)
    if (nCycles < 2413 * CPU_F_FACTOR)
    {
        //if (nCycles > 2100 * CPU_F_FACTOR) jitter = nCycles - 2100 * CPU_F_FACTOR;  // 2100 --> 2383 = 283
        if (nCycles > 2072 * CPU_F_FACTOR) jitter = nCycles - 2072 * CPU_F_FACTOR;  // 2072--> 2413 = 341
        return 3;
    } // if
    if (nCycles < 3045 * CPU_F_FACTOR)
    //if (nCycles < 3080 * CPU_F_FACTOR)
    {
        //if (nCycles > 2655 * CPU_F_FACTOR) jitter = nCycles - 2655 * CPU_F_FACTOR;  // 2655 --> 3045 = 390
        if (nCycles > 2731 * CPU_F_FACTOR) jitter = nCycles - 2731 * CPU_F_FACTOR;  // 2731 --> 3045 = 314
        return 4;
    } // if
    //if (nCycles < 3665 * CPU_F_FACTOR)
    if (nCycles < 3680 * CPU_F_FACTOR)
    {
        if (nCycles > 3300 * CPU_F_FACTOR) jitter = nCycles - 3300 * CPU_F_FACTOR;  // 3300 --> 3680 = 380
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

    VanBusRx._AdvanceHead();
} // WaitAckIsr

#ifdef ARDUINO_ARCH_ESP32
hw_timer_t * timer = NULL;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#endif // ARDUINO_ARCH_ESP32

// Pin level change interrupt handler
void ICACHE_RAM_ATTR RxPinChangeIsr()
{
    // The logic is:
    // - if pinLevel == VAN_LOGICAL_HIGH, we've just had a series of VAN_LOGICAL_LOW bits.
    // - if pinLevel == VAN_LOGICAL_LOW, we've just had a series of VAN_LOGICAL_HIGH bits.

  #ifdef ARDUINO_ARCH_ESP32
    const int pinLevel = digitalRead(VanBusRx.pin);
  #else // ! ARDUINO_ARCH_ESP32
    const int pinLevel = GPIP(VanBusRx.pin);  // GPIP() is faster than digitalRead()?
  #endif // ARDUINO_ARCH_ESP32

    static int prevPinLevel = VAN_BIT_RECESSIVE;

    static uint32_t prev = 0;
    const uint32_t curr = ESP.getCycleCount();  // Store CPU cycle counter value as soon as possible

    static uint32_t jitter = 0;

    const uint32_t nCycles = curr - prev;  // Arithmetic has safe roll-over

    static bool pinLevelChangedDuringInterruptHandling = false;

    TVanPacketRxDesc* rxDesc = VanBusRx._head;

    uint16_t flipBits = 0;

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
        debugIsr->nCycles = _min(nCycles, USHRT_MAX * CPU_F_FACTOR) / CPU_F_FACTOR;
        debugIsr->fromJitter = 0;
        debugIsr->nBits = 0;
        debugIsr->prevPinLevel = prevPinLevel;
        debugIsr->pinLevel = pinLevel;
        debugIsr->fromState = rxDesc->state;
        debugIsr->atBit = 0;
    } // if

    // Macros useful for debugging

    // Just before returning from this ISR, record some data for debugging
    #define RETURN \
    { \
        if (debugIsr != NULL) \
        { \
            debugIsr->toJitter = _min(jitter, (1 << 10 - 1) * CPU_F_FACTOR) / CPU_F_FACTOR; \
            debugIsr->flipBits = flipBits; \
            debugIsr->toState = rxDesc->state; \
            debugIsr->pinLevelAtReturnFromIsr = GPIP(VanBusRx.pin); \
            isrDebugPacket->at++; \
        } \
        return; \
    }

    #define DEBUG_ISR(TO_, FROM_) if (debugIsr != NULL) debugIsr->TO_ = (FROM_);
    #define DEBUG_ISR_M(TO_, FROM_, MAX_) if (debugIsr != NULL) debugIsr->TO_ = _min((FROM_), (MAX_));

  #else

    #define RETURN return

    #define DEBUG_ISR(TO_, FROM_)
    #define DEBUG_ISR_M(TO_, FROM_, MAX_)

  #endif // VAN_RX_ISR_DEBUGGING

    const bool samePinLevel = (pinLevel == prevPinLevel);

    // Return quickly when it is a spurious interrupt (shorter than a single bit time)
    if (nCycles + jitter < 405 * CPU_F_FACTOR
        || (pinLevelChangedDuringInterruptHandling && nCycles + jitter < 485 * CPU_F_FACTOR)
        || (samePinLevel && nCycles + jitter < 457 * CPU_F_FACTOR))
    {
        RETURN;
    } // if

  #ifdef ARDUINO_ARCH_ESP32
    portENTER_CRITICAL_ISR(&mux);
  #endif // ARDUINO_ARCH_ESP32

    // Media access detection for packet transmission
    if (pinLevel == VAN_BIT_RECESSIVE)
    {
        // Pin level just changed to 'recessive', so that was the end of the media access ('dominant')
        VanBusRx.lastMediaAccessAt = curr;
    } // if

    prev = curr;


    prevPinLevel = pinLevel;

    static unsigned int atBit = 0;
    static uint16_t readBits = 0;

    PacketReadState_t state = rxDesc->state;

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
            debugIfs->nCycles = _min(nCycles, USHRT_MAX * CPU_F_FACTOR) / CPU_F_FACTOR;
            const unsigned int _nBits = nBits(nCycles);
            debugIfs->nBits = _min(_nBits, UCHAR_MAX);
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

    if (state == VAN_RX_VACANT)
    {
        if (pinLevel == VAN_LOGICAL_LOW)
        {
            // Normal detection: we've seen a series of VAN_LOGICAL_HIGH bits

            DEBUG_ISR(nBits, 0);

            rxDesc->state = VAN_RX_SEARCHING;

            DEBUG_IFS(toState, VAN_RX_SEARCHING);

            rxDesc->ack = VAN_NO_ACK;
            atBit = 0;

            DEBUG_ISR(atBit, 0);

            readBits = 0;
            rxDesc->size = 0;
            jitter = 0;

            pinLevelChangedDuringInterruptHandling = false;

            //timer1_disable(); // TODO - necessary?
        }
        else if (pinLevel == VAN_LOGICAL_HIGH)
        {
            const unsigned int _nBits = nBits(nCycles);

            DEBUG_ISR_M(nBits, _nBits, UCHAR_MAX);

            if (_nBits == 4 || _nBits == 3 || _nBits == 5)
            {
                // Late detection

                rxDesc->state = VAN_RX_SEARCHING;

                DEBUG_IFS(toState, VAN_RX_SEARCHING);

                rxDesc->ack = VAN_NO_ACK;

                atBit = 4;

                DEBUG_ISR(atBit, 4);

                readBits = 0;
                rxDesc->size = 0;
                jitter = 0;

                pinLevelChangedDuringInterruptHandling = false;
            } // if
        } // if

      #ifdef ARDUINO_ARCH_ESP32
        portEXIT_CRITICAL_ISR(&mux);
      #endif // ARDUINO_ARCH_ESP32

        RETURN;
    } // if

    if (state == VAN_RX_WAITING_ACK)
    {
      #ifdef VAN_RX_ISR_DEBUGGING
        const unsigned int _nBits = nBits(nCycles);
        DEBUG_ISR_M(nBits, _nBits, UCHAR_MAX);
      #endif // VAN_RX_ISR_DEBUGGING

        // If the "ACK" came too soon, it is not an "ACK" but the first "1" bit of the next byte
        if (pinLevelChangedDuringInterruptHandling || nCycles < 500 * CPU_F_FACTOR)
        {
            atBit = 1;

            DEBUG_ISR(atBit, 1);

            readBits = 0x01;
            rxDesc->state = VAN_RX_LOADING;

            DEBUG_IFS(toState, VAN_RX_LOADING);

          #ifdef ARDUINO_ARCH_ESP32
            portEXIT_CRITICAL_ISR(&mux);
            timerEnd(timer);
          #else // ! ARDUINO_ARCH_ESP32
            timer1_disable();
          #endif // ARDUINO_ARCH_ESP32

            RETURN;
        } // if

        rxDesc->ack = VAN_ACK;

        // The timer ISR 'WaitAckIsr' will call 'VanBusRx._AdvanceHead()'

      #ifdef ARDUINO_ARCH_ESP32
        portEXIT_CRITICAL_ISR(&mux);
      #endif // ARDUINO_ARCH_ESP32

        RETURN;
    } // if

    // If the current head packet is already VAN_RX_DONE, the circular buffer is completely full
    // TODO - simply test for if (state == VAN_RX_DONE) ?
    if (state != VAN_RX_SEARCHING && state != VAN_RX_LOADING)
    {
        VanBusRx._overrun = true;
        //SetTxBitTimer();

      #ifdef ARDUINO_ARCH_ESP32
        portEXIT_CRITICAL_ISR(&mux);
      #endif // ARDUINO_ARCH_ESP32

        RETURN;
    } // if

  #ifdef VAN_RX_ISR_DEBUGGING
    if (debugIsr != NULL) debugIsr->fromJitter = _min(jitter, (1 << 10 - 1) * CPU_F_FACTOR) / CPU_F_FACTOR;
  #endif // VAN_RX_ISR_DEBUGGING

    unsigned int nBits = nBitsTakingIntoAccountJitter(nCycles, jitter);

    if (state == VAN_RX_SEARCHING && nBits == 0) nBits = 1;  // Yet another hack

    DEBUG_ISR_M(nBits, nBits, UCHAR_MAX);

    // During packet reception, the "Enhanced Manchester" encoding guarantees at most 5 bits are the same,
    // except during EOD when it can be 6.
    // However, sometimes the Manchester bit is missed. Let's be tolerant with that, and just pretend it
    // was there, by accepting up to 10 equal bits.
    if (nBits > 10)
    {
        if (state == VAN_RX_SEARCHING)
        {
            atBit = 0;

            DEBUG_ISR(atBit, 0);

            readBits = 0;
            rxDesc->size = 0;
            jitter = 0;

          #ifdef ARDUINO_ARCH_ESP32
            portEXIT_CRITICAL_ISR(&mux);
          #endif // ARDUINO_ARCH_ESP32

            RETURN;
        } // if

        rxDesc->result = VAN_RX_ERROR_NBITS;
        VanBusRx._AdvanceHead();
        //WaitAckIsr();

      #ifdef ARDUINO_ARCH_ESP32
        portEXIT_CRITICAL_ISR(&mux);
      #endif // ARDUINO_ARCH_ESP32

        RETURN;
    } // if

    // Be flexible during SOF detection
    //if (state == VAN_RX_SEARCHING && (nBits == 3 || nBits == 5)) nBits = 4;
    if (state == VAN_RX_SEARCHING)
    {
        if (nBits == 3 || nBits == 5
            || (atBit == 0 && nBits == 6)
            || (atBit == 4 && nBits == 2)
            || (atBit == 4 && nBits == 1)
            )
    {
        nBits = 4;

            DEBUG_ISR(nBits, 4);

            jitter = 0;
        }
        else if (atBit == 0 && nBits == 1)
        {
            rxDesc->state = VAN_RX_VACANT;

            DEBUG_IFS(toState, VAN_RX_VACANT);

          #ifdef ARDUINO_ARCH_ESP32
            portEXIT_CRITICAL_ISR(&mux);
          #endif // ARDUINO_ARCH_ESP32

            RETURN;
        } // if
    } // if

    // Experimental handling of special situations caused by a missed interrupt
    if (nBits == 0)
    {
        // Set or clear the last read bit
        readBits = pinLevel == VAN_LOGICAL_LOW ? readBits | 0x0001 : readBits & 0xFFFE;
    }
    else if (pinLevelChangedDuringInterruptHandling)
    {
        // Flip the last read bit
        readBits ^= 0x0001;
    }
    else if (samePinLevel)
    {
        if (nBits == 1)
        {
            flipBits = 0x0001;
        }
        else
        {
            // Flip the last 'nBits' except the very last bit, e.g. if nBits == 4 ==> flip the bits -- ---- XXX-
            flipBits = (1 << nBits) - 1 - 1;

            // If the interrupt was so late that the pin level has already changed again, then flip also the very
            // last bit
            if (jitter > 318 * CPU_F_FACTOR) flipBits |= 0x0001;
        } // if

        if ((flipBits & 0x0001) == 0x0001) prevPinLevel = 2; // TODO - prevPinLevel = 1 - prevPinLevel
    } // if

    atBit += nBits;

    DEBUG_ISR(atBit, atBit);

    readBits <<= nBits;

    // Remember: if pinLevel == VAN_LOGICAL_LOW, we've just had a series of VAN_LOGICAL_HIGH bits
    if (pinLevel == VAN_LOGICAL_LOW)
    {
        uint16_t pattern = (1 << nBits) - 1;
        readBits |= pattern;
    } // if

    readBits ^= flipBits;

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
            // - 00 0111 1101 (0x07D) : the first interrupt (0->1) is a bit too early
            // - 00 0011 1100 (0x03C) : missed Manchester bit
            // - 00 0011 1001 (0x039)
            // - 00 0011 1111 (0x03F) : missed bit
            //   -->> TODO - not sure. Will incorrectly accept ...1 0000 1111111111 0000 1111 0 1 as 0E F1 D...
            // - 01 0011 1101 (0x13D) : spurious bit in the 4 x '0' series
            //
            if (currentByte != 0x03D
                && currentByte != 0x01D && currentByte != 0x07D
                && currentByte != 0x03C //&& currentByte != 0x03F
                && currentByte != 0x039
                && currentByte != 0x13D
               )
            {
                rxDesc->state = VAN_RX_VACANT;

                DEBUG_IFS(toState, VAN_RX_VACANT);

                //SetTxBitTimer();

              #ifdef ARDUINO_ARCH_ESP32
                portEXIT_CRITICAL_ISR(&mux);
              #endif // ARDUINO_ARCH_ESP32

                RETURN;
            } // if

            currentByte = 0x03D;
            rxDesc->state = VAN_RX_LOADING;

            DEBUG_IFS(toState, VAN_RX_LOADING);
        } // if

        // Get ready for next byte
        readBits &= (1 << atBit) - 1;

        // Remove the 2 Manchester bits 'm'; the relevant 8 bits are 'X':
        //   9 8 7 6 5 4 3 2 1 0
        //   X X X X m X X X X m
        uint8_t readByte = (currentByte >> 2 & 0xF0) | (currentByte >> 1 & 0x0F);

        rxDesc->bytes[rxDesc->size++] = readByte;

        // EOD detected if last two bits are 0 followed by a 1, but never in bytes 0...4
        if ((currentByte & 0x003) == 0 && atBit == 0 && rxDesc->size >= 5

            // Experiment for 3 last "0"-bits: too short means it is not EOD
            && (nBits != 3 || nCycles + jitter + 17 > 3 * VAN_NORMAL_BIT_TIME_CPU_CYCLES))
        {
            rxDesc->state = VAN_RX_WAITING_ACK;

            DEBUG_IFS(toState, VAN_RX_WAITING_ACK);

            // Set a timeout for the ACK bit

          #ifdef ARDUINO_ARCH_ESP32

            timerEnd(timer);
            timerAttachInterrupt(timer, WaitAckIsr, true);
            timerAlarmWrite(timer, 16 * 5, false); // 2 time slots = 2 * 8 us = 16 us
            timerAlarmEnable(timer);

          #else // ! ARDUINO_ARCH_ESP32

            timer1_disable();
            timer1_attachInterrupt(WaitAckIsr);

            // Clock to timer (prescaler) is always 80MHz, even F_CPU is 160 MHz
            timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);

            timer1_write(16 * 5); // 2 time slots = 2 * 8 us = 16 us

          #endif // ARDUINO_ARCH_ESP32

        } // if
        else if (rxDesc->size >= VAN_MAX_PACKET_SIZE)
        {
            rxDesc->result = VAN_RX_ERROR_MAX_PACKET;
            VanBusRx._AdvanceHead();
            //WaitAckIsr();

          #ifdef ARDUINO_ARCH_ESP32
            portEXIT_CRITICAL_ISR(&mux);
          #endif // ARDUINO_ARCH_ESP32

            RETURN;
        } // if
    } // if

  #ifdef ARDUINO_ARCH_ESP32
    portEXIT_CRITICAL_ISR(&mux);
  #endif // ARDUINO_ARCH_ESP32

    // Pin level changed while handling the interrupt?
    const int pinLevelAtReturnFromIsr = GPIP(VanBusRx.pin);
    pinLevelChangedDuringInterruptHandling = jitter < 200 && pinLevelAtReturnFromIsr != pinLevel;

  #ifdef VAN_RX_ISR_DEBUGGING
    if (debugIsr != NULL)
    {
        debugIsr->toState = rxDesc->state;
        debugIsr->toJitter = _min(jitter, (1 << 10 - 1) * CPU_F_FACTOR) / CPU_F_FACTOR;
        debugIsr->flipBits = flipBits;
        debugIsr->pinLevelAtReturnFromIsr = pinLevelAtReturnFromIsr;
        isrDebugPacket->at++;
    } // if
  #endif // VAN_RX_ISR_DEBUGGING

    #undef RETURN

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

        s.println();

        i++;
    } // while
} // TIfsDebugPacket::Dump

#endif // VAN_RX_IFS_DEBUGGING

#ifdef VAN_RX_ISR_DEBUGGING

void TIsrDebugPacket::Dump(Stream& s) const
{
    NO_INTERRUPTS;
    if (wLock)
    {
        // Packet has not (yet) been written to, or is currently being used to write into
        INTERRUPTS;
        return;
    } // if

    rLock = true;
    INTERRUPTS;

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
        // Printing all this can take really long...
        if (i % 100 == 0) wdt_reset();

        const TIsrDebugData* isrData = samples + i;
        if (i == 0)
        {
            // Print headings
            s.print(F("  # nCycles+jitt = nTotal -> nBits atBit (nLate) pinLVLs        fromState     toState data  flip byte\n"));
        } // if

        if (i <= 1) reset();

        s.printf("%3u", i);

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
                if (atBit + i == 9) s.print("|");  // End of byte marker
            } // for
            for (int i = nBits; i < 6; i++) s.print(" ");
        } // if

        atBit += nBits;
        readBits <<= nBits;

        if (pinLevel == VAN_LOGICAL_LOW)
        {
            uint16_t pattern = (1 << nBits) - 1;
            readBits |= pattern;
        } // if

        const uint16_t flipBits = isrData->flipBits;
        if (flipBits == 0) s.print("    "); else s.printf(" %02X ", flipBits);

        readBits ^= flipBits;

        if (pinLevel != isrData->pinLevelAtReturnFromIsr)
        {
            // Flip the last read bit
            readBits ^= 0x0001;
        } // if

        if (eodSeen)
        {
            if (pinLevel == VAN_LOGICAL_LOW && nBits == 1)
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

            // Print each bit. Use small superscript characters for Manchester bits.
            for (int i = 9; i >= 6; i--) s.print(currentByte & 1 << i ? "1" : "-");
            s.print(currentByte & 1 << 5 ? "\u00b9" : "\u00b0");
            for (int i = 4; i >= 1; i--) s.print(currentByte & 1 << i ? "1" : "-");
            s.print(currentByte & 1 << 0 ? "\u00b9" : "\u00b0");

            // Get ready for next byte
            readBits &= (1 << atBit) - 1;

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

    rLock = false;  // Assumed to be atomic

} // TIsrDebugPacket::Dump

#endif // VAN_RX_ISR_DEBUGGING

TVanPacketRxQueue VanBusRx;
