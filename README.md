<h3 align="center">PSA (Peugeot, Citroën) VAN bus reader/writer for Arduino-ESP8266 and ESP32</h3>

<p align="center">Reading and writing packets from/to PSA vehicles' VAN bus.</p>

---

[![Release Version](https://img.shields.io/github/release/0xCAFEDECAF/VanBus.svg?style=plastic)](https://github.com/0xCAFEDECAF/VanBus/releases/latest/)
[![Release Date](https://img.shields.io/github/release-date/0xCAFEDECAF/VanBus.svg?style=plastic)](https://github.com/0xCAFEDECAF/VanBus/releases/latest/)

[![arduino-library-badge](https://www.ardu-badge.com/badge/VanBus.svg)](https://www.ardu-badge.com/VanBus)

[![Platform ESP8266](https://img.shields.io/badge/Platform-Espressif8266-yellow)](https://github.com/esp8266/Arduino)
[![Platform ESP32](https://img.shields.io/badge/Platform-Espressif32-orange)](https://github.com/espressif/arduino-esp32)

[![Framework](https://img.shields.io/badge/Framework-Arduino-blue)](https://www.arduino.cc/)

[![Compile Library and Build Example Sketches](https://github.com/0xCAFEDECAF/VanBus/actions/workflows/compile.yml/badge.svg)](https://github.com/0xCAFEDECAF/VanBus/actions/workflows/compile.yml)

## 📝 Table of Contents
- [Description](#description)
- [Schematics](#schematics)
- [Building your Project](#building)
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

Both the ESP8266 / ESP8285 and ESP32 platforms are supported by this library.

## 🔌 Schematics<a name = "schematics"></a>

You can usually find the VAN bus on pins 2 and 3 of ISO block "A" of your head unit (car radio). See
https://en.wikipedia.org/wiki/Connectors_for_car_audio and https://github.com/morcibacsi/esp32_rmt_van_rx#schematics .

There are various possibilities to hook up a ESP8266/ESP32 based board to your vehicle's VAN bus:

1. Use a [MCP2551] transceiver, connected with its CANH and CANL pins to the vehicle's VAN bus.
   As the MCP2551 has 5V logic, a 5V ↔ 3.3V [level converter] is needed to connect the CRX / RXD / R pin of the
   transceiver, via the level converter, to a GPIO pin of your ESP8266/ESP32 board. For transmitting packets,
   also connect the CTX / TXD / D pins of the transceiver, via the level converter, to a GPIO pin of your
   ESP8266/ESP32 board.

   A board with the MCP2551 transceiver can be ordered e.g.
   [here](https://domoticx.net/webshop/can-bus-transceiver-module-5v-mcp2551/) or
   [here](https://nl.aliexpress.com/item/1005004475976642.html).

   Example schema using a [Wemos D1 mini] (ESP8266 based):

![schema](extras/Schematics/Schematic%20using%20MCP2551_bb.png)

   Example schema using a [LilyGO TTGO T7 Mini32] (ESP32 based):

![schema](extras/Schematics/Schematic%20with%20ESP32%20using%20MCP2551_bb.png)

> 👉 Notes:
>  * <img src="extras/Schematics/MCP2551%20terminator%20resistors.jpg" align="right" width="200px"/>The two terminator
     resistors R3 and R4 (2 x 100 Ohm, near the CANH and CANL pins) on this transceiver board
     are meant for operating inside a CAN bus network, but are not necessary on a VAN bus. In fact, they may even
     cause the other equipment on the bus to malfunction. If you experience problems in the vehicle equipment,
     you may want to remove (unsolder) these terminator resistors.
     See also [this issue](https://github.com/0xCAFEDECAF/VanBus/issues/9).
>  * CANH of the transceiver is connected to VAN BAR (DATA B), CANL to VAN (DATA). This may seem illogical
     but in practice it turns out this works best.
>  * The clamping circuit (D1, D2, R1) seems to (somehow) help in reducing the amount of bit errors
     (packet CRC errors).

2. Use a [SN65HVD230] transceiver, connected with its CANH and CANL pins to the vehicle's VAN bus.
   The SN65HVD230 transceiver already has 3.3V logic, so it is possible to directly connect the CRX / RXD / R pin of
   the transceiver to a GPIO pin of your ESP8266/ESP32 board. For transmitting packets, also connect the
   CTX / TXD / D pins of the transceiver to a GPIO pin of your ESP8266/ESP32 board.

   A board with the SN65HVD230 transceiver can be ordered e.g.
   [here](https://domoticx.net/webshop/can-bus-transceiver-module-3-3v-sn65hvd230-vp230/) or
   [here](https://eu.robotshop.com/products/waveshare-can-board-sn65hvd230).

![schema](extras/Schematics/Schematic%20using%20SN65HVD230_bb.png)

> 👉 Notes:
>  * <img src="extras/Schematics/SN65HVD230%20terminator%20resistor.jpg" align="right" width="200px"/>The terminator
     resistor R2 (120 Ohm, near the CANH and CANL pins) on this transceiver board is meant
     for operating inside a CAN bus network, but is not necessary on a VAN bus. In fact, it may even cause the
     other equipment on the bus to malfunction. If you experience problems in the vehicle equipment, you may
     want to remove (unsolder) the R2 terminator resistor.
     See also [this issue](https://github.com/0xCAFEDECAF/VanBus/issues/9).
>  * CANH of the transceiver is connected to VAN BAR (DATA B), CANL to VAN (DATA). This may seem illogical
     but in practice it turns out this works best.
>  * The clamping circuit (D1, D2, R1) seems to (somehow) help in reducing the amount of bit errors
     (packet CRC errors).

3. The simplest schematic is not to use a transceiver at all, but connect the VAN DATA line to GrouND using
   two 4.7 kOhm resistors. Connect the GPIO pin of your ESP8266/ESP32 board to the 1:2 [voltage divider] that is thus
   formed by the two resistors. This is only for receiving packets, not for transmitting. Results may vary.

![schema](extras/Schematics/Schematic%20using%20voltage%20divider_bb.png)

> 👉 Notes:
>  * This schematic seems to work well only with an ESP8266-based board, like the [Wemos D1 mini]. With an
     ESP32-based board I get a lot of CRC errors.
>  * I used this schematic during many long debugging hours, but I cannot guarantee that it won't ultimately
     cause your car to explode! (or anything less catastrophic)

## 🚀 Building your Project<a name = "building"></a>

Before proceeding with this project, make sure you check all the following prerequisites.

### Install Arduino IDE

See [Arduino IDE](https://www.arduino.cc/en/software). I am currently using
[version 1.8.19](https://downloads.arduino.cc/arduino-1.8.19-windows.exe) but other versions
may also be working fine.

### ESP8266-based board

An example of an ESP8266-based board is the [Wemos D1 mini].

#### 1. Install Board Package in Arduino IDE

For an ESP8266-based board you will need to install the
[ESP8266 Board Package](https://arduino-esp8266.readthedocs.io/en/stable/installing.html).

I am currently using [version 3.1.2](https://github.com/esp8266/Arduino/releases/tag/3.1.2) but other versions
seem to be also working fine (I tested with versions 2.6.3 ... 3.1.2).

Follow [this tutorial](https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/) to install
the ESP8266 Board Package.

#### 2. Install the VAN Bus Library<a id="install_library"></a>

In the Arduino IDE, go to the "Sketch" menu → "Include Library" → "Manage Libraries..." and install the
[Vehicle Area Network (VAN) bus packet reader/writer](https://github.com/0xCAFEDECAF/VanBus) library. Hint:
type "vanbus" in the search box.

For more explanation on using the Arduino library manager, you can browse to:
* this [tutorial from Arduino](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries), and
* this [explanation from Adafruit](https://learn.adafruit.com/adafruit-all-about-arduino-libraries-install-use/library-manager)

#### 3. Board settings

In the Arduino IDE, go to the "Tools" menu, and choose:

* CPU frequency: 160 MHz

Here is a picture of board settings that have been tested to work:

![Board settings](extras/Arduino%20IDE/Board%20settings%20ESP8266.png)

### ESP32-based board

An example of an ESP32-based board is the [LilyGO TTGO T7 Mini32].

#### 1. Install Board Package in Arduino IDE

For an ESP32-based board you will need the ESP32 board package installed. 

I am currently using [version 1.0.6](https://github.com/espressif/arduino-esp32/releases/tag/1.0.6) but other versions
may also be working fine.

Follow [this tutorial](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)
to install the ESP32 Board Package. Alternatively, turn to
[this page](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) for instructions.

#### 2. Install the VAN Bus Library

See section ['Install the VAN Bus Library', above](#install_library).

#### 3. Board settings

In the Arduino IDE, go to the "Tools" menu, and choose:

* CPU frequency: 240 MHz

Here is a picture of board settings that have been tested to work:

![Board settings](extras/Arduino%20IDE/Board%20settings%20ESP32.png)

## 🧰 Usage<a name = "usage"></a>

### General<a name = "general"></a>

Add the following line to your ```.ino``` sketch:
```cpp
#include <VanBus.h>
```

For receiving and transmitting packets:

1. Add the following lines to your initialisation block ```void setup()```:
```cpp
int TX_PIN = D3; // GPIO pin connected to VAN bus transceiver input
int RX_PIN = D2; // GPIO pin connected to VAN bus transceiver output
TVanBus::Setup(RX_PIN, TX_PIN);
```

2. Add e.g. the following lines to your main loop ```void loop()```:
```cpp
TVanPacketRxDesc pkt;
if (VanBus.Receive(pkt)) pkt.DumpRaw(Serial);

uint8_t rmtTemperatureBytes[] = {0x0F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x70};
VanBus.SyncSendPacket(0x8A4, 0x08, rmtTemperatureBytes, sizeof(rmtTemperatureBytes));
```

If you only want to receive packets, you may save a few hundred precious bytes by using directly the ```VanBusRx```
object:

1. Add the following lines to your initialisation block ```void setup()```:
```cpp
int RX_PIN = D2; // GPIO pin connected to VAN bus transceiver output
VanBusRx.Setup(RX_PIN);
```

2. Add the following lines to your main loop ```void loop()```:
```cpp
TVanPacketRxDesc pkt;
if (VanBusRx.Receive(pkt)) pkt.DumpRaw(Serial);
```

### ```VanBus``` object

The following methods are available for the ```VanBus``` object:<a name = "functions"></a>

Interfaces for both receiving and transmitting of packets:

1. [```void Setup(uint8_t rxPin, uint8_t txPin)```](#setup)
2. [```void DumpStats(Stream& s, bool longForm = true)```](#dumpstats)

Interfaces for receiving packets:

3. [```bool Available()```](#available)
4. [```bool Receive(TVanPacketRxDesc& pkt, bool* isQueueOverrun = NULL)```](#receive)
5. [```uint32_t GetRxCount()```](#getrxcount)
6. [```int QueueSize()```](#queuesize)
7. [```int GetNQueued()```](#getnqueued)
8. [```int GetMaxQueued()```](#getmaxqueued)
9. [```void SetDropPolicy(int startAt, bool (*isEssential)(const TVanPacketRxDesc&) = 0)```](#setdroppolicy)

Interfaces for transmitting packets:

10. [```bool SyncSendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10)```](#syncsendpacket)
11. [```bool SendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10)```](#sendpacket)
12. [```uint32_t GetTxCount()```](#gettxcount)

---

#### 1. ```void Setup(uint8_t rxPin, uint8_t txPin)``` <a id="setup"></a>

Start the receiver listening on GPIO pin ```rxPin```. The transmitter will transmit on GPIO pin ```txPin```.

#### 2. ```void DumpStats(Stream& s, bool longForm = true)``` <a id="dumpstats"></a>

Dumps a few packet statistics on the passed stream. Passing **false** to the `longForm` parameter generates
the short form.

#### 3. ```bool Available()``` <a id="available"></a>

Returns ```true``` if a VAN packet is available in the receive queue.

#### 4. ```bool Receive(TVanPacketRxDesc& pkt, bool* isQueueOverrun = NULL)``` <a id="receive"></a>

Copy a VAN packet out of the receive queue, if available. Otherwise, returns ```false```.
If a valid pointer is passed to 'isQueueOverrun', will report then clear any queue overrun condition.

#### 5. ```uint32_t GetRxCount()``` <a id="getrxcount"></a>

Returns the number of received VAN packets since power-on. Counter may roll over.

#### 6. ```int QueueSize()``` <a id="queuesize"></a>

Returns the number of VAN packets that can be queued before packets are lost.

#### 7. ```int GetNQueued()``` <a id="getnqueued"></a>

Returns the number of VAN packets currently queued.

#### 8. ```int GetMaxQueued()``` <a id="getmaxqueued"></a>

Returns the highest number of VAN packets that were queued.

#### 9. ```void SetDropPolicy(int startAt, bool (*isEssential)(const TVanPacketRxDesc&) = 0)``` <a id="setdroppolicy"></a>

Implements a simple packet drop policy for if the receive queue is starting to fill up.

Packets are dropped if the receive queue has ```startAt``` packets or more queued, unless a packet is identified
as "important". To identify such packets, pass a function pointer via the ```isEssential``` parameter. The passed
function is called within interrupt context, so it *must* have the ```ICACHE_RAM_ATTR``` attribute.

Example:

```cpp
bool ICACHE_RAM_ATTR IsImportantPacket(const TVanPacketRxDesc& pkt)
{
    return
        pkt.DataLen() >= 3 &&
        (
            pkt.Iden() == CAR_STATUS1_IDEN  // Right-hand stalk button press
            || (pkt.Iden() == DEVICE_REPORT && pkt.Data()[0] == 0x8A)  // head_unit_report, head_unit_button_pressed
        );
} // IsImportantPacket

#define RX_PIN D2
#define VAN_PACKET_QUEUE_SIZE 60
VanBusRx.Setup(RX_PIN, VAN_PACKET_QUEUE_SIZE);
VanBusRx.SetDropPolicy(VAN_PACKET_QUEUE_SIZE * 8 / 10, &IsImportantPacket);
```

The above example will drop incoming packets if the receive queue contains 48 or more packets, unless they
are recognized by ```IsImportantPacket```.

#### 10. ```bool SyncSendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10)``` <a id="syncsendpacket"></a>

Sends a packet for transmission. Returns ```true``` if the packet was successfully transmitted.

#### 11. ```bool SendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs = 10)``` <a id="sendpacket"></a>

Queues a packet for transmission. Returns ```true``` if the packet was successfully queued.

#### 12. ```uint32_t GetTxCount()``` <a id="gettxcount"></a>

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

1. [```uint16_t Iden()```](#iden)
2. [```uint8_t CommandFlags()```](#commandflags)
3. [```const uint8_t* Data()```](#data)
4. [```int DataLen()```](#datalen)
5. [```unsigned long Millis()```](#millis)
6. [```uint16_t Crc()```](#crc)
7. [```bool CheckCrc()```](#checkcrc)
8. [```bool CheckCrcAndRepair()```](#checkcrcandrepair)
9. [```void DumpRaw(Stream& s, char last = '\n')```](#dumpraw)
10. [```const char* CommandFlagsStr()```](#commandflagsstr)
11. [```const char* AckStr()```](#ackstr)
12. [```const char* ResultStr()```](#resultstr)
13. [```const TIfsDebugPacket& getIfsDebugPacket()```](#getifsdebugpacket)
14. [```const TIsrDebugPacket& getIsrDebugPacket()```](#getisrdebugpacket)

---

#### 1. ```uint16_t Iden()``` <a id="iden"></a>

Returns the IDEN field of the VAN packet.

An overview of known IDEN values can be found e.g. at:

- http://pinterpeti.hu/psavanbus/PSA-VAN.html
- http://graham.auld.me.uk/projects/vanbus/protocol.html

#### 2. ```uint8_t CommandFlags()``` <a id="commandflags"></a>

Returns the 4-bit "command" FLAGS field of the VAN packet. Each VAN packet has 4 "command" flags:
- EXT (bit 3, MSB) : always 1
- RAK (bit 2): 1 = Requesting AcKnowledge
- R/W (bit 1): 1 = Read operation, 0 = Write operation
- RTR (bit 0, LSB): 1 = Remote Transmit Request

A thorough explanation is found (in French) on page 6 and 7 of
http://www.educauto.org/files/file_fields/2013/11/18/mux3.pdf#page=6 .

#### 3. ```const uint8_t* Data()``` <a id="data"></a>

Returns the data field (bytes) of the VAN packet.

#### 4. ```int DataLen()``` <a id="datalen"></a>

Returns the number of data bytes in the VAN packet. There can be at most 28 data bytes in a VAN packet.

#### 5. ```unsigned long Millis()``` <a id="millis"></a>

Packet time stamp in milliseconds.

#### 6. ```uint16_t Crc()``` <a id="crc"></a>

Returns the 15-bit CRC value of the VAN packet.

#### 7. ```bool CheckCrc()``` <a id="checkcrc"></a>

Checks the CRC value of the VAN packet.

#### 8. ```bool CheckCrcAndRepair()``` <a id="checkcrcandrepair"></a>

Checks the CRC value of the VAN packet. If not, tries to repair it by flipping each bit. Returns ```true``` if the
packet is OK (either before or after the repair).

#### 9. ```void DumpRaw(Stream& s, char last = '\n')``` <a id="dumpraw"></a>

Dumps the raw packet bytes to a stream. Optionally specify the last character; default is '\n' (newline).

Example of invocation:

    pkt.DumpRaw(Serial);

Example of output:

    Raw: #0002 ( 2/15) 11(16) 0E 4D4 RA0 82-0C-01-00-11-00-3F-3F-3F-3F-82:7B-A4 ACK OK 7BA4 CRC_OK

Example of dumping into a char array:

```cpp
const char* PacketRawToStr(TVanPacketRxDesc& pkt)
{
    static char dumpBuffer[MAX_DUMP_RAW_SIZE];

    GString str(dumpBuffer);
    PrintAdapter streamer(str);
    pkt.DumpRaw(streamer, '\0');

    return dumpBuffer;
}
```

Note: for this, you will need to install the [PrintEx](https://github.com/Chris--A/PrintEx) library. I tested with
version 1.2.0 .

#### 10. ```const char* CommandFlagsStr()``` <a id="commandflagsstr"></a>

Returns the "command" FLAGS field of the VAN packet as a string

Note: uses a statically allocated buffer, so don't call this method twice within the same printf invocation.

#### 11. ```const char* AckStr()``` <a id="ackstr"></a>

Returns the ACK field of the VAN packet as a string, either "ACK" or "NO_ACK".

#### 12. ```const char* ResultStr()``` <a id="resultstr"></a>

Returns the RESULT field of the VAN packet as a string, either "OK" or a string starting with "ERROR_".

#### 13. ```const TIfsDebugPacket& getIfsDebugPacket()``` <a id="getifsdebugpacket"></a>

Retrieves a debug structure that can be used to analyse inter-frame space events.

Only available when ```#define VAN_RX_ISR_DEBUGGING``` is uncommented (see
[```VanBusRx.h```](https://github.com/0xCAFEDECAF/VanBus/blob/756b05097e57c183f87b7879e431308daef5ce5f/VanBusRx.h#L32)).

#### 14. ```const TIsrDebugPacket& getIsrDebugPacket()``` <a id="getisrdebugpacket"></a>

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
- [ESP32 RMT peripheral Vehicle Area Network (VAN bus) reader]

["VAN" bus]: https://en.wikipedia.org/wiki/Vehicle_Area_Network
[MCP2551]: http://ww1.microchip.com/downloads/en/devicedoc/21667d.pdf
[Wemos D1 mini]: https://www.tinytronics.nl/en/development-boards/microcontroller-boards/with-wi-fi/wemos-d1-mini-v4-esp8266-ch340
[LilyGO TTGO T7 Mini32]: https://www.tinytronics.nl/en/development-boards/microcontroller-boards/with-wi-fi/lilygo-ttgo-t7-mini32-v1.3-esp32-4mb-flash
[level converter]: https://www.tinytronics.nl/shop/en/dc-dc-converters/level-converters/i2c-uart-bi-directional-logic-level-converter-5v-3.3v-2-channel-with-supply
[SN65HVD230]: https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf?ts=1592992149874
[voltage divider]: https://www.quora.com/How-many-pins-on-Arduino-Uno-give-a3-3v-pin-output
[ESP32 RMT peripheral Vehicle Area Network (VAN bus) reader]: https://github.com/morcibacsi/esp32_rmt_van_rx
