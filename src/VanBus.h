/*
 * VanBus packet receiver and transmitter for ESP8266
 *
 * Written by Erik Tromp
 *
 * Version 0.3.4 - October, 2023
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
 *     int TX_PIN = D3; // VAN bus transceiver input is connected (via level shifter if necessary) to GPIO pin 3
 *     int RX_PIN = D2; // VAN bus transceiver output is connected (via level shifter if necessary) to GPIO pin 2
 *     VanBus.Setup(RX_PIN, TX_PIN);
 *
 *   In loop() :
 *     TVanPacketRxDesc pkt;
 *     if (VanBus.Receive(pkt)) pkt.DumpRaw(Serial);
 *
 *     uint8_t rmtTemperatureBytes[] = {0x0F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x70};
 *     VanBus.SendPacket(0x8A4, 0x08, rmtTemperatureBytes, sizeof(rmtTemperatureBytes));
 */

#ifndef VanBus_h
#define VanBus_h

#include "VanBusRx.h"
#include "VanBusTx.h"

// Interface object
class TVanBus
{
  public:

    // -----
    // Interfaces for both Tx and Rx
    static void Setup(uint8_t rxPin, uint8_t txPin) { VanBusTx.Setup(rxPin, txPin); }

    static void DumpStats(Stream& s, bool longForm = true)
    {
        VanBusTx.DumpStats(s);
        VanBusRx.DumpStats(s, longForm);
    } // DumpStats

    // -----
    // Rx interfaces
    static bool Available() { return VanBusRx.Available(); }

    static bool Receive(TVanPacketRxDesc& pkt, bool* isQueueOverrun = NULL)
    {
        return VanBusRx.Receive(pkt, isQueueOverrun);
    } // Receive

    static uint32_t GetRxCount() { return VanBusRx.GetCount(); }
    static int QueueSize() { return VanBusRx.QueueSize(); }
    static int GetNQueued() { return VanBusRx.GetNQueued(); }
    static int GetMaxQueued() { return VanBusRx.GetMaxQueued(); }
    static void SetDropPolicy(int startAt, bool (*isEssential)(const TVanPacketRxDesc&) = 0)
    {
        return VanBusRx.SetDropPolicy(startAt, isEssential);
    } // SetDropPolicy

    // -----
    // Tx interfaces
    static bool SyncSendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10)
    {
        return VanBusTx.SyncSendPacket(iden, cmdFlags, data, dataLen, timeOutMs);
    } // SyncSendPacket

    static bool SendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10)
    {
        return VanBusTx.SendPacket(iden, cmdFlags, data, dataLen, timeOutMs);
    } // SendPacket

    static uint32_t GetTxCount() { return VanBusTx.GetCount(); }

}; // class TVanBus

// For those who prefer to write things like 'VanBus.Setup(...)' instead of 'TVanBus::Setup(...)'
// Note: the 'VanBus' object is never actually instantiated. It is only declared here to make the compiler happy.
extern TVanBus VanBus;

#endif // VanBus_h
