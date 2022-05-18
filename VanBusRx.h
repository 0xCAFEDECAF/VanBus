/*
 * VanBus packet receiver for ESP8266
 *
 * Written by Erik Tromp
 *
 * Version 0.2.5 - January, 2022
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

#define VAN_BUX_RX_VERSION 000002005

//#define VAN_RX_ISR_DEBUGGING
//#define VAN_RX_IFS_DEBUGGING

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

#ifdef VAN_RX_ISR_DEBUGGING

// ISR invocation data, for debugging purposes

struct TIsrDebugData
{
    uint32_t nCycles:16;
    uint32_t jitterBefore:10;
    uint32_t jitterAfter:10;
    uint16_t nBits:8;
    uint16_t flipBits:8;
    uint16_t prevPinLevel:2;
    uint16_t pinLevel:1;
    uint16_t state:4;
    uint16_t pinLevelAtReturnFromIsr:1;
} __attribute__((packed)); // struct TIsrDebugData

// Buffer of ISR invocation debug data
class TIsrDebugPacket
{
  public:

    void Init() { at = 0; rLock = false; wLock = true; }
    void Dump(Stream& s) const;
    TIsrDebugPacket() { Init(); }  // Constructor

  private:

    // There can be at most 33 * 8 = 264 ISR invocations
    #define VAN_ISR_DEBUG_BUFFER_SIZE 264
    TIsrDebugData samples[VAN_ISR_DEBUG_BUFFER_SIZE];

    int at;  // Index of next sample to write into
    uint16_t slot;  // in RxQueue
    mutable bool rLock;
    bool wLock;

    friend void RxPinChangeIsr();
    friend class TVanPacketRxDesc;
    friend class TVanPacketRxQueue;
}; // TIsrDebugPacket

#endif // VAN_RX_ISR_DEBUGGING

#ifdef VAN_RX_IFS_DEBUGGING

// Data for Inter-frame space analysis
struct TIfsDebugData
{
    uint32_t nCycles:16;
    uint16_t nBits:8;
    uint16_t pinLevel:1;
    uint16_t state:4;
} __attribute__((packed)); // struct TIfsDebugData

class TIfsDebugPacket
{
  public:

    void Init() { at = 0; }
    bool IsAbnormal() const;
    void Dump(Stream& s) const;
    TIfsDebugPacket() { Init(); }  // Constructor

  private:

    #define VAN_ISF_DEBUG_BUFFER_SIZE 20
    TIfsDebugData samples[VAN_ISF_DEBUG_BUFFER_SIZE];
    int at;  // Index of next sample to write into

    friend void RxPinChangeIsr();
    friend class TVanPacketRxDesc;
}; // TIfsDebugPacket

#endif // VAN_RX_IFS_DEBUGGING

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
    __attribute__((always_inline)) uint16_t Iden() const { return bytes[1] << 4 | bytes[2] >> 4; }
    uint8_t CommandFlags() const;  // See page 17 of http://ww1.microchip.com/downloads/en/DeviceDoc/doc4205.pdf
    const uint8_t* Data() const;
    int DataLen() const;
    unsigned long Millis() { return millis_; }  // Packet time stamp in milliseconds
    uint16_t Crc() const;
    bool CheckCrc() const;
    bool CheckCrcAndRepair(bool (TVanPacketRxDesc::*wantToCount)() const = 0);  // Yes, we can sometimes repair a corrupt packet by flipping one or two bits
    void DumpRaw(Stream& s, char last = '\n') const;

    // Example of the longest string that can be dumped (not realistic):
    // Raw: #1234 (123/123) 28(33) 0E ABC RA0 01-02-03-04-05-06-07-08-09-10-11-12-13-14-15-16-17-18-19-20-21-22-23-24-25-26-27-28:CC-DD NO_ACK VAN_RX_ERROR_MAX_PACKET CCDD CRC_ERROR
    // + 1 for terminating '\0'
    #define VAN_MAX_DUMP_RAW_SIZE (38 + VAN_MAX_DATA_BYTES * 3 + 45 + 1)

  #ifdef VAN_RX_IFS_DEBUGGING
    const TIfsDebugPacket& getIfsDebugPacket() const { return ifsDebugPacket; }
  #endif // VAN_RX_IFS_DEBUGGING

  #ifdef VAN_RX_ISR_DEBUGGING
    const TIsrDebugPacket& getIsrDebugPacket() const { return *isrDebugPacket; }
  #endif // VAN_RX_ISR_DEBUGGING

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

    static const char* StateStr(uint8_t state)
    {
        return
            state == VAN_RX_VACANT ? "VACANT" :
            state == VAN_RX_SEARCHING ? "SEARCHING" :
            state == VAN_RX_LOADING ? "LOADING" :
            state == VAN_RX_WAITING_ACK ? "WAITING_ACK" :
            state == VAN_RX_DONE ? "DONE" :
            "ERROR_??";
    } // ResultStr

    __attribute__((always_inline)) bool IsSatnavPacket() const
    {
        return
            size >= 3 && 
            (
                Iden() == 0x6CE  // SATNAV_REPORT_IDEN
                || Iden() == 0x64E  // SATNAV_GUIDANCE_IDEN
            );
    } // IsSatnavPacket

  private:

    uint8_t bytes[VAN_MAX_PACKET_SIZE];
    int size;
    PacketReadState_t state;
    PacketReadResult_t result;
    PacketAck_t ack;
    unsigned long millis_;  // Packet time stamp in milliseconds

  #ifdef VAN_RX_ISR_DEBUGGING
    TIsrDebugPacket* isrDebugPacket;  // For debugging of packet reception inside ISR
  #endif // VAN_RX_ISR_DEBUGGING

  #ifdef VAN_RX_IFS_DEBUGGING
    TIfsDebugPacket ifsDebugPacket;  // For debugging of inter-frame space
  #endif // VAN_RX_IFS_DEBUGGING

    uint32_t seqNo;
    uint16_t slot;  // in RxQueue

    void Init()
    {
        size = 0;
        state = VAN_RX_VACANT;
        result = VAN_RX_PACKET_OK;
        ack = VAN_NO_ACK;

      #ifdef VAN_RX_IFS_DEBUGGING
        ifsDebugPacket.Init();
      #endif // VAN_RX_IFS_DEBUGGING
    } // Init

    friend void RxPinChangeIsr();
    friend class TVanPacketRxQueue;
}; // class TVanPacketRxDesc

#ifdef ARDUINO_ARCH_ESP32
extern hw_timer_t * timer;
extern portMUX_TYPE mux;
#endif // ARDUINO_ARCH_ESP32

#ifdef ARDUINO_ARCH_ESP32

    #define NO_INTERRUPTS portENTER_CRITICAL(&mux)
    #define INTERRUPTS portEXIT_CRITICAL(&mux)

#else // ! ARDUINO_ARCH_ESP32

    #define NO_INTERRUPTS noInterrupts()
    #define INTERRUPTS interrupts()

#endif // ARDUINO_ARCH_ESP32

#define ISR_SAFE_GET(TYPE, CODE) \
{ \
    NO_INTERRUPTS; \
    TYPE result = (CODE); \
    INTERRUPTS; \
    return result; \
}

#define ISR_SAFE_SET(VAR, CODE) \
{ \
    NO_INTERRUPTS; \
    (VAR) = (CODE); \
    INTERRUPTS; \
}

// Forward declaration
class TVanPacketTxDesc;

#ifdef ARDUINO_ARCH_ESP32
typedef void(*timercallback)(void);
#endif // ARDUINO_ARCH_ESP32

//  Circular buffer of VAN packet Rx descriptors
class TVanPacketRxQueue
{
  public:

    #define VAN_DEFAULT_RX_QUEUE_SIZE 15

    // Constructor
    TVanPacketRxQueue()
        : pin(VAN_NO_PIN_ASSIGNED)
        , _overrun(false)
        , txTimerTicks(0)
        , txTimerIsr(NULL)
        , lastMediaAccessAt(0)
      #ifdef VAN_RX_ISR_DEBUGGING
        , isrDebugPacket(isrDebugPacketPool)
      #endif // VAN_RX_ISR_DEBUGGING
        , count(0)
        , nCorrupt(0)
        , nRepaired(0)
        , nOneBitError(0)
        , nTwoConsecutiveBitErrors(0)
        , nTwoSeparateBitErrors(0)
    { }

    bool Setup(uint8_t rxPin, int queueSize = VAN_DEFAULT_RX_QUEUE_SIZE);
    bool Available() const { ISR_SAFE_GET(bool, tail->state == VAN_RX_DONE); }
    bool Receive(TVanPacketRxDesc& pkt, bool* isQueueOverrun = NULL);

    // Disabling the VAN bus receiver is necessary for timer-intensive tasks, like e.g. operations on the SPI Flash
    // File System (SPIFFS), which otherwise cause system crash. Unfortunately, after disabling then enabling the
    // VAN bus receiver like this, the CRC error rate seems to increase...
    void Disable();
    void Enable();

    bool IsSetup() const { return pin != VAN_NO_PIN_ASSIGNED; }
    uint32_t GetCount() const { ISR_SAFE_GET(uint32_t, count); }
    void DumpStats(Stream& s, bool longForm = true) const;
    int QueueSize() const { return size; }

  private:

    uint8_t pin;
    int size;
    TVanPacketRxDesc* pool;
    TVanPacketRxDesc* volatile _head;
    TVanPacketRxDesc* tail;
    TVanPacketRxDesc* end;
    volatile bool _overrun;
    uint32_t txTimerTicks;
    timercallback txTimerIsr;
    volatile uint32_t lastMediaAccessAt;  // For carrier sense: CPU cycle counter value when last sensed

  #ifdef VAN_RX_ISR_DEBUGGING
    #define N_ISR_DEBUG_PACKETS 3
    TIsrDebugPacket isrDebugPacketPool[N_ISR_DEBUG_PACKETS];
    TIsrDebugPacket* isrDebugPacket;
  #endif // VAN_RX_ISR_DEBUGGING

    // Some statistics. Numbers can roll over.
    uint32_t count;
    uint32_t nCorrupt;
    uint32_t nRepaired;
    uint32_t nOneBitError;
    uint32_t nTwoConsecutiveBitErrors;
    uint32_t nTwoSeparateBitErrors;

    void RegisterTxTimerTicks(uint32_t ticks) { txTimerTicks = ticks; };
    void RegisterTxIsr(timercallback isr) { ISR_SAFE_SET(txTimerIsr, isr); };

    uint32_t GetLastMediaAccessAt() { ISR_SAFE_GET(uint32_t, lastMediaAccessAt); };
    void SetLastMediaAccessAt(uint32_t at) { ISR_SAFE_SET(lastMediaAccessAt, at); };

    bool IsQueueOverrun() { NO_INTERRUPTS; bool result = _overrun; _overrun = false; INTERRUPTS; }

    // Only to be called from ISR, unsafe otherwise
    __attribute__((always_inline)) void _AdvanceHead()
    {
        _head->millis_ = millis();
        _head->state = VAN_RX_DONE;
        _head->seqNo = count++;

      #ifdef VAN_RX_ISR_DEBUGGING

        // Keep the ISR debug packet if CRC is wrong, otherwise just overwrite
        if (! _head->CheckCrc())
        {
            isrDebugPacket->wLock = false;  // Indicate this debug packet is free for reading

            // Move to the next debug packet, but skip it if it is currently being read
            do
            {
                isrDebugPacket++;
                if (isrDebugPacket == isrDebugPacketPool + N_ISR_DEBUG_PACKETS) isrDebugPacket = isrDebugPacketPool;
            } while (isrDebugPacket->rLock && isrDebugPacket != _head->isrDebugPacket);
        } // if
      #endif // VAN_RX_ISR_DEBUGGING

        if (++_head == end) _head = pool;  // roll over if needed

      #ifdef VAN_RX_ISR_DEBUGGING
        isrDebugPacket->Init();
        _head->isrDebugPacket = isrDebugPacket;
      #endif // VAN_RX_ISR_DEBUGGING

      #ifdef VAN_RX_IFS_DEBUGGING
        _head->ifsDebugPacket.Init();
      #endif // VAN_RX_IFS_DEBUGGING
    } // _AdvanceHead

    void AdvanceTail()
    {
        if (++tail == end) tail = pool;  // roll over if needed
    } // AdvanceTail

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
