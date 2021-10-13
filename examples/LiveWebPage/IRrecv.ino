
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

#define IR_RECV_PIN D5  // IR receiver data pin is connected to pin D5

// Using D7 as VCC and D6 as ground pin for the IR receiver. Should be possible with e.g. the
// TSOP4838 IR receiver as it typically uses only 0.7 mA.
#define IR_VCC D7
#define IR_GND D6

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
    unsigned int rawbuf[RAWBUF];  // raw data
    uint8_t rawlen;  // counter of entries in rawbuf
} TIrParams;

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
        irparams.rcvstate = STATE_MARK;	
        irparams.rawbuf[irparams.rawlen++] = 20;		
    }
    else
    {
        uint32_t ticks = (now - lastIrPulse) / USECPERTICK + 1;

        // Timeout after
        #define TIMEOUT_USECS (15000)
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

    if (decodeHash(results)) return IR_DECODED;

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
long IRrecv::decodeHash(TIrPacket *results)
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

    const char* buttonStr =
        pkt.value == 0x01A0DA1B ? PSTR("MENU_BUTTON") :
        pkt.value == 0x8E8C855C ? PSTR("MODE_BUTTON") :
        pkt.value == 0x816E43D7 ? PSTR("ESC_BUTTON") :
        pkt.value == 0x02B619B3 ? PSTR("DOWN_BUTTON") :
        pkt.value == 0x77678D53 ? PSTR("LEFT_BUTTON") :
        pkt.value == 0xF79A2397 ? PSTR("RIGHT_BUTTON") :
        pkt.value == 0xF59A2071 ? PSTR("UP_BUTTON") :
        pkt.value == 0xF98D3EE1 ? PSTR("VAL_BUTTON") :  // "Enter" button
        PSTR("");

    if (strlen_P(buttonStr) == 0) return "";

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"mfd_remote_control\": \"%S\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(jsonBuffer, IR_JSON_BUFFER_SIZE, jsonFormatter, buttonStr);

    // JSON buffer overflow?
    if (at >= IR_JSON_BUFFER_SIZE) return "";

    #ifdef PRINT_JSON_BUFFERS_ON_SERIAL

    Serial.print(F("Parsed to JSON object:\n"));
    PrintJsonText(jsonBuffer);

    #endif // PRINT_JSON_BUFFERS_ON_SERIAL

    return jsonBuffer;
} // ParseIrPacketToJson

IRrecv irrecv(IR_RECV_PIN);

void IrSetup()
{
    Serial.println(F("Setting up IR receiver"));

    // Using GPIO pins to feed the IR receiver. Should be possible with e.g. the TSOP4838 IR receiver as
    // it typically uses only 0.7 mA.
    pinMode(IR_VCC, OUTPUT); 
    digitalWrite(IR_VCC, HIGH);
    pinMode(IR_GND, OUTPUT); 
    digitalWrite(IR_GND, LOW);

    irrecv.enableIRIn(); // Start the receiver
} // IrSetup

bool IrReceive(TIrPacket& irPacket)
{
    if (! irrecv.decode(&irPacket)) return false;

#if 0
    Serial.print(F("IR receiver: "));
    Serial.println(irPacket.value, HEX);
#endif

    irrecv.resume(); // Receive the next value

    // Ignore same code within 150 ms
    static unsigned long lastValue = 0;
    static unsigned long lastUpdate = 0;

    // Arithmetic has safe roll-over
    if (irPacket.value == lastValue && millis() - lastUpdate < 150UL) return false;

    lastUpdate = millis();
    lastValue = irPacket.value;

    return true;
} // IrReceive
