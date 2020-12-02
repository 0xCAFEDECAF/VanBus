/*
 * VanBus packet receiver for ESP8266
 *
 * Written by Erik Tromp
 *
 * Version 0.2.0 - November, 2020
 *
 * MIT license, all text above must be included in any redistribution.
 */

/*
 * USAGE
 *
 *   Add the following line to your sketch:
 *     #include <VanBusRx.h>
 *
 *   In setup() :
 *     int RECV_PIN = 2; // VAN bus transceiver output is connected (via level shifter if necessary) to GPIO pin 2
 *     VanBusRx.Setup(RECV_PIN);
 *
 *   In loop() :
 *     TVanPacketRxDesc pkt;
 *     if (VanBusRx.Receive(pkt)) pkt.DumpRaw(Serial);
 */

#ifndef VanBusRx_h
#define VanBusRx_h

#include <Arduino.h>

// VAN_BIT_DOMINANT, VAN_BIT_RECESSIVE: pick the logic

// MCP2551 CAN_H pin connected to VAN_DATA, CAN_L connected to VAN_DATA_BAR
//#define VAN_BIT_DOMINANT HIGH
//#define VAN_BIT_RECESSIVE LOW

// MCP2551 CAN_H pin connected to VAN_DATA_BAR, CAN_L connected to VAN_DATA
#define VAN_BIT_DOMINANT LOW
#define VAN_BIT_RECESSIVE HIGH

// The CAN bus has two states: dominant and recessive.
// - For the MCP2551 device: "The dominant and recessive states correspond to the low and high state of the TXD input,
//   pin, respectively." and "The low and high states of the RXD output pin correspond to the dominant and recessive
//   states of the CAN bus, respectively."
//   (see: http://ww1.microchip.com/downloads/en/devicedoc/21667d.pdf#page=3 )
// - For the SN65HVD23x device: "LOW for dominant and HIGH for recessive bus states"
//   (see: https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf?ts=1592992149874#page=5 )
#define VAN_LOGICAL_LOW VAN_BIT_DOMINANT
#define VAN_LOGICAL_HIGH VAN_BIT_RECESSIVE

// F_CPU is set by the Arduino IDE option as chosen in menu Tools > CPU Frequency. It is always a multiple of 80000000.
#define CPU_F_FACTOR (F_CPU / 80000000)

#define VAN_NO_PIN_ASSIGNED (0xFF)

void RxPinChangeIsr();

#define MAX_FLOAT_SIZE 12
char* FloatToStr(char* buffer, float f, int prec = 1);

uint16_t _crc(const uint8_t bytes[], int size);

// ISR invocation data, for debugging purposes

// In theory, there can be 33 * 8 = 264 ISR invocations, but in practice 128 is enough for the vast majority of cases
struct TIsrDebugData
{
    uint32_t nCycles;
    uint32_t nCyclesProcessing;
    uint8_t pinLevel;
    uint8_t pinLevelAtReturnFromIsr;
    uint8_t slot;  // in RxQueue
}; // struct TIsrDebugData

// Buffer of ISR invocation data
class TIsrDebugPacket
{
  public:

    void Init() { at = 0; }
    void Dump(Stream& s) const;
    TIsrDebugPacket() { Init(); }  // Constructor

  private:

    #define VAN_ISR_DEBUG_BUFFER_SIZE 128
    TIsrDebugData samples[VAN_ISR_DEBUG_BUFFER_SIZE];
    int at;  // Index of next sample to write into

    friend void RxPinChangeIsr();
    friend class TVanPacketRxDesc;
}; // TIsrDebugPacket

enum PacketReadState_t { VAN_RX_VACANT, VAN_RX_SEARCHING, VAN_RX_LOADING, VAN_RX_WAITING_ACK, VAN_RX_DONE };
enum PacketReadResult_t { VAN_RX_PACKET_OK, VAN_RX_ERROR_NBITS, VAN_RX_ERROR_MANCHESTER, VAN_RX_ERROR_MAX_PACKET };
enum PacketAck_t { VAN_ACK, VAN_NO_ACK };

class Stream;

// VAN packet Rx descriptor
class TVanPacketRxDesc
{
  public:

    // VAN packet layout:
    // - SOF = 10 time slots (TS) = 8 bits = 1 byte
    // - IDEN = 15 TS = 12 bits = 1.5 bytes
    // - COM = 5 TS = 4 bits = 0.5 bytes
    // - Data = 280 TS max = 28 bytes max
    // - CRC + EOD = 18 + 2 TS = 2 bytes
    // - ACK  = 2 TS
    // - EOF  = 8 TS
    // Total 1 + 1.5 + 0.5 + 28 + 2 = 33 bytes excl. ACK and EOF

    #define VAN_MAX_DATA_BYTES 28
    #define VAN_MAX_PACKET_SIZE 33

    TVanPacketRxDesc() { Init(); }
    uint16_t Iden() const;
    uint8_t CommandFlags() const;  // See page 17 of http://ww1.microchip.com/downloads/en/DeviceDoc/doc4205.pdf
    const uint8_t* Data() const;
    int DataLen() const;
    uint16_t Crc() const;
    bool CheckCrc() const;
    bool CheckCrcAndRepair();  // Yes, we can sometimes repair a corrupt packet by flipping a single bit
    void DumpRaw(Stream& s, char last = '\n') const;

    // Example of the longest string that can be dumped (not realistic):
    // Raw: #1234 (123/123) 28(33) 0E ABC RA0 01-02-03-04-05-06-07-08-09-10-11-12-13-14-15-16-17-18-19-20-21-22-23-24-25-26-27-28:CC-DD NO_ACK VAN_RX_ERROR_MAX_PACKET CCDD CRC_ERROR
    // + 1 for terminating '\0'
    #define VAN_MAX_DUMP_RAW_SIZE (38 + VAN_MAX_DATA_BYTES * 3 + 45 + 1)

    const TIsrDebugPacket& getIsrDebugPacket() const { return isrDebugPacket; }

    // String representation of various fields.
    // Notes:
    // - Uses statically allocated buffer, so don't call twice within the same printf invocation
    // - Make sure to check VAN_MAX_DUMP_RAW_SIZE when making changes to any of the ...Str() methods.
    const char* CommandFlagsStr() const
    {
        static char result[4];
        sprintf(result, "%c%c%1u",
            bytes[2] & 0x02 ? 'R' : 'W',  // R/W
            bytes[2] & 0x04 ? 'A' : '-',   // RAK
            bytes[2] & 0x01  // RTR
        );
        return result;
    } // CommandFlagsStr
    const char* AckStr() const { return ack == VAN_ACK ? "ACK" : ack == VAN_NO_ACK ? "NO_ACK": "ACK_??"; }
    const char* ResultStr() const
    {
        return
            result == VAN_RX_PACKET_OK ? "OK" :
            result == VAN_RX_ERROR_NBITS ? "ERROR_NBITS" :
            result == VAN_RX_ERROR_MANCHESTER ? "ERROR_MANCHESTER" :
            result == VAN_RX_ERROR_MAX_PACKET ? "ERROR_MAX_PACKET" :
            "ERROR_??";
    } // ResultStr

  private:

    uint8_t bytes[VAN_MAX_PACKET_SIZE];
    int size;
    PacketReadState_t state;
    PacketReadResult_t result;
    PacketAck_t ack;
    TIsrDebugPacket isrDebugPacket;  // For debugging of packet reception inside ISR
    uint32_t seqNo;
    uint8_t slot;  // in RxQueue

    void Init()
    {
        size = 0;
        state = VAN_RX_VACANT;
        result = VAN_RX_PACKET_OK; // TODO - not necessary
        ack = VAN_NO_ACK; // TODO - not necessary
        isrDebugPacket.Init();
    } // Init

    friend void RxPinChangeIsr();
    friend class TVanPacketRxQueue;
}; // class TVanPacketRxDesc

#define ISR_SAFE_GET(TYPE, CODE) \
{ \
    noInterrupts(); \
    TYPE result = (CODE); \
    interrupts(); \
    return result; \
}

#define ISR_SAFE_SET(VAR, CODE) \
{ \
    noInterrupts(); \
    (VAR) = (CODE); \
    interrupts(); \
}

// Forward declaration
class TVanPacketTxDesc;

//  Circular buffer of VAN packet Rx descriptors
class TVanPacketRxQueue
{
  public:

    #define VAN_RX_QUEUE_SIZE 15

    // Constructor
    TVanPacketRxQueue()
        : pin(VAN_NO_PIN_ASSIGNED)
        , _head(pool)
        , tail(pool)
        , end(pool + VAN_RX_QUEUE_SIZE)
        , _overrun(false)
        , txTimerIsr(NULL)
        , txTimerTicks(0)
        , lastMediaAccessAt(0)
        , count(0)
        , nCorrupt(0)
        , nRepaired(0)
    { }

    void Setup(uint8_t rxPin);
    bool Available() const { ISR_SAFE_GET(bool, tail->state == VAN_RX_DONE); }
    bool Receive(TVanPacketRxDesc& pkt, bool* isQueueOverrun = NULL);
    uint32_t GetCount() const { ISR_SAFE_GET(uint32_t, count); }
    void DumpStats(Stream& s) const;

  private:

    uint8_t pin;
    TVanPacketRxDesc pool[VAN_RX_QUEUE_SIZE];
    TVanPacketRxDesc* volatile _head;
    TVanPacketRxDesc* tail;
    TVanPacketRxDesc* end;
    volatile bool _overrun;
    uint32_t txTimerTicks;
    timercallback txTimerIsr;
    volatile uint32_t lastMediaAccessAt;  // For carrier sense: CPU cycle counter value when last sensed

    // Some statistics. Numbers can roll over.
    uint32_t count;
    uint32_t nCorrupt;
    uint32_t nRepaired;

    void RegisterTxTimerTicks(uint32_t ticks) { txTimerTicks = ticks; };
    void RegisterTxIsr(timercallback isr) { ISR_SAFE_SET(txTimerIsr, isr); };

    uint32_t GetLastMediaAccessAt() { ISR_SAFE_GET(uint32_t, lastMediaAccessAt); };
    void SetLastMediaAccessAt(uint32_t at) { ISR_SAFE_SET(lastMediaAccessAt, at); };

    bool IsQueueOverrun() const { ISR_SAFE_GET(bool, _overrun); }
    bool ClearQueueOverrun() { ISR_SAFE_SET(_overrun, false); }

    // Only to be called from ISR, unsafe otherwise
    void ICACHE_RAM_ATTR _AdvanceHead()
    {
        _head->state = VAN_RX_DONE;
        _head->seqNo = count++;
        if (++_head == end) _head = pool;  // roll over if needed
    } // _AdvanceHead

    void AdvanceTail()
    {
        if (++tail == end) tail = pool;  // roll over if needed
    } // _AdvanceTail

    friend void FinishPacketTransmission(TVanPacketTxDesc* txDesc);
    friend void SendBitIsr();
    friend void RxPinChangeIsr();
    friend void SetTxBitTimer();
    friend void WaitAckIsr();
    friend class TVanPacketRxDesc;
    friend class TVanPacketTxQueue;
}; // class TVanPacketRxQueue

extern TVanPacketRxQueue VanBusRx;

#endif // VanBusRx_h
