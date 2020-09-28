/*
 * VanBus packet receiver for ESP8266
 *
 * Written by Erik Tromp
 *
 * Version 0.1.2 - September, 2020
 *
 * MIT license, all text above must be included in any redistribution.   
 */

/*
 * USAGE
 *
 *   Add the following line to your sketch:
 *     #include <VanBus.h>
 *
 *   In setup() :
 *     int RECV_PIN = 2; // VAN bus transceiver output is connected (via level shifter if necessary) to GPIO pin 2
 *     VanBus.Setup(RECV_PIN);
 *
 *   In loop() :
 *     TVanPacketRxDesc pkt;
 *     if (VanBus.Receive(pkt)) pkt.DumpRaw(Serial);
 */

#ifndef VanBus_h
#define VanBus_h

#include <Arduino.h>

// BIT_DOMINANT, BIT_RECESSIVE: pick the logic

// When connected to "DATA" (positive logic)
#define BIT_DOMINANT HIGH
#define BIT_RECESSIVE LOW

// When connected to "DATA-BAR" (inverse logic)
//#define BIT_DOMINANT LOW
//#define BIT_RECESSIVE HIGH

// The CAN bus has two states: dominant and recessive. The dominant state represents logic 0, and the recessive
// state represents logic 1.
#define LOGICAL_LOW BIT_DOMINANT
#define LOGICAL_HIGH BIT_RECESSIVE

// Forward declaration, needed to be able to add these as friend functions to the declared classes
static void PinChangeIsr();
void WaitAckIsr();

#define MAX_FLOAT_SIZE 12
char* FloatToStr(char* buffer, float f, int prec = 1);

// ISR invocation data, for debugging purposes

// In theory, there can be 33 * 8 = 264 ISR invocations, but in practice 128 is enough for the vast majority of cases
struct TIsrDebugData
{
    uint32_t nCycles;
    uint32_t nCyclesProcessing;
    uint8_t pinLevel;
    uint8_t pinLevelAtReturnFromIsr;
    uint8_t slot;
}; // struct TIsrDebugData

// Buffer of ISR invocation data
class TIsrDebugPacket
{
  public:

    void Init() { at = 0; }
    void Dump(Stream& s) const;
    TIsrDebugPacket() { Init(); }  // Constructor

  private:

    #define ISR_DEBUG_BUFFER_SIZE 128
    TIsrDebugData samples[ISR_DEBUG_BUFFER_SIZE];
    int at;  // Index of next sample to write into

    friend void PinChangeIsr();
    friend class TVanPacketRxDesc;
}; // TIsrDebugPacket

enum PacketReadState_t { VACANT, SEARCHING, LOADING, WAITING_ACK, DONE };
enum PacketReadResult_t { PACKET_OK, ERROR_NBITS, ERROR_MANCHESTER, ERROR_MAX_PACKET };
enum PacketAck_t { VAN_ACK, VAN_NO_ACK };

class Stream;

// VAN packet descriptor
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
    // - EOF  = 5 TS
    // Total 1 + 1.5 + 0.5 + 28 + 2 = 33 bytes excl. ACK and EOF

    #define MAX_DATA_BYTES 28
    #define MAX_PACKET_SIZE 33

    TVanPacketRxDesc() { Init(); }
    uint16_t Iden() const;
    uint8_t Flags() const;
    const uint8_t* Data() const;
    int DataLen() const;
    uint16_t Crc() const;
    bool CheckCrc() const;
    bool CheckCrcAndRepair();  // Yes, we can sometimes repair a packet by flipping a single bit
    void DumpRaw(Stream& s, char last = '\n') const;

    // Example of the longest string that can be dumped (not realistic):
    // Raw: #1234 (123/123) 28(33) 0E ABC RA0 01-02-03-04-05-06-07-08-09-10-11-12-13-14-15-16-17-18-19-20-21-22-23-24-25-26-27-28:CC-DD NO_ACK ERROR_MAX_PACKET CCDD CRC_ERROR
    // + 1 for terminating '\0'
    #define MAX_DUMP_RAW_SIZE (83 + MAX_DATA_BYTES * 3 + 1)

    const TIsrDebugPacket& getIsrDebugPacket() const { return isrDebugPacket; }

    // String representation of various fields.
    // Note: make sure to check MAX_DUMP_RAW_SIZE when making changes to any of the ...Str() methods.
    const char* FlagsStr() const
    {
        // Note: statically allocated buffer, so don't call twice within the same printf invocation
        static char result[4];
        sprintf(result, "%c%c%1u",
            bytes[2] & 0x02 ? 'R' : 'W',  // R/W
            bytes[2] & 0x04 ? 'A' : '-',   // RAK
            bytes[2] & 0x01  // RTR
        );
        return result;
    } // FlagsStr
    const char* AckStr() const { return ack == VAN_ACK ? "ACK" : ack == VAN_NO_ACK ? "NO_ACK": "ACK_UNKOWN"; }
    const char* ResultStr() const
    {
        return
            result == PACKET_OK ? "OK" :
            result == ERROR_NBITS ? "ERROR_NBITS" :
            result == ERROR_MANCHESTER ? "ERROR_MANCHESTER" :
            result == ERROR_MAX_PACKET ? "ERROR_MAX_PACKET" :
            "ERROR_UNKNOWN";
    } // ResultStr

  private:

    uint8_t bytes[MAX_PACKET_SIZE];
    int size;
    PacketReadState_t state;
    PacketReadResult_t result;
    PacketAck_t ack;
    TIsrDebugPacket isrDebugPacket;  // For debugging of packet reception inside ISR
    uint32_t seqNo;

    void Init()
    {
        size = 0;
        state = VACANT;
        result = PACKET_OK; // TODO - not necessary
        ack = VAN_NO_ACK; // TODO - not necessary
        isrDebugPacket.Init();
    } // Init

    friend void PinChangeIsr();
    friend class TVanPacketRxQueue;
}; // struct TVanPacketRxDesc

#define ISR_SAFE_GET(TYPE, CODE) \
{ \
    noInterrupts(); \
    TYPE result = (CODE); \
    interrupts(); \
    return result; \
}

void SendPacket(uint16_t iden, uint8_t flags, const char* data, size_t len);

//  Circular buffer of VAN packet Rx descriptors
class TVanPacketRxQueue
{
  public:

    #define RX_QUEUE_SIZE 15

    // Constructor
    TVanPacketRxQueue()
        : _head(pool)
        , tail(pool)
        , end(pool + RX_QUEUE_SIZE)
        , _overrun(false)
        , count(0)
        , nCorrupt(0)
        , nRepaired(0)
    { }

    void Setup(uint8_t rxPin);
    bool Available() const { ISR_SAFE_GET(bool, tail->state == DONE); }
    bool Receive(TVanPacketRxDesc& pkt, bool* isQueueOverrun = NULL);
    uint32_t GetCount() const { ISR_SAFE_GET(uint32_t, count); }
    void DumpStats(Stream& s) const;

  private:

    uint8_t pin;
    TVanPacketRxDesc pool[RX_QUEUE_SIZE];
    TVanPacketRxDesc* volatile _head;
    TVanPacketRxDesc* tail;
    TVanPacketRxDesc* end;
    volatile bool _overrun;

    // Some statistics. Numbers can roll over.
    uint32_t count;
    uint32_t nCorrupt;
    uint32_t nRepaired;

    bool _IsQueueOverrun() const { ISR_SAFE_GET(bool, _overrun); }

    // Only to be called from ISR, unsafe otherwise
    void ICACHE_RAM_ATTR _AdvanceHead()
    {
        _head->state = DONE;
        _head->seqNo = count++;
        if (++_head == end) _head = pool;  // roll over if needed
    } // _AdvanceHead

    friend void PinChangeIsr();
    friend void WaitAckIsr();
    friend class TVanPacketRxDesc;
}; // struct TVanPacketRxQueue

extern TVanPacketRxQueue VanBus;

#endif // VanBus_h
