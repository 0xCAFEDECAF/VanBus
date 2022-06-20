
// IR receiver to follow along while browsing the menus
//
// Code adapted from 'IRremoteESP8266' libary (https://github.com/sebastienwarin/IRremoteESP8266.git).
//
// Modifications:
// - Removed all IR-send related stuff; kept only the IR-receiver part
// - No use of (precious) hardware timer
// - GPIO handling cleanup: no longer monopolizing resources
// - Safer handling of ISR-written data
// - Removed all decoding stuff except decoding as hash
//

#include "Config.h"

// Receiver states
#define STATE_IDLE 2
#define STATE_MARK 3
#define STATE_STOP 5

#define IR_ERR 0
#define IR_DECODED 1

// Some useful constants
#define USECPERTICK 50  // microseconds per clock interrupt tick
#define RAWBUF 100 // Length of raw duration buffer

// Information for the interrupt handler
typedef struct
{
    uint8_t recvpin;  // pin for IR data from detector
    uint8_t rcvstate;  // state machine
    unsigned long millis_;
    unsigned int rawbuf[RAWBUF];  // raw data
    uint8_t rawlen;  // counter of entries in rawbuf
} TIrParams;

// Defined in PacketToJson.ino
extern const char emptyStr[];
extern const char yesStr[];
extern const char noStr[];
void PrintJsonText(const char* jsonBuffer);

// Main class for receiving IR
class IRrecv
{
  public:
    IRrecv(int recvpin);
    int decode(TIrPacket* results);
    void enableIRIn();
    void disableIRIn();
    void resume();

  private:
    long decodeHash(TIrPacket* results);  // Called by decode
    int compare(unsigned int oldval, unsigned int newval);
}; // class IRrecv

volatile uint32_t lastIrPulse = 0;

volatile TIrParams irparams;

void ICACHE_RAM_ATTR irPinChangeIsr()
{
    if (irparams.rcvstate == STATE_STOP) return;

    uint32_t now = system_get_time();
    if (irparams.rcvstate == STATE_IDLE)
    {
        irparams.millis_ = millis();
        irparams.rcvstate = STATE_MARK;
        irparams.rawbuf[irparams.rawlen++] = 20;
    }
    else
    {
        uint32_t ticks = (now - lastIrPulse) / USECPERTICK + 1;

        // Timeout after
        #define TIMEOUT_USECS (10000)
        #define TIMEOUT_TICKS (TIMEOUT_USECS / USECPERTICK)

        if (ticks > TIMEOUT_TICKS) irparams.rcvstate = STATE_STOP;
        else if (irparams.rawlen < RAWBUF) irparams.rawbuf[irparams.rawlen++] = ticks;
    }
    lastIrPulse = now;
} // irPinChangeIsr

IRrecv::IRrecv(int recvpin)
{
    irparams.recvpin = recvpin;
} // IRrecv::IRrecv

// Initialization
void IRrecv::enableIRIn()
{
    // Initialize state machine variables
    noInterrupts();
    irparams.rcvstate = STATE_IDLE;
    irparams.rawlen = 0;
    interrupts();

    pinMode(irparams.recvpin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(irparams.recvpin), irPinChangeIsr, CHANGE);
} // IRrecv::enableIRIn

void IRrecv::disableIRIn()
{
    detachInterrupt(irparams.recvpin);
} // IRrecv::disableIRIn

void IRrecv::resume()
{
    noInterrupts();
    irparams.rcvstate = STATE_IDLE;
    irparams.rawlen = 0;
    interrupts();
} // IRrecv::resume

// IR controller button hash codes
enum IrButton_t
{
    IB_MENU = 0x01A0DA1B,
    IB_MODE = 0x8E8C855C,
    IB_ESC = 0x816E43D7,
    IB_DOWN = 0x02B619B3,
    IB_LEFT = 0x77678D53,
    IB_RIGHT = 0xF79A2397,
    IB_UP = 0xF59A2071,
    IB_VALIDATE = 0xF98D3EE1
}; // enum IrButton_t

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
PGM_P IrButtonStr(unsigned long data)
{
    return
        data == IB_MENU ? PSTR("MENU_BUTTON") :
        data == IB_MODE ? PSTR("MODE_BUTTON") :
        data == IB_ESC ? PSTR("ESC_BUTTON") :
        data == IB_DOWN ? PSTR("DOWN_BUTTON") :
        data == IB_LEFT ? PSTR("LEFT_BUTTON") :
        data == IB_RIGHT ? PSTR("RIGHT_BUTTON") :
        data == IB_UP ? PSTR("UP_BUTTON") :
        data == IB_VALIDATE ? PSTR("VAL_BUTTON") :
        emptyStr;
} // IrButtonStr

// Decodes the received IR message.
// Returns 0 if no data ready, 1 if data ready.
// Results of decoding are stored in results.
int IRrecv::decode(TIrPacket* results)
{
    noInterrupts();
    results->rawbuf = irparams.rawbuf;
    results->rawlen = irparams.rawlen;
    if (irparams.rawlen && irparams.rcvstate != STATE_STOP)
    {
        uint32_t now = system_get_time();
        uint32_t then = lastIrPulse;
        uint32_t ticks = (now - then) / USECPERTICK + 1;
        if (ticks > TIMEOUT_TICKS) irparams.rcvstate = STATE_STOP;
    } // if
    interrupts();

    if (irparams.rcvstate != STATE_STOP) return IR_ERR;

    if (decodeHash(results))
    {
        results->millis_ = irparams.millis_;
        results->buttonStr = IrButtonStr(results->value);
        if (strlen_P(results->buttonStr) > 0) return IR_DECODED;
    } // if

    // Throw away and start over
    resume();
    return IR_ERR;
} // IRrecv::decode

// Use FNV hash algorithm: http://isthe.com/chongo/tech/comp/fnv/#FNV-param
#define FNV_PRIME_32 16777619
#define FNV_BASIS_32 2166136261

// Converts the raw code values into a 32-bit hash code.
// Hopefully this code is unique for each button.
// This isn't a "real" decoding, just an arbitrary value.
long IRrecv::decodeHash(TIrPacket* results)
{
    // Require at least 6 samples to prevent triggering on noise
    if (results->rawlen < 6) return IR_ERR;

    long hash = FNV_BASIS_32;
    for (int i = 1; i+2 < results->rawlen; i++)
    {
        int value =  compare(results->rawbuf[i], results->rawbuf[i+2]);

        // Add value into the hash
        hash = (hash * FNV_PRIME_32) ^ value;
    } // for

    results->value = hash;
    results->bits = 32;
    return IR_DECODED;
} // IRrecv::decodeHash

// Compare two tick values, returning 0 if newval is shorter,
// 1 if newval is equal, and 2 if newval is longer.
// Use a tolerance of 20%
int IRrecv::compare(unsigned int oldval, unsigned int newval)
{
    if (newval < oldval * .8) return 0;
    else if (oldval < newval * .8) return 2;
    return 1;
} // IRrecv::compare

const char* ParseIrPacketToJson(const TIrPacket& pkt)
{
    #define IR_JSON_BUFFER_SIZE 256
    static char jsonBuffer[IR_JSON_BUFFER_SIZE];

    if (strlen_P(pkt.buttonStr) == 0) return "";

    PGM_P heldStr = pkt.held ? PSTR(" (held)") : emptyStr;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"mfd_remote_control\": \"%S%S\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(jsonBuffer, IR_JSON_BUFFER_SIZE, jsonFormatter, pkt.buttonStr, heldStr);

    // JSON buffer overflow?
    if (at >= IR_JSON_BUFFER_SIZE) return "";

  #ifdef PRINT_JSON_BUFFERS_ON_SERIAL
    Serial.print(F("Parsed to JSON object:\n"));
    PrintJsonText(jsonBuffer);
  #endif // PRINT_JSON_BUFFERS_ON_SERIAL

    return jsonBuffer;
} // ParseIrPacketToJson

IRrecv* irrecv;

void IrSetup()
{
    Serial.println(F("Setting up IR receiver"));

    // Using GPIO pins to feed the IR receiver. Should be possible with e.g. the TSOP4838 IR receiver as
    // it typically uses only 0.7 mA.
    pinMode(IR_VCC, OUTPUT);
    digitalWrite(IR_VCC, HIGH);
    pinMode(IR_GND, OUTPUT);
    digitalWrite(IR_GND, LOW);

    irrecv = new IRrecv(IR_RECV_PIN);
    irrecv->enableIRIn(); // Start the receiver
} // IrSetup

bool IrReceive(TIrPacket& irPacket)
{
    if (! irrecv->decode(&irPacket)) return false;

    irrecv->resume(); // Receive the next value

    // Code that detects "button held" condition
    //
    // The IR controller normally fires ~ 20 times per second (measured 16 firings in 805 milliseconds,
    // i.e. 805 / 16 = 50.3 milliseconds).
    //
    // Handling procedure:
    //
    // - If the same button is seen within IR_BUTTON_HELD_2_MS (101) milliseconds, set the "held" bit, otherwise clear.
    //
    // - Return an "IR packet" when first seen ("held" bit clear); as long as "held" bit is set, count down
    //   initially 13, then 5 packet intervals before returning the next "IR packet". This achieves an initial delay
    //   of ~ 0.65 seconds followed by a firing rate of ~ 4 times per second.
    //
    // Note: the original MFD has a pretty buggy IR receiver (or its handling). When using the IR in a 50 Hz lighted
    // environment, it misses many (or even all) IR packets.

    static unsigned long lastValue = 0;
    static unsigned long lastUpdate = 0;

    unsigned long now = irPacket.millis_;  // Retrieve packet reception time stamp from ISR
    unsigned long interval = now - lastUpdate;  // Arithmetic has safe roll-over

    // Firing interval or IR unit (milliseconds)
    #define IR_BUTTON_HELD_INTV_MS (50UL)

    static unsigned nFirings = 0;

    // Same IR decoded value seen within this time (milliseconds) is seen as "held" button
    #define IR_BUTTON_HELD_2_MS (101UL)

    irPacket.held = irPacket.value == lastValue && interval < IR_BUTTON_HELD_2_MS;

    lastValue = irPacket.value;
    lastUpdate = now;

  #ifdef DEBUG_IR_RECV
    Serial.printf_P
    (
        PSTR("[irRecv] val = 0x%lX (%S), intv = %lu, held = %S"),
        irPacket.value,
        irPacket.buttonStr,
        interval,
        irPacket.held ? yesStr : noStr
    );
  #endif // DEBUG_IR_RECV

    // "MENU_BUTTON" and "MODE_BUTTON" are never "held"; they fire only once.
    if (irPacket.held && (irPacket.value == IB_MENU || irPacket.value == IB_MODE))
    {
      #ifdef DEBUG_IR_RECV
        Serial.println();
      #endif // DEBUG_IR_RECV
        return false;
    } // if

    // IR controller button codes
    enum IrButtonHeldState_t
    {
        IBHS_DELAYING,
        IBHS_REPEATING
    };

    static IrButtonHeldState_t irButtonRepeatState = IBHS_DELAYING;

    // For as long as "held" bit is set:

    // - wait for 13 packet intervals (13 * 50 = 650 milliseconds), for a delay of ~ 0.65 seconds
    #define IR_DELAY_N_INTERVALS (13)

    // - by default, wait for 5 packet intervals (5 * 50 = 250 milliseconds), for a firing rate of ~ 4 times per second
    #define IR_REPEAT_N_INTERVALS (5)

    static int countDown = IR_DELAY_N_INTERVALS;

    if (! irPacket.held)
    {
        irButtonRepeatState = IBHS_DELAYING;
        countDown = IR_DELAY_N_INTERVALS;
        static bool firstTime = true;
        if (firstTime)
        {
            countDown += 2;
            firstTime = false;
        } // if

        nFirings = 1;

      #ifdef DEBUG_IR_RECV
        Serial.printf_P(PSTR(" --> FIRING (%u)\n"), nFirings);
      #endif // DEBUG_IR_RECV

        return true;
    } // if

    // irPacket.held == true

  #ifdef DEBUG_IR_RECV
    Serial.printf_P(PSTR(", countDown = %d"), countDown);
  #endif // DEBUG_IR_RECV

    // 50 = -1; 74 = -1; 75 = -2; 100 = -2; 124 = -2; 125 = -3; 150 = -3; 175 = -4; 200 = -4; ...
    countDown -= (interval + IR_BUTTON_HELD_INTV_MS / 2) / IR_BUTTON_HELD_INTV_MS;

  #ifdef DEBUG_IR_RECV
    Serial.printf_P(PSTR("-->%d"), countDown);
  #endif // DEBUG_IR_RECV

    if (irButtonRepeatState == IBHS_DELAYING)
    {
        if (countDown > 0)
        {
          #ifdef DEBUG_IR_RECV
            Serial.println();
          #endif // DEBUG_IR_RECV
            return false;
        } // if

        countDown += IR_REPEAT_N_INTERVALS - 1;
        nFirings = 2;
        irButtonRepeatState = IBHS_REPEATING;
    }
    else // irButtonRepeatState == IBHS_REPEATING
    {
        if (countDown > 0)
        {
          #ifdef DEBUG_IR_RECV
            Serial.println();
          #endif // DEBUG_IR_RECV
            return false;
        } // if

        countDown += IR_REPEAT_N_INTERVALS;

        nFirings++;

        // Subtle extra speed adjustments (found by trial and error)
        if ((nFirings + 1) % 10 == 0) countDown--;
    } // if

  #ifdef DEBUG_IR_RECV
    Serial.printf_P(PSTR(" --> FIRING (%u)\n"), nFirings);
  #endif // DEBUG_IR_RECV

    return true;
} // IrReceive
