/*
 * VanBus packet transmitter for ESP8266
 *
 * Written by Erik Tromp
 *
 * Version 0.4.1 - September, 2024
 *
 * MIT license, all text above must be included in any redistribution.
 */

/*
 * USAGE
 *
 *   Add the following line to your sketch:
 *     #include <VanBusTx.h>
 *
 *   In setup() :
 *     int TX_PIN = D3; // VAN bus transceiver input is connected (via level shifter if necessary) to GPIO pin 3
 *     int RX_PIN = D2; // VAN bus transceiver output is connected (via level shifter if necessary) to GPIO pin 2
 *     VanBusTx.Setup(RX_PIN, TX_PIN);
 *
 *   In loop() :
 *     uint8_t rmtTemperatureBytes[] = {0x0F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x70};
 *     VanBusTx.SendPacket(0x8A4, 0x08, rmtTemperatureBytes, sizeof(rmtTemperatureBytes));
 */

#ifndef VanBusTx_h
#define VanBusTx_h

#include "VanBusRx.h"

enum PacketWriteState_t { VAN_TX_WAITING, VAN_TX_SENDING, VAN_TX_DONE };

// VAN packet Tx descriptor
class TVanPacketTxDesc
{
  public:
    TVanPacketTxDesc() { Init(); }
    void PreparePacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen);
    void Dump() const;

  private:

    uint16_t stuffedBytes[VAN_MAX_PACKET_SIZE + 1];  // 1 extra "byte": 2 ACK bits and 8 EOF bits
    uint16_t* p_eod;
    uint16_t* p_last;
    uint32_t n;
    unsigned int size;
    unsigned int eodAt;
    volatile PacketWriteState_t state;

    #define VAN_TX_MAX_COLLISIONS 10

    uint32_t nCollisions;
    uint32_t firstCollisionAtBit;
    bool bitError;
    bool bitOk;
    bool busOccupied;
    uint32_t interFrameCpuCycles;  // Inter-Frame Spacing (IFS) after last received packet, counted in CPU cycles

    void Init()
    {
        size = 0;
        eodAt = 0;
        state = VAN_TX_DONE;
        nCollisions = 0;
        firstCollisionAtBit = 0;
        bitError = false;
        bitOk = false;
        busOccupied = false;
    } // Init

    friend void FinishPacketTransmission(TVanPacketTxDesc* txDesc);
    friend void SendBitIsr();
    friend class TVanPacketTxQueue;
}; // class TVanPacketTxDesc

// Circular buffer of VAN packet Tx descriptors
class TVanPacketTxQueue
{
  public:

    #define VAN_TX_QUEUE_SIZE 5

    // Constructor
    TVanPacketTxQueue()
        : txPin(VAN_NO_PIN_ASSIGNED)
        , _head(pool)
        , _tail(pool)
        , end(pool + VAN_TX_QUEUE_SIZE)
        , count(0)
        , nDropped(0)
        , nSingleCollisions(0)
        , nMultipleCollisions(0)
        , nMaxCollisionErrors(0)
    { }

    void Setup(uint8_t theRxPin, uint8_t theTxPin);
    bool SyncSendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10);
    bool SendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10);
    uint32_t GetCount() const { ISR_SAFE_GET(uint32_t, count); }
    void DumpStats(Stream& s) const;

  private:

    uint8_t txPin;
    TVanPacketTxDesc pool[VAN_TX_QUEUE_SIZE];
    TVanPacketTxDesc* volatile _head;
    TVanPacketTxDesc* volatile _tail;
    TVanPacketTxDesc* end;

    // Some statistics. Numbers can roll over.
    uint32_t count;
    uint32_t nDropped;
    uint32_t nSingleCollisions;
    uint32_t nMultipleCollisions;
    uint32_t nMaxCollisionErrors;

    bool SlotAvailable() const { ISR_SAFE_GET(bool, _head->state == VAN_TX_DONE); }
    static void StartBitSendTimer();
    bool WaitForHeadAvailable(unsigned int timeOutMs = 10);

    // Only to be called from ISR, unsafe otherwise
    void IRAM_ATTR _AdvanceTail()
    {
        _tail->state = VAN_TX_DONE;
        _tail = _tail + 1;
        if (_tail == end) _tail = pool;  // roll over if needed
    } // _AdvanceTail

    void AdvanceHead()
    {
        _head = _head + 1;
        if (_head == end) _head = pool;  // roll over if needed
        count++;
    } // AdvanceHead

    friend void FinishPacketTransmission(TVanPacketTxDesc* txDesc);
    friend void SendBitIsr();
    friend class TVanPacketTxDesc;
}; // class TVanPacketTxQueue

extern TVanPacketTxQueue VanBusTx;

#endif // VanBusTx_h
