<h3 align="center">PSA (Peugeot, Citroën) VAN bus reader/writer for Arduino-ESP8266</h3>

---

<p align="center">Reading and writing packets from/to PSA vehicles' VAN bus.</p>

## 📝 Table of Contents
- [Description](#description)
- [Schematics](#schematics)
- [Usage](#usage)
  - [General](#general)
  - [Functions](#functions)
- [Work to be Done](#todo)
- [License](#license)

## 🎈 Description<a name = "description"></a>

This module allows you to receive and transmit packets on the ["VAN" bus] of your Peugeot or Citroen vehicle.

VAN bus is pretty similar to CAN bus. It was used in many cars (Peugeot, Citroen) made by PSA.

In the beginning of 2000's the PSA group (Peugeot and Citroen) used VAN bus as a communication protocol
between the various comfort-related equipment. Later, around 2005, they started to replace this protocol
in their newer cars with the CAN bus protocol, however some models had VAN bus inside them until 2009.
[This overview](https://github.com/morcibacsi/PSAVanCanBridge#compatibility) lists vehicles that are
supposedly fitted with a VAN (comfort) bus.

Only ESP8266 / ESP8285 is supported. ESP32 is NOT supported by this library; for boards with those MCUs there is this
excellent library: [ESP32 RMT peripheral Vehicle Area Network (VAN bus) reader].

## 🔌 Schematics<a name = "schematics"></a>

You can usually find the VAN bus on pins 2 and 3 of ISO block "A" of your head unit (car radio). See 
https://en.wikipedia.org/wiki/Connectors_for_car_audio and https://github.com/morcibacsi/esp32_rmt_van_rx#schematics .

There are various possibilities to hook up a ESP8266 based board to your vehicle's VAN bus:

1. Use a [MCP2551] transceiver, connected with its CANH and CANL pins to the vehicle's VAN bus.
   As the MCP2551 has 5V logic, a 5V ↔ 3.3V [level converter] is needed to connect the CRX / RXD / R pin of the
   transceiver, via the level converter, to a GPIO pin of your ESP8266 board. For transmitting packets, also connect
   the CTX / TXD / D pins of the transceiver, via the level converter, to a GPIO pin of your ESP8266 board.

   A board with the MCP2551 transceiver can be ordered e.g.
   [here](https://webshop.domoticx.nl/can-bus-transceiver-module-5v-mcp2551) or
   [here](https://nl.aliexpress.com/item/1005004475976642.html).

![schema](extras/Schematics/Schematic%20using%20MCP2551_bb.png)

> 👉 Notes:
>  * CANH of the transceiver is connected to VAN BAR (DATA B), CANL to VAN (DATA). This may seem illogical
     but in practice it turns out this works best.
>  * The clamping circuit (D1, D2, R1) seems to (somehow) help in reducing the amount of bit errors
     (packet CRC errors).

2. Use a [SN65HVD230] transceiver, connected with its CANH and CANL pins to the vehicle's VAN bus.
   The SN65HVD230 transceiver already has 3.3V logic, so it is possible to directly connect the CRX / RXD / R pin of
   the transceiver to a GPIO pin of your ESP8266 board. For transmitting packets, also connect the CTX / TXD / D pins
   of the transceiver to a GPIO pin of your ESP8266 board.

   A board with the SN65HVD230 transceiver can be ordered e.g.
   [here](https://webshop.domoticx.nl/index.php?route=product/product&product_id=3935).

![schema](extras/Schematics/Schematic%20using%20SN65HVD230_bb.png)
   
> 👉 Notes:
>  * CANH of the transceiver is connected to VAN BAR (DATA B), CANL to VAN (DATA). This may seem illogical
     but in practice it turns out this works best.
>  * The clamping circuit (D1, D2, R1) seems to (somehow) help in reducing the amount of bit errors
     (packet CRC errors).

3. The simplest schematic is not to use a transceiver at all, but connect the VAN DATA line to GrouND using
   two 4.7 kOhm resistors. Connect the GPIO pin of your ESP8266 board to the 1:2 [voltage divider] that is thus
   formed by the two resistors. This is only for receiving packets, not for transmitting. Results may vary.

![schema](extras/Schematics/Schematic%20using%20voltage%20divider_bb.png)
   
> 👉 Note: I used this schematic during many long debugging hours, but I cannot guarantee that it won't ultimately
     cause your car to explode! (or anything less catastrofic)

## 🚀 Usage<a name = "usage"></a>

### General<a name = "general"></a>

Add the following line to your ```.ino``` sketch:
```
#include <VanBus.h>
```

For receiving and transmitting packets:

1. Add the following lines to your initialisation block ```void setup()```:
```
int TX_PIN = D3; // GPIO pin connected to VAN bus transceiver input
int RX_PIN = D2; // GPIO pin connected to VAN bus transceiver output
TVanBus::Setup(RX_PIN, TX_PIN);
```

2. Add e.g. the following lines to your main loop ```void loop()```:
```
TVanPacketRxDesc pkt;
if (VanBus.Receive(pkt)) pkt.DumpRaw(Serial);

uint8_t rmtTemperatureBytes[] = {0x0F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x70};
VanBus.SyncSendPacket(0x8A4, 0x08, rmtTemperatureBytes, sizeof(rmtTemperatureBytes));
```

If you only want to receive packets, you may save a few hundred precious bytes by using directly the ```VanBusRx```
object:

1. Add the following lines to your initialisation block ```void setup()```:
```
int RX_PIN = D2; // GPIO pin connected to VAN bus transceiver output
VanBusRx.Setup(RX_PIN);
```

2. Add the following lines to your main loop ```void loop()```:
```
TVanPacketRxDesc pkt;
if (VanBusRx.Receive(pkt)) pkt.DumpRaw(Serial);
```

### ```VanBus``` object

The following methods are available for the ```VanBus``` object:<a name = "functions"></a>

Interfaces for both receiving and transmitting of packets:

1. [```void Setup(uint8_t rxPin, uint8_t txPin)```](#Setup)
2. [```void DumpStats(Stream& s, bool longForm = true)```](#DumpStats)

Interfaces for receiving packets:

3. [```bool Available()```](#Available)
4. [```bool Receive(TVanPacketRxDesc& pkt, bool* isQueueOverrun = NULL)```](#Receive)
5. [```uint32_t GetRxCount()```](#GetRxCount)
6. [```int QueueSize()```](#QueueSize)
7. [```int GetNQueued()```](#GetNQueued)
8. [```int GetMaxQueued()```](#GetMaxQueued)

Interfaces for transmitting packets:

9. [```bool SyncSendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10)```](#SyncSendPacket)
10. [```bool SendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10)```](#SendPacket)
11. [```uint32_t GetTxCount()```](#GetTxCount)

---

#### 1. ```void Setup(uint8_t rxPin, uint8_t txPin)``` <a name="Setup"></a>

Start the receiver listening on GPIO pin ```rxPin```. The transmitter will transmit on GPIO pin ```txPin```.

#### 2. ```void DumpStats(Stream& s, bool longForm = true)``` <a name = "DumpStats"></a>

Dumps a few packet statistics on the passed stream. Passing **false** to the `longForm` parameter generates
the short form.

#### 3. ```bool Available()``` <a name = "Available"></a>

Returns ```true``` if a VAN packet is available in the receive queue.

#### 4. ```bool Receive(TVanPacketRxDesc& pkt, bool* isQueueOverrun = NULL)``` <a name = "Receive"></a>

Copy a VAN packet out of the receive queue, if available. Otherwise, returns ```false```.
If a valid pointer is passed to 'isQueueOverrun', will report then clear any queue overrun condition.

#### 5. ```uint32_t GetRxCount()``` <a name = "GetRxCount"></a>

Returns the number of received VAN packets since power-on. Counter may roll over.

#### 6. ```int QueueSize()``` <a name = "QueueSize"></a>

Returns the number of VAN packets that can be queued before packets are lost.

#### 7. ```int GetNQueued()``` <a name = "GetNQueued"></a>

Returns the number of VAN packets currently queued.

#### 8. ```int GetMaxQueued()``` <a name = "GetMaxQueued"></a>

Returns the highest number of VAN packets that were queued.

#### 9. ```bool SyncSendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10)``` <a name = "SyncSendPacket"></a>

Sends a packet for transmission. Returns ```true``` if the packet was successfully transmitted.

#### 10. ```bool SendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10)``` <a name = "SendPacket"></a>

Queues a packet for transmission. Returns ```true``` if the packet was successfully queued.

#### 11. ```uint32_t GetTxCount()``` <a name = "GetTxCount"></a>

Returns the number of VAN packets, offered for transmitting, since power-on. Counter may roll over.

### VAN packets

On a VAN bus, the electrical signals are the same as CAN. However, CAN and VAN use different data encodings on the
line which makes it impossible to use CAN interfaces on a VAN bus.

For background reading:
- [Wiki page on VAN](https://en.wikipedia.org/wiki/Vehicle_Area_Network)
- [VAN line protocol - Graham Auld - November 27, 2011](http://graham.auld.me.uk/projects/vanbus/lineprotocol.html)
- [Collection of data sheets - Graham Auld](http://graham.auld.me.uk/projects/vanbus/datasheets/)
- [Lecture on Enhanced Manchester Coding (in French) - Alain Chautar - June 30, 2003](http://www.educauto.org/files/file_fields/2013/11/18/mux1.pdf)
- [Lecture on VAN bus access: collisions and arbitration (in French) - Alain Chautar - October 10, 2003](http://www.educauto.org/sites/www.educauto.org/files/file_fields/2013/11/18/mux2.pdf)
- [Lecture on frame format (in French) - Alain Chautar - January 22, 2004](http://www.educauto.org/files/file_fields/2013/11/18/mux3.pdf)
- [Industrial networks CAN / VAN - Master Course - Marie-Agnès Peraldi-Frati - January 2008](http://www.i3s.unice.fr/~map/Cours/MASTER_STIC_SE/COURS32007.pdf)
- [De CAN à CANopen en passant par VAN (in French) - Jean Mercklé - March 2006](http://ebajic.free.fr/Ecole%20Printemps%20Reseau%20Mars%202006/Supports/J%20MERCKLE%20CANopen.pdf)
- [Les réseaux VAN - CAN (in French) - Guerrin Guillaume, Guers Jérôme, Guinchard Sébastien - February 2005](http://igm.univ-mlv.fr/~duris/NTREZO/20042005/Guerrin-Guers-Guinchard-VAN-CAN-rapport.pdf)
- [Le bus VAN, vehicle area network: Fondements du protocole (French) Paperback – June 4, 1997](https://www.amazon.com/bus-VAN-vehicle-area-network/dp/2100031600)
- [Vehicle Area Network (VAN bus) Analyzer for Saleae USB logic analyzer - Peter Pinter](https://github.com/morcibacsi/VanAnalyzer/)
- [Atmel TSS463C VAN Data Link Controller with Serial Interface](http://ww1.microchip.com/downloads/en/DeviceDoc/doc7601.pdf)
- [Multiplexed BSI Operating Principle for the Xsara Picasso And Xsara - The VAN protocol](https://web.archive.org/web/20221115101711/http://milajda22.sweb.cz/Manual_k_ridici_jednotce.pdf#page=17)

The following methods are available for ```TVanPacketRxDesc``` packet objects as obtained from
```VanBusRx.Receive(...)```:

1. [```uint16_t Iden()```](#Iden)
2. [```uint8_t CommandFlags()```](#CommandFlags)
3. [```const uint8_t* Data()```](#Data)
4. [```int DataLen()```](#DataLen)
5. [```unsigned long Millis()```](#Millis)
6. [```uint16_t Crc()```](#Crc)
7. [```bool CheckCrc()```](#CheckCrc)
8. [```bool CheckCrcAndRepair()```](#CheckCrcAndRepair)
9. [```void DumpRaw(Stream& s, char last = '\n')```](#DumpRaw)
10. [```const char* CommandFlagsStr()```](#CommandFlagsStr)
11. [```const char* AckStr()```](#AckStr)
12. [```const char* ResultStr()```](#ResultStr)
13. [```const TIfsDebugPacket& getIfsDebugPacket()```](#getIfsDebugPacket)
14. [```const TIsrDebugPacket& getIsrDebugPacket()```](#getIsrDebugPacket)

---

#### 1. ```uint16_t Iden()``` <a name = "Iden"></a>

Returns the IDEN field of the VAN packet.

An overview of known IDEN values can be found e.g. at:

- http://pinterpeti.hu/psavanbus/PSA-VAN.html
- http://graham.auld.me.uk/projects/vanbus/protocol.html

#### 2. ```uint8_t CommandFlags()``` <a name = "CommandFlags"></a>

Returns the 4-bit "command" FLAGS field of the VAN packet. Each VAN packet has 4 "command" flags:
- EXT (bit 3, MSB) : always 1
- RAK (bit 2): 1 = Requesting AcKnowledge
- R/W (bit 1): 1 = Read operation, 0 = Write operation
- RTR (bit 0, LSB): 1 = Remote Transmit Request

A thorough explanation is found (in French) on page 6 and 7 of
http://www.educauto.org/files/file_fields/2013/11/18/mux3.pdf#page=6 .

#### 3. ```const uint8_t* Data()``` <a name = "Data"></a>

Returns the data field (bytes) of the VAN packet.

#### 4. ```int DataLen()``` <a name = "DataLen"></a>

Returns the number of data bytes in the VAN packet. There can be at most 28 data bytes in a VAN packet.

#### 5. ```unsigned long Millis()``` <a name = "Millis"></a>

Packet time stamp in milliseconds.

#### 6. ```uint16_t Crc()``` <a name = "Crc"></a>

Returns the 15-bit CRC value of the VAN packet.

#### 7. ```bool CheckCrc()``` <a name = "CheckCrc"></a>

Checks the CRC value of the VAN packet.

#### 8. ```bool CheckCrcAndRepair()``` <a name = "CheckCrcAndRepair"></a>

Checks the CRC value of the VAN packet. If not, tries to repair it by flipping each bit. Returns ```true``` if the
packet is OK (either before or after the repair).

#### 9. ```void DumpRaw(Stream& s, char last = '\n')``` <a name = "DumpRaw"></a>

Dumps the raw packet bytes to a stream. Optionally specify the last character; default is '\n' (newline).

Example of invocation:

    pkt.DumpRaw(Serial);

Example of output:

    Raw: #0002 ( 2/15) 11(16) 0E 4D4 RA0 82-0C-01-00-11-00-3F-3F-3F-3F-82:7B-A4 ACK OK 7BA4 CRC_OK

Example of dumping into a char array:

    const char* PacketRawToStr(TVanPacketRxDesc& pkt)
    {
        static char dumpBuffer[MAX_DUMP_RAW_SIZE];
        
        GString str(dumpBuffer);
        PrintAdapter streamer(str);
        pkt.DumpRaw(streamer, '\0');
        
        return dumpBuffer;
    }

Note: for this, you will need to install the [PrintEx](https://github.com/Chris--A/PrintEx) library. I tested with
version 1.2.0 .

#### 10. ```const char* CommandFlagsStr()``` <a name = "CommandFlagsStr"></a>

Returns the "command" FLAGS field of the VAN packet as a string

Note: uses a statically allocated buffer, so don't call this method twice within the same printf invocation.

#### 11. ```const char* AckStr()``` <a name = "AckStr"></a>

Returns the ACK field of the VAN packet as a string, either "ACK" or "NO_ACK".

#### 12. ```const char* ResultStr()``` <a name = "ResultStr"></a>

Returns the RESULT field of the VAN packet as a string, either "OK" or a string starting with "ERROR_".

#### 13. ```const TIfsDebugPacket& getIfsDebugPacket()``` <a name = "getIfsDebugPacket"></a>

Retrieves a debug structure that can be used to analyse inter-frame space events.

Only available when ```#define VAN_RX_ISR_DEBUGGING``` is uncommented (see
[```VanBusRx.h```](https://github.com/0xCAFEDECAF/VanBus/blob/756b05097e57c183f87b7879e431308daef5ce5f/VanBusRx.h#L32)).

#### 14. ```const TIsrDebugPacket& getIsrDebugPacket()``` <a name = "getIsrDebugPacket"></a>

Retrieves a debug structure that can be used to analyse (observed) bit timings.

Only available when ```#define VAN_RX_IFS_DEBUGGING``` is uncommented (see
[```VanBusRx.h```](https://github.com/0xCAFEDECAF/VanBus/blob/756b05097e57c183f87b7879e431308daef5ce5f/VanBusRx.h#L33)).

## 👷 Work to be Done<a name = "todo"></a>

### Future

Currently the library supports only 125 kbit/s VAN bus. Need to add support for different rate, like 62.5 kbit/s,
which can be passed as an optional parameter to ```VanBusRx.Setup(...)```.

## 📖 License<a name = "license"></a>

This library is open-source and licensed under the [MIT license](http://opensource.org/licenses/MIT).

Do whatever you like with it, but contributions are appreciated!

## See also

- [VAN Live Connect](https://github.com/0xCAFEDECAF/VanLiveConnect) - Live data from your PSA vehicle (Peugeot,
  Citroën) on your smartphone or tablet, directly from the VAN bus.

["VAN" bus]: https://en.wikipedia.org/wiki/Vehicle_Area_Network
[MCP2551]: http://ww1.microchip.com/downloads/en/devicedoc/21667d.pdf
[level converter]: https://www.tinytronics.nl/shop/en/dc-dc-converters/level-converters/i2c-uart-bi-directional-logic-level-converter-5v-3.3v-2-channel-with-supply
[SN65HVD230]: https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf?ts=1592992149874
[voltage divider]: https://www.quora.com/How-many-pins-on-Arduino-Uno-give-a3-3v-pin-output
[ESP32 RMT peripheral Vehicle Area Network (VAN bus) reader]: https://github.com/morcibacsi/esp32_rmt_van_rx
