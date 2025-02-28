/*
 * VanBus packet transmitter for ESP8266
 *
 * Written by Erik Tromp
 *
 * Version 0.4.1 - September, 2024
 *
 * MIT license, all text above must be included in any redistribution.
 */

#include "VanBus.h"

// Normally this value should be 8 * 5 to have a 1-bit time of 8 microseconds.
// However, it seems that results may be better when adding a few tenths of a microsecond.
#ifdef ARDUINO_ARCH_ESP32
  #define VAN_BIT_TIMER_TICKS (8 * 5 + 3)
#else // ! ARDUINO_ARCH_ESP32
  #define VAN_BIT_TIMER_TICKS (8 * 5 + 1)
#endif // ARDUINO_ARCH_ESP32

// Finish packet transmission
void IRAM_ATTR FinishPacketTransmission(TVanPacketTxDesc* txDesc)
{
    // Save statistics
    if (txDesc->nCollisions != 0)
    {
        if (txDesc->nCollisions == 1) ++VanBusTx.nSingleCollisions; else ++VanBusTx.nMultipleCollisions;
    } // if

    VanBusTx._AdvanceTail();

    // Nothing more to send?
    if (VanBusTx._tail->state == VAN_TX_DONE)
    {
        VanBusRx.RegisterTxIsr(NULL);

      #ifdef ARDUINO_ARCH_ESP32
        timerEnd(timer);
      #else // ! ARDUINO_ARCH_ESP32
        timer1_disable();
      #endif // ARDUINO_ARCH_ESP32
        TVanPacketTxQueue::alarmEnabled = false;
    } // if 

    VanBusRx.SetLastMediaAccessAt(ESP.getCycleCount()); // It was me! :-)

    // Start listening again at other devices on the bus
    attachInterrupt(digitalPinToInterrupt(VanBusRx.pin), RxPinChangeIsr, CHANGE);
} //

// Send one bit on the VAN bus
void IRAM_ATTR SendBitIsr()
{
    uint32_t curr = ESP.getCycleCount();  // Store CPU cycle counter value as soon as possible

    static unsigned int atBit = 9;

    static uint16_t* p_stuffedByte;

    TVanPacketTxDesc* txDesc = VanBusTx._tail;

    //if (txDesc->state == VAN_TX_DONE) return;

    if (txDesc->state == VAN_TX_WAITING)
    {
        // Wait at least 8 (EOF) + 4 (IFS) bits after last media access
        uint32_t nCycles = curr - VanBusRx.GetLastMediaAccessAt();  // Arithmetic has safe roll-over
        if (nCycles < (8 /* EOF */ + 5 /* IFS */) * (VAN_BIT_TIMER_TICKS * 16) * CPU_F_FACTOR)
        {
            txDesc->busOccupied = true;
            return;
        } // if

        // Don't waste precious CPU time handling the RX pin interrupts of my own transmission.
        // TODO - this will cause any colliding incoming packet to be not received by the receiver.
        detachInterrupt(digitalPinToInterrupt(VanBusRx.pin));

        txDesc->interFrameCpuCycles = nCycles;
        txDesc->state = VAN_TX_SENDING;
        atBit = 9;
        p_stuffedByte = txDesc->stuffedBytes;
    } // if

    static int lastSetLevel = VAN_BIT_RECESSIVE;

    // Detect collision and bit errors until (but not including) the EOD. Otherwise we will see an ACK bit from the
    // receiver as a collision.
    if (p_stuffedByte < txDesc->p_eod)
    {
        // Check if previously transmitted bit has been copied by reading RX pin

    #ifdef ARDUINO_ARCH_ESP32
        int pinLevel = digitalRead(VanBusRx.pin);
    #else // ! ARDUINO_ARCH_ESP32
        int pinLevel = GPIP(VanBusRx.pin);
    #endif // ARDUINO_ARCH_ESP32

        if (pinLevel == VAN_BIT_DOMINANT && lastSetLevel == VAN_BIT_RECESSIVE)
        {
            int atByte = p_stuffedByte - txDesc->stuffedBytes;
            if (txDesc->nCollisions == 0) txDesc->firstCollisionAtBit = atByte * 10 + (9 - atBit);
            txDesc->nCollisions++;

            // Backout and start all over again
            txDesc->state = VAN_TX_WAITING;
        } // if

        if (pinLevel == VAN_BIT_RECESSIVE && lastSetLevel == VAN_BIT_DOMINANT) txDesc->bitError = true;

        if (pinLevel == lastSetLevel) txDesc->bitOk = true;
    } // if

    uint16_t byte = *p_stuffedByte;
    uint16_t bit = byte & (1 << atBit); // TODO - use static bitMask variable: bitmask <<= 1;

    // Write to GPIO pin
    if (bit != 0)
    {
    #ifdef ARDUINO_ARCH_ESP32
        REG_WRITE(GPIO_OUT_W1TS_REG, 1 << VanBusTx.txPin);
    #else // ! ARDUINO_ARCH_ESP32
        GPOS = (1 << VanBusTx.txPin);
    #endif // ARDUINO_ARCH_ESP32
        lastSetLevel = VAN_BIT_RECESSIVE;
    }
    else
    {
    #ifdef ARDUINO_ARCH_ESP32
        REG_WRITE(GPIO_OUT_W1TC_REG, 1 << VanBusTx.txPin);
    #else // ! ARDUINO_ARCH_ESP32
        GPOC = (1 << VanBusTx.txPin);
    #endif // ARDUINO_ARCH_ESP32
        lastSetLevel = VAN_BIT_DOMINANT;
    } // if

    // Advance to next bit
    if (atBit-- == 0)
    {
        // Advance to next byte
        atBit = 9;

        // Finished sending packet?
        if (++p_stuffedByte == txDesc->p_last) FinishPacketTransmission(txDesc);
    } // if
} // SendBitIsr

void IRAM_ATTR sendBitIsrWrapper(void *arg) {
    (void)arg;  // Ignore arg
    SendBitIsr();
}

// Initializes the VAN packet transmitter
void TVanPacketTxQueue::Setup(uint8_t theRxPin, uint8_t theTxPin)
{
    txPin = theTxPin;

    pinMode(theTxPin, OUTPUT);
    digitalWrite(theTxPin, VAN_BIT_RECESSIVE);  // Set bus state to 'recessive' (CANH and CANL: not driven)

    VanBusRx.Setup(theRxPin);
    VanBusRx.RegisterTxTimerTicks(VAN_BIT_TIMER_TICKS);
} // TVanPacketTxQueue::Setup

// Send data as a packet on the VAN bus
void TVanPacketTxDesc::PreparePacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen)
{
    Init();

    n = VanBusTx.GetCount();

    // Send at most VAN_MAX_DATA_BYTES data
    if (dataLen > VAN_MAX_DATA_BYTES) dataLen = VAN_MAX_DATA_BYTES;

    // Prepare full packet data
    uint8_t bytes[VAN_MAX_PACKET_SIZE];
    bytes[0] = 0x0E;  // SOF
    bytes[1] = iden >> 4 & 0xFF;  // IDEN (MSB 8 bits)
    bytes[2] = iden << 4 | 0x08 | (cmdFlags & 0x07);  // IDEN (LSB 4 bits), fixed-1 (1 bit), COM (3 bits)
    memcpy(bytes + 3, data, dataLen);
    uint16_t crc = _crc(bytes, dataLen + 5);
    bytes[dataLen + 3] = crc >> 8;
    bytes[dataLen + 4] = crc & 0xFF;

    // Stuff with Manchester bits
    for (size_t i = 0; i < dataLen + 5; i++)
    {
        uint8_t byte = bytes[i];
        stuffedBytes[i] = (byte & 0xF0) << 2 | (~ byte & 0x10) << 1 | (byte & 0x0F) << 1 | (~ byte & 0x01);
    } // for

    // The last bit is always 0 (CRC has been shifted left 1 bit), and the last Manchester bit is also always 0,
    // to indicate EOD
    stuffedBytes[dataLen + 4] &= 0xFFFC;
    eodAt = dataLen + 5;
    p_eod = stuffedBytes + dataLen + 5;

    // End with 10 VAN_LOGICAL_HIGH-bits: 2 bits for the (optional) ACK, then 8 bits for EOF
    stuffedBytes[dataLen + 5] = 0xFFFF;
    size = dataLen + 5 + 1;  // Adding 1 for the last 10 VAN_LOGICAL_HIGH-bits
    p_last = stuffedBytes + dataLen + 5 + 1;

    state = VAN_TX_WAITING;
} // TVanPacketTxDesc::PreparePacket

// Print information about a transmitted package
void TVanPacketTxDesc::Dump() const
{
    // Only for transmitted packets
    if (state != VAN_TX_DONE) return;

    // Only if there is something interesting to print
    if (! busOccupied && bitOk && nCollisions == 0 && ! bitError) return;

    uint32_t ifsBits = interFrameCpuCycles / CPU_F_FACTOR / VAN_BIT_TIMER_TICKS / 16;
    Serial.printf_P(PSTR("#%" PRIu32 ", ifsBits=%" PRIu32 "%s"), n, ifsBits, busOccupied ? ", busOccupied" : "");

    if (nCollisions > 0)
    {
        Serial.printf_P(PSTR(", nCollisions=%" PRIu32 ", firstCollisionAtBit=%" PRIu32), nCollisions, firstCollisionAtBit);
    } // if

    Serial.printf_P(PSTR("%s%s\n"), bitOk ? "" : ", NO bitOk", bitError ? ", bitError" : "");
} // TVanPacketTxDesc::Dump

void TVanPacketTxQueue::StartBitSendTimer()
{
    VanBusRx.RegisterTxIsr(&SendBitIsr);

    // TODO - wait here until:
    // nCycles >= (8 /* EOF */ + 5 /* IFS */) * (VAN_BIT_TIMER_TICKS * 16) * CPU_F_FACTOR
    // If we start the SendBitIsr now, we might introduce extra wobbling in the RxPinChangeIsr, causing CRC errors
    // Preference is to not have the timer1 interrupt handler being called while a packet is being received.

    //uint32_t curr = ESP.getCycleCount();
    //uint32_t nCycles = curr - VanBusRx.GetLastMediaAccessAt();  // Arithmetic has safe roll-over
    //if (nCycles < (8 /* EOF */ + 5 /* IFS */) * (VAN_BIT_TIMER_TICKS * 16) * CPU_F_FACTOR) return;

    NO_INTERRUPTS;

    // Transmitting a packet is done completely by interrupt-servicing

#ifdef ARDUINO_ARCH_ESP32

    if (!alarmEnabled)
    {
        // Set a repetitive timer
        timerEnd(timer);
        timerAttachInterruptArg(timer, sendBitIsrWrapper, nullptr);
        timerAlarm(timer, VAN_BIT_TIMER_TICKS, true, 0);
        alarmEnabled = true;
    } // if

#else // ! ARDUINO_ARCH_ESP32

    if (! timer1_enabled())
    {
        // Set a repetitive timer
        timer1_disable();
        timer1_attachInterrupt(SendBitIsr);

        // Clock to timer (prescaler) is always 80MHz, even F_CPU is 160 MHz
        timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);

        timer1_write(VAN_BIT_TIMER_TICKS);
    } // if

#endif // ARDUINO_ARCH_ESP32

    INTERRUPTS;
} // void TVanPacketTxQueue::StartBitSendTimer

// Wait until the head of the queue is available. When 'timeOutMs' is set to 0, will wait forever.
bool TVanPacketTxQueue::WaitForHeadAvailable(unsigned int timeOutMs)
{
    unsigned int waitPoll = timeOutMs;

    // Relying on short-circuit boolean evaluation
    while (!SlotAvailable()) {
        if (timeOutMs != 0) {
            if (waitPoll == 0)
                break;
            waitPoll--;
        }
        delay(1);
    }


    return SlotAvailable();
} // TVanPacketTxQueue::WaitForHeadAvailable

// Synchronous packet send: returns as soon as the packet was transmitted.
// Will wait at most 'timeOutMs' milliseconds. When 'timeOutMs' is set to 0, will wait forever
bool TVanPacketTxQueue::SyncSendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs)
{
    // If the Tx queue is full, wait a bit
    if (! WaitForHeadAvailable(timeOutMs))
    {
        ++nDropped;
        return false;
    } // if

    _head->PreparePacket(iden, cmdFlags, data, dataLen);
    StartBitSendTimer();

    // Wait here for the packet transmission to be finished
    if (! WaitForHeadAvailable()) return false;

    AdvanceHead();

    return true;
} // TVanPacketTxQueue::SyncSendPacket

// Asynchronous packet send: queues the packet to be transmitted then returns.
// If the TX queue is full, will wait at most 'timeOutMs' milliseconds. When 'timeOutMs' is set to 0, will wait forever.
bool TVanPacketTxQueue::SendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs)
{
    // If the Tx queue is full, wait a bit
    if (! WaitForHeadAvailable(timeOutMs))
    {
        ++nDropped;
        return false;
    } // if

    _head->PreparePacket(iden, cmdFlags, data, dataLen);
    StartBitSendTimer();

    AdvanceHead();

    return true;
} // TVanPacketTxQueue::SendPacket

// Dumps packet statistics
void TVanPacketTxQueue::DumpStats(Stream& s) const
{
    s.printf_P(
        PSTR("transmitted pkts: %" PRIu32 ", single collisions: %" PRIu32 ", multiple collisions: %" PRIu32 ", dropped: %" PRIu32 "\n"),
        GetCount(),
        nSingleCollisions,
        nMultipleCollisions,
        nDropped
    );
} // TVanPacketTxQueue::DumpStats

TVanPacketTxQueue VanBusTx;
