/*
 * VanBus packet transmitter for ESP8266 and ESP32
 *
 * Written by Erik Tromp
 *
 * MIT license, all text above must be included in any redistribution.
 */

#include "VanBus.h"

#ifdef ARDUINO_ARCH_ESP32

  #define TX_TIMER_FREQ (5000000)  // In Hz, must be multiple of 1000000
  #define TX_TIMER_DIVIDER (TIMER_BASE_CLK / TX_TIMER_FREQ)
  #define TX_TIMER_TICKS_PER_MICROSECOND (TX_TIMER_FREQ / 1000000)

  // Normally this value should be 8 * 5 to have a 1-bit time of 8 microseconds.
  // However, it seems that results may be better when adding a few tenths of a microsecond.
  #define VAN_TX_BIT_TIMER_TICKS (8 * TX_TIMER_TICKS_PER_MICROSECOND + 3)

  #define VAN_TX_BIT_TIMER_TICKS_TO_CPU_CYCLES \
    CPU_CYCLES(VAN_TX_BIT_TIMER_TICKS * TX_TIMER_DIVIDER)

  hw_timer_t* txTimer = NULL;
  hw_timer_t* rtrTimer = NULL;

#elif defined(CH32V006)
  #define IRAM_ATTR //the CH32V00x mcus execute ISR from flash directly
  
  //CH32V006 TX frames timing
  #define TX_TIMER_FREQ (TIMER_TICKS_PER_MICROSECOND * 1000000)  // In Hz, must be multiple of 1000000
  #define TX_TIMER_DIVIDER (TIMER_BASE_CLK / TX_TIMER_FREQ) // 48/6 = 8
  #define TX_TIMER_TICKS_PER_MICROSECOND (TX_TIMER_FREQ / 1000000) // = TIMER_TICKS_PER_MICROSECOND
  #define VAN_TX_BIT_TIMER_TICKS (8 * TIMER_TICKS_PER_MICROSECOND) // 1 bit is 8 microseconds
  #define VAN_TX_BIT_TIMER_TICKS_TO_CPU_CYCLES \
      CPU_CYCLES(VAN_TX_BIT_TIMER_TICKS * TX_TIMER_DIVIDER)

  HardwareTimer txTimer(TIM2);

#else // ! ARDUINO_ARCH_ESP32

  #define VAN_TX_BIT_TIMER_TICKS (8 * TIMER_TICKS_PER_MICROSECOND)

  #define VAN_TX_BIT_TIMER_TICKS_TO_CPU_CYCLES \
    CPU_CYCLES(VAN_TX_BIT_TIMER_TICKS * (TIMER_BASE_CLK / 1000000 / TIMER_TICKS_PER_MICROSECOND))

#endif // ARDUINO_ARCH_ESP32

uint8_t globalTxPin = VAN_NO_PIN_ASSIGNED;
//CH32V006 gpio optimisation 
#ifdef CH32V006
  static GPIO_TypeDef* globalTxPort = nullptr;
  static uint32_t      globalTxMask = 0;

  static GPIO_TypeDef* globalRxPort = nullptr;
  static uint32_t      globalRxMask = 0;
#endif

// Single timer operations for ESP8266 (timer0 used for WIFI) and CH32V006 mcu
// this idea is at the core of the ESP8266TimerInterrupt library (https://github.com/khoih-prog/ESP8266TimerInterrupt/blob/master/src/ESP8266_ISR_Timer-Impl.h)
extern void setTimerRouter(uint8_t);
extern void SendBitIsr();
extern void SendBitRtrIsr();
volatile uint8_t timerRouterDirection = 0; //off by default
void IRAM_ATTR timerRouter()
{
    // 1 -> TX isr
    // 2 -> RTR isr
    switch(timerRouterDirection){

      case 0:
        #ifndef CH32V006 // speed optimisation for CH32V006
        setTimerRouter(0); 
        #endif
        break;

      case 1:
        SendBitIsr();
        break;
      
      case 2:
        SendBitRtrIsr();
        break;
    }
} // timerRouter

void setTimerRouter(uint8_t directionSelect){

    static bool isEnabled = false;

      if(directionSelect == 0){ //deactivate the timer
        timerRouterDirection = 0;

        #ifdef ARDUINO_ARCH_ESP32
          timerStop(txTimer);
          #if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
          timerAlarmDisable(txTimer);
          #endif // ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
        #elif defined(CH32V006)
          txTimer.pause();
        #else // ! ARDUINO_ARCH_ESP32
          timer1_disable();
        #endif // ARDUINO_ARCH_ESP32

        isEnabled = false;

        return;
      }
      timerRouterDirection = directionSelect; 

      if(!isEnabled){ //activate the timer

        #ifdef ARDUINO_ARCH_ESP32
          #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
            timerAlarm(txTimer, VAN_TX_BIT_TIMER_TICKS, true, 0);
          #else // ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
            // Set a repetitive timer
            timerAlarmWrite(txTimer, VAN_TX_BIT_TIMER_TICKS, true);
            timerAlarmEnable(txTimer);
          #endif // ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
            timerStart(txTimer);
        #elif defined(CH32V006)
          txTimer.resume();
        #else // ! ARDUINO_ARCH_ESP32
          if (! timer1_enabled())
          {
              // Set a repetitive timer
              timer1_disable();
              timer1_attachInterrupt(timerRouter); //lors du setup ?

              // Clock to timer (prescaler) is always 80 MHz, even if F_CPU is 160 MHz
              timer1_enable(TIMER_DIVIDER, TIM_EDGE, TIM_LOOP);

              timer1_write(VAN_TX_BIT_TIMER_TICKS);
          } // if
        #endif // ARDUINO_ARCH_ESP32
      }

      isEnabled = true;

} // setTimerRouter

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

      #if defined CONFIG_IDF_TARGET_ESP32C3 || defined CONFIG_IDF_TARGET_ESP32C6 || defined CONFIG_IDF_TARGET_ESP32H2 //only 2 timers
        setTimerRouter(0); //C3
      #elif defined(ARDUINO_ARCH_ESP32)
        timerStop(txTimer);
        #if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
          timerAlarmDisable(txTimer);
        #endif // ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
      #elif defined(CH32V006)
        setTimerRouter(0);
      #else // ! ARDUINO_ARCH_ESP32
        setTimerRouter(0);
      #endif // ARDUINO_ARCH_ESP32
     
    } // if 
    
    VanBusRx.SetLastMediaAccessAt(GetCpuCycleCount()); // It was me! :-)
    
    // Start listening again at other devices on the bus
    #if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
        attachInterrupt(digitalPinToInterrupt(VanBusRx.pin), RxPinChangeIsr, CHANGE);
    #elif defined(CH32V006)
        attachInterrupt(VanBusRx.pin, GPIO_Mode_IPU, &RxPinChangeIsr, EXTI_Mode_Interrupt, EXTI_Trigger_Rising_Falling);
    #endif
} // FinishPacketTransmission

// Finish RTR transmission
void IRAM_ATTR FinishRtrTransmission(TVanPacketRtrDesc* rtrDesc, bool resetRx = true){ //TODO if RTR bit is 0
      
    rtrDesc->state = VAN_TX_DONE;
    if(rtrDesc->stateRtrAck != VAN_PASSIVE_ACK){
      rtrDesc->stateRtrAck = VAN_NO_ACK;
    }

    #if defined CONFIG_IDF_TARGET_ESP32C3 || defined CONFIG_IDF_TARGET_ESP32C6 || defined CONFIG_IDF_TARGET_ESP32H2 //only 2 timers
      setTimerRouter(0); //C3
    #elif defined(ARDUINO_ARCH_ESP32)
      timerStop(rtrTimer);
      #if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
      timerAlarmDisable(rtrTimer);
      #endif // ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    #elif defined(CH32V006)
      setTimerRouter(0);
    #else // ! ARDUINO_ARCH_ESP32
      setTimerRouter(0);
    #endif // ARDUINO_ARCH_ESP32

    VanBusRx.SetLastMediaAccessAt(GetCpuCycleCount()); // now it's me! :-)

    //reset the current packet reception slot
    VanBusRx._head->Init(); //forward declaration of class TVanPacketRtrDesc in VanBusRX.h
    // Start listening again to incoming frames
    #if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
        attachInterrupt(digitalPinToInterrupt(VanBusRx.pin), RxPinChangeIsr, CHANGE);
    #elif defined(CH32V006)
        attachInterrupt(digitalPinToInterrupt(VanBusRx.pin), GPIO_Mode_IPD, &RxPinChangeIsr, EXTI_Mode_Interrupt, EXTI_Trigger_Rising_Falling);
    #endif
} // FinishRtrTransmission

// Indicates to the user if the sent RTR frame received an acknowledgement 
bool TVanPacketRtrDesc::rtrAckReceived(){
  return stateRtrAck == VAN_PASSIVE_ACK;
} // rtrAckReceived

// Send one bit of the RTR frame on the VAN bus
void IRAM_ATTR SendBitRtrIsr()
{
  uint32_t curr = GetCpuCycleCount();  // Store CPU cycle counter value as soon as possible
  static uint32_t CmdRtrBitOffsetCycles = 0;
  static unsigned int atBit = 9;
  static uint16_t* p_stuffedByte;

  //if (VanBusRtr.state == VAN_TX_DONE) return;

  //delay to read the RTR bit of the CMD byte
  if (VanBusRtr.state == VAN_TX_WAITING)
  {
    if(CmdRtrBitOffsetCycles==0){
      CmdRtrBitOffsetCycles = (2  * VAN_TX_BIT_TIMER_TICKS_TO_CPU_CYCLES + 0) + curr; // TODO - adjust delay for ESP8266 reliability ?
      return;
    }else if(curr < CmdRtrBitOffsetCycles){
      return;
    }
    
    if(digitalRead(VanBusRx.pin)==VAN_BIT_DOMINANT){ // TODO - test this condition with the not RTR address and a pushed button frame
      FinishRtrTransmission(&VanBusRtr, false);
      Serial.println("abort RTR");
      // TODO - reset RTR if RTR bit is 0 ?
      return;
    }

    atBit = 11; //9 + RTR bit + Manchester bit
    VanBusRtr.state = VAN_TX_SENDING;
    p_stuffedByte = VanBusRtr.stuffedBytes;
    
    return; // Transmission begin in the next time slot
  }

  static int lastSetLevel = VAN_BIT_RECESSIVE;
  //if (p_stuffedByte < txDesc->p_eod){} // TODO - collision detection during RTR transmission
  uint16_t byte = *p_stuffedByte;
  uint16_t bit = byte & (1 << atBit); // TODO - use static bitMask variable: bitmask <<= 1;
  
  // Write to GPIO pin
  if (bit != 0)
  {
    #ifdef ARDUINO_ARCH_ESP32
      REG_WRITE(GPIO_OUT_W1TS_REG, 1 << globalTxPin);
    #elif defined(CH32V006)
      globalTxPort->BSHR = globalTxMask;
    #else // ! ARDUINO_ARCH_ESP32
      GPOS = (1 << globalTxPin);
    #endif // ARDUINO_ARCH_ESP32
      lastSetLevel = VAN_BIT_RECESSIVE;
    
    if(p_stuffedByte == VanBusRtr.p_eod && atBit < 9){
      if(digitalRead(VanBusRx.pin) != VAN_BIT_RECESSIVE){
        VanBusRtr.stateRtrAck = VAN_PASSIVE_ACK;
      }
    }
  }
  else
  {
    #ifdef ARDUINO_ARCH_ESP32
      REG_WRITE(GPIO_OUT_W1TC_REG, 1 << globalTxPin);
    #elif defined(CH32V006)
      globalTxPort->BCR = globalTxMask;
    #else // ! ARDUINO_ARCH_ESP32
      GPOC = (1 << globalTxPin);
    #endif // ARDUINO_ARCH_ESP32
      lastSetLevel = VAN_BIT_DOMINANT;
  } // if

  // Advance to next bit
  if (atBit-- == 0)
  {
    // Advance to next byte
    atBit = 9;
    
    // Finished sending packet?
    if (++p_stuffedByte == VanBusRtr.p_last){
      CmdRtrBitOffsetCycles = 0;
      FinishRtrTransmission(&VanBusRtr);
    }
  } // if
} // SendBitRtrIsr

// Send one bit on the VAN bus
void IRAM_ATTR SendBitIsr()
{
    uint32_t curr = GetCpuCycleCount();  // Store CPU cycle counter value as soon as possible
    static unsigned int atBit = 9;
    static uint16_t* p_stuffedByte;
    TVanPacketTxDesc* txDesc = VanBusTx._tail;

    //if (txDesc->state == VAN_TX_DONE) return;

    if (txDesc->state == VAN_TX_WAITING)
    {
        // Wait at least 5 (EOF) + 7 (IFS) bits after last media access.
        // See also Figure 30 of http://ww1.microchip.com/downloads/en/DeviceDoc/doc4205.pdf#page=47 .
        //
        uint32_t nCycles = curr - VanBusRx.GetLastMediaAccessAt();  // Arithmetic has safe roll-over
        if (nCycles < (5 /* EOF */ + 7 /* IFS */ + 1 /* safety */ ) * VAN_TX_BIT_TIMER_TICKS_TO_CPU_CYCLES)
        {
            txDesc->busOccupied = true;
            return;
        } // if

        // Don't waste precious CPU time handling the RX pin interrupts of my own transmission.
        // TODO - this will cause any colliding incoming packet to be not received by the receiver.
        detachInterrupt(digitalPinToInterrupt(VanBusRx.pin)); // TODO - no reception with CH32V006, comment this line

        txDesc->interFrameCpuCycles = nCycles;
        txDesc->state = VAN_TX_SENDING;
        atBit = 9;
        p_stuffedByte = txDesc->stuffedBytes;

        // We've already wasted much time here, so let the actual transmission begin in the next time slot
        return;
    } // if

    static int lastSetLevel = VAN_BIT_RECESSIVE;

    // Detect collision and bit errors until (but not including) the EOD. Otherwise we will see an ACK bit from the
    // receiver as a collision.
    if (p_stuffedByte < txDesc->p_eod)
    {
      // Check if previously transmitted bit has been copied by reading RX pin
      
      #ifdef ARDUINO_ARCH_ESP32
      int pinLevel = digitalRead(VanBusRx.pin);
      #elif defined(CH32V006)
      int pinLevel = (globalRxPort->INDR & globalRxMask) ? HIGH : LOW; //digitalRead() timing is not concistent across all of the pins
      #else // ! ARDUINO_ARCH_ESP32
      int pinLevel = GPIP(VanBusRx.pin);
      #endif // ARDUINO_ARCH_ESP32

      if (pinLevel == lastSetLevel) txDesc->bitOk = true;

      else if (pinLevel == VAN_BIT_DOMINANT && lastSetLevel == VAN_BIT_RECESSIVE)
      {
          int atByte = p_stuffedByte - txDesc->stuffedBytes;
          if (txDesc->nCollisions == 0) txDesc->firstCollisionAtBit = atByte * 10 + (9 - atBit);
          txDesc->nCollisions++;

          // Backout and start all over again
          txDesc->state = VAN_TX_WAITING;

          return;
      } // if

      else if (pinLevel == VAN_BIT_RECESSIVE && lastSetLevel == VAN_BIT_DOMINANT) txDesc->bitError = true;

    } // if

    uint16_t byte = *p_stuffedByte;
    uint16_t bit = byte & (1 << atBit); // TODO - use static bitMask variable: bitmask <<= 1;

    // Write to GPIO pin
    if (bit != 0)
    {
      #ifdef ARDUINO_ARCH_ESP32
        REG_WRITE(GPIO_OUT_W1TS_REG, 1 << VanBusTx.txPin);
      #elif defined(CH32V006)
        globalTxPort->BSHR = globalTxMask;
      #else // ! ARDUINO_ARCH_ESP32
        GPOS = (1 << VanBusTx.txPin);
      #endif // ARDUINO_ARCH_ESP32
        lastSetLevel = VAN_BIT_RECESSIVE;
    }
    else
    {
      #ifdef ARDUINO_ARCH_ESP32
        REG_WRITE(GPIO_OUT_W1TC_REG, 1 << VanBusTx.txPin);
      #elif defined(CH32V006)
        globalTxPort->BCR = globalTxMask;
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

// Initializes the VAN packet transmitter
void TVanPacketTxQueue::Setup(uint8_t theRxPin, uint8_t theTxPin)
{
    txPin = theTxPin;
    globalTxPin = txPin;
    #ifdef CH32V006
      globalTxPort = digitalPinToPort(txPin);
      globalTxMask = digitalPinToBitMask(txPin);

      globalRxPort = digitalPinToPort(theRxPin);
      globalRxMask = digitalPinToBitMask(theRxPin);
    #endif

    pinMode(theTxPin, OUTPUT);
    digitalWrite(theTxPin, VAN_BIT_RECESSIVE);  // Set bus state to 'recessive' (CANH and CANL: not driven)

  #if defined CONFIG_IDF_TARGET_ESP32C3 || defined CONFIG_IDF_TARGET_ESP32C6 || defined CONFIG_IDF_TARGET_ESP32H2 //only 2 timers
    txTimer = timerBegin(TX_TIMER_FREQ);
    timerAttachInterrupt(txTimer, &timerRouter); //C3
  #elif defined(ARDUINO_ARCH_ESP32)
   #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    txTimer = timerBegin(TX_TIMER_FREQ);
    timerAttachInterrupt(txTimer, &SendBitIsr);

    rtrTimer = timerBegin(TX_TIMER_FREQ);
    timerAttachInterrupt(rtrTimer, &SendBitRtrIsr);
   #else
    txTimer = timerBegin(0, TX_TIMER_DIVIDER, true);
    timerAlarmDisable(txTimer);
    timerDetachInterrupt(txTimer);
    timerAttachInterrupt(txTimer, &SendBitIsr, true);

    rtrTimer = timerBegin(1, TX_TIMER_DIVIDER, true);
    timerAlarmDisable(rtrTimer);
    timerDetachInterrupt(rtrTimer);
    timerAttachInterrupt(rtrTimer, &SendBitRtrIsr, true);
   #endif
  #elif defined(CH32V006)
      txTimer.setPrescaleFactor(TIMER_DIVIDER);
      txTimer.setOverflow(VAN_TX_BIT_TIMER_TICKS, TICK_FORMAT);
      txTimer.setMode(1, TIMER_OUTPUT_COMPARE, NC); // not connected to a pin because timer
      txTimer.attachInterrupt(1, &timerRouter);
  #else // ! ARDUINO_ARCH_ESP32

    if (! timer1_enabled())
    {
        // Set a repetitive timer
        timer1_disable();
        timer1_attachInterrupt(timerRouter);

        // Clock to timer (prescaler) is always 80 MHz, even if F_CPU is 160 MHz
        timer1_enable(TIMER_DIVIDER, TIM_EDGE, TIM_LOOP);

        timer1_write(VAN_TX_BIT_TIMER_TICKS);
    } // if

  #endif // ARDUINO_ARCH_ESP32

    VanBusRx.Setup(theRxPin);
    VanBusRx.RegisterTxTimerTicks(VAN_TX_BIT_TIMER_TICKS);
} // TVanPacketTxQueue::Setup

void TVanPacketRtrDesc::PrepareRtrPacket(uint16_t incomingIden, const uint8_t* data, size_t dataLen){
    Init();

    VanBusRtr.incomingIden = incomingIden;

    // Send at most VAN_MAX_DATA_BYTES data
    if (dataLen > VAN_MAX_DATA_BYTES) dataLen = VAN_MAX_DATA_BYTES;
  
    // Prepare full packet data
    uint8_t bytes[VAN_MAX_PACKET_SIZE];
    bytes[0] = 0x0E;  // SOF
    bytes[1] = incomingIden >> 4 & 0xFF;  // IDEN (MSB 8 bits) //TODO optimize byte usage
    bytes[2] = incomingIden << 4 | 0x08 | (0b1110);  // IDEN (LSB 4 bits), fixed-1 (1 bit), COM (3 bits)
    memcpy(bytes + 3, data, dataLen);
    uint8_t* p_data = &bytes[3]; // OFFSET
    uint16_t crc = _crc(bytes, dataLen + 5);
    bytes[dataLen + 3] = crc >> 8;
    bytes[dataLen + 4] = crc & 0xFF;
    dataLen -= 3; // OFFSET

    // Stuff with Manchester bits
    for (size_t i = 0; i < dataLen + 5; i++)
    {
      uint8_t byte = *(p_data++);
      stuffedBytes[i] = (byte & 0xF0) << 2 | (~ byte & 0x10) << 1 | (byte & 0x0F) << 1 | (~ byte & 0x01);
    } // for

    stuffedBytes[0] |= 0b010000000000; // RTR bit followed by Manchester bit (atBit initialized at 11 in the ISR)

    // EOD
    stuffedBytes[dataLen + 4] &= 0xFFFC;
    eodAt = dataLen + 5;
    p_eod = stuffedBytes + dataLen + 5;

    // EOF
    stuffedBytes[dataLen + 5] = 0xFFFF;
    size = dataLen + 5 + 1;  // Adding 1 for the last 10 VAN_LOGICAL_HIGH-bits
    p_last = stuffedBytes + dataLen + 5 + 1;
    stuffedBytes[size] = 0xFFFF;

} //TVanPacketRtrDesc::PrepareRtrPacket

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
    stuffedBytes[size] = 0xFFFF; // intermittent bug correction, TX line was sometime set to 0 at the end of EOF

    state = VAN_TX_WAITING;
} // TVanPacketTxDesc::PreparePacket

void TVanPacketRtrDesc::Dump() const
{
  // Only for transmitted packets
  if (state != VAN_TX_DONE) return;

  // TODO - print the iden, cmd bits, sent data and acknowledge
  Serial.println("rtr dump");
}

// Print information about a transmitted package
void TVanPacketTxDesc::Dump() const
{
    // Only for transmitted packets
    if (state != VAN_TX_DONE) return;

    // Only if there is something interesting to print
    if (! busOccupied && bitOk && nCollisions == 0 && ! bitError) return;

    uint32_t ifsBits = interFrameCpuCycles / VAN_TX_BIT_TIMER_TICKS_TO_CPU_CYCLES;
    Serial.printf_P(PSTR("#%" PRIu32 ", ifsBits=%" PRIu32 "%s"), n, ifsBits, busOccupied ? ", busOccupied" : "");

    if (nCollisions > 0)
    {
        Serial.printf_P(PSTR(", nCollisions=%" PRIu32 ", firstCollisionAtBit=%" PRIu32), nCollisions, firstCollisionAtBit);
    } // if

    Serial.printf_P(PSTR("%s%s\n"), bitOk ? "" : ", NO bitOk", bitError ? ", bitError" : "");
} // TVanPacketTxDesc::Dump

void TVanPacketRtrDesc::StartRtrBitSendTimer()
{

    #if defined CONFIG_IDF_TARGET_ESP32C3 || defined CONFIG_IDF_TARGET_ESP32C6 || defined CONFIG_IDF_TARGET_ESP32H2 //only 2 timers
      setTimerRouter(2); //C3
    #elif defined(ARDUINO_ARCH_ESP32)
      #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
        timerAlarm(rtrTimer, VAN_TX_BIT_TIMER_TICKS, true, 0);
        timerStart(rtrTimer);
      #else // ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
      // TODO - prepare the timer ahead of time, only trigger here : 
        // Set a repetitive timer
        //timerAlarmWrite(rtrTimer, VAN_TX_BIT_TIMER_TICKS, true);
        //timerAlarmEnable(rtrTimer);
        timerStart(rtrTimer);
      #endif // ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)

    #elif defined(CH32V006)
      #warning "CH32V006 mcu RTR not implemented yet"
      setTimerRouter(2);
    #else // ! ARDUINO_ARCH_ESP32
      setTimerRouter(2);
    #endif // ARDUINO_ARCH_ESP32

    detachInterrupt(digitalPinToInterrupt(VanBusRx.pin));
} // StartRtrBitSendTimer

void TVanPacketTxQueue::StartBitSendTimer()
{
    VanBusRx.RegisterTxIsr(&SendBitIsr);

    // TODO - wait here until:
    // nCycles >= (8 /* EOF */ + 5 /* IFS */) * (VAN_TX_BIT_TIMER_TICKS * 16) * CPU_F_FACTOR
    // If we start the SendBitIsr now, we might introduce extra wobbling in the RxPinChangeIsr, causing CRC errors
    // Preference is to not have the timer1 interrupt handler being called while a packet is being received.

    //uint32_t curr = GetCpuCycleCount();
    //uint32_t nCycles = curr - VanBusRx.GetLastMediaAccessAt();  // Arithmetic has safe roll-over
    //if (nCycles < (8 /* EOF */ + 5 /* IFS */) * (VAN_TX_BIT_TIMER_TICKS * 16) * CPU_F_FACTOR) return;

    NO_INTERRUPTS;

    // Transmitting a packet is done completely by interrupt-servicing

  #if defined CONFIG_IDF_TARGET_ESP32C3 || defined CONFIG_IDF_TARGET_ESP32C6 || defined CONFIG_IDF_TARGET_ESP32H2 //only 2 timers
    detachInterrupt(digitalPinToInterrupt(VanBusRx.pin));
    setTimerRouter(1);//C3
  #elif defined(ARDUINO_ARCH_ESP32)
   #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    timerAlarm(txTimer, VAN_TX_BIT_TIMER_TICKS, true, 0);
    timerStart(txTimer);
   #else // ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    // Set a repetitive timer
    timerAlarmWrite(txTimer, VAN_TX_BIT_TIMER_TICKS, true);
    timerAlarmEnable(txTimer);
    timerStart(txTimer);
   #endif // ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)

  #elif defined(CH32V006)
    // Don't waste precious CPU time handling the RX pin interrupts of my own transmission.
    // TODO - this will cause any colliding incoming packet to be not received by the receiver.
    detachInterrupt(digitalPinToInterrupt(VanBusRx.pin));
    setTimerRouter(1);
  #else // ! ARDUINO_ARCH_ESP32
    setTimerRouter(1);
  #endif // ARDUINO_ARCH_ESP32

    INTERRUPTS;
} // void TVanPacketTxQueue::StartBitSendTimer

// Wait until the head of the queue is available. When 'timeOutMs' is set to 0, will wait forever.
bool TVanPacketTxQueue::WaitForHeadAvailable(unsigned int timeOutMs)
{
    unsigned int waitPoll = timeOutMs;

    // Relying on short-circuit boolean evaluation
    while (! SlotAvailable() && (timeOutMs == 0 || --waitPoll > 0)) delay(1);

    return SlotAvailable();
} // TVanPacketTxQueue::WaitForHeadAvailable

// Synchronous packet send: returns as soon as the packet was transmitted.
// Will wait at most 'timeOutMs' milliseconds. When 'timeOutMs' is set to 0, will wait forever
bool TVanPacketTxQueue::SyncSendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs)
{
    // do not perform calculations while receiving or transmitting another packet
    unsigned int timeoutStop = millis()+timeOutMs; //no rollover protection !!
    while(!VanBusTx.isIdle() /*|| !VanBusRx.isIdle()*/ || !VanBusRtr.isIdle()){  // TODO - VanBusRx.isIdle() is HS on ESP8266 !!
      if (millis() > timeoutStop){Serial.println("exit");return 0;}
      delay(1);
    }

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

    //_head->Dump();

    AdvanceHead();

    return true;
} // TVanPacketTxQueue::SyncSendPacket

// Asynchronous packet send: queues the packet to be transmitted then returns.
// If the TX queue is full, will wait at most 'timeOutMs' milliseconds. When 'timeOutMs' is set to 0, will wait forever.
bool TVanPacketTxQueue::SendPacket(uint16_t iden, uint8_t cmdFlags, const uint8_t* data, size_t dataLen, unsigned int timeOutMs)
{
    // do not perform calculations while receiving or transmitting another packet
    unsigned int timeoutStop = millis()+timeOutMs; //no rollover protection !!
    while(!VanBusTx.isIdle() || !VanBusRx.isIdle() || !VanBusRtr.isIdle()){ 
      if (millis() > timeoutStop){Serial.println("exit");return 0;}
      delay(1);
    }

    // If the Tx queue is full, wait a bit
    if (! WaitForHeadAvailable(timeOutMs))
    {
        ++nDropped;
        return false;
    } // if

    _head->PreparePacket(iden, cmdFlags, data, dataLen);
    StartBitSendTimer();

    //_head->Dump();

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

bool TVanPacketRtrDesc::rtrPacket(uint16_t incomingIden, const uint8_t* data, size_t dataLen, unsigned int timeOutMs){
  timeoutStop = millis()+timeOutMs; //no rollover protection !!
  /*
  while(!VanBusTx.isIdle() || !VanBusRx.isIdle() || !VanBusRtr.isIdle()){ // do not perform calculations while receiving or transmitting another packet
    digitalWrite(debug_eventA, 1);
    if (millis() > timeoutStop){Serial.println("exit");return 0;}
    delay(1);
  }
      digitalWrite(debug_eventA, 0);*/
  PrepareRtrPacket(incomingIden, data, dataLen);
  state = VAN_TX_WAITING;
  return true;
}

TVanPacketTxQueue VanBusTx;
TVanPacketRtrDesc VanBusRtr;