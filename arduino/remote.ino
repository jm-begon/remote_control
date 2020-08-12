#include<stdint.h>
#include <IRLibAll.h>

/****************************************************************************
 *                              SERIAL BUFFER                               *
 ****************************************************************************/
#define BUFFER_SIZE 256
static uint8_t buffer[BUFFER_SIZE];


/****************************************************************************
 *                                 TIMEOUT                                  *
 ****************************************************************************/
typedef struct timer_t {
  size_t counter;
  size_t waittime;
  size_t limit; // Number of cycle to wait
} Timer;

static inline Timer newTimer(size_t waittime, size_t limit)
{
  Timer timer = {0, waittime, limit);
  return timer;
}

static inline bool hasTimedOut(Timer *timer)
{
  return timer->counter == timer->limit;
}

static bool timeout(Timer *timer)
{
  if(timer->counter++ == limit)
    return true;
  if(timer->waittime > 0)
    delay(timer->waittime)
  return false;
}

 

/****************************************************************************
 *                                  STATES                                  *
 ****************************************************************************/
void (*currentState)();
/*
 * STATES:
 * --> Ready
 * Ready --handshake--> Waiting
 * Waiting --communication--> Processing
 * Processing --done--> Success
 * Waiting --timeout--> Ready
 * Success --> Waiting
 * Processing --error--> Error
 * Error --> Ready
 */
#define READY 0
#define WAITING 1
#define PROCESSING 2
#define SUCCESS 3
#define ERROR_HANDSHAKE 4
// Not those ? Error 

static uint8_t state;

static void (*currentState)();
static void ready();
static void waiting();
static void success();
static void error();

/*
 * LEDS
 *   States    |  Green  | Yellow
 * ------------+---------+---------
 *  Ready      |   on    |   off
 *  Waiting    |   on    |   on
 *  Success    |   on    |  blink
 *  Error      |  blink  |  blink
 */
#define ledGreen 0
#define ledYellow 1

#define blinkPeriod 500
#define blinkCycles 4

enum ledState {ON, OFF, BLINK};

typedef struct leds_t {
  ledState green;
  ledState yellow;
} LEDs;

static void led(const LEDs *leds)
{
  Timer timer = newTimer(blinkPeriod, blinkCycles);
  
  bool blink = leds->green == BLINK || leds->yellow == BLINK;
  bool greenOn = leds->green != OFF;
  bool yellowOn = leds->green != OFF;

  do{
    digitalWrite(ledGreen, greenOn? HIGH:LOW);
    digitalWrite(ledYellow, greenOn? HIGH:LOW);
    if(leds->green == BLINK)
      greenOn = !greenOn;
    if(leds->yellow == BLINK)
      yellowOn = !yellowOn;
  } while(blink && !timeout(&timer);
  
}


/****************************************************************************
 *                              COMMUNICATION                               *
 ****************************************************************************/
#define COM_TIMEOUT_WAITTIME 200
#define COM_TIMEOUT_LIMIT 10

IRsend irSender;

typdef struct command_t {
  size_t protocol;
  uint32_t code;
  size_t length;
} Command;

/*-------------------------------------CHAR-------------------------------- */
typedef struct str_t {
  const char* word;
  size_t length;
} Str;

static const Str SYN = {"syn", 3};
static const Str SYNACK = {"synack", 6};
static const Str ACK = {"ack", 3};
static const Str OK = {"ok", 2};
static const Str SENT = {"sent", 4};
enum Availability {YES, NOTYET, ERR};

static Availability isAvailable(const Str *str)
{
  if(Serial.available() < str->length)
    return NOTYET;
  Serial.readBytes(buffer, str->length);
  
  for(size_t i=0; i<str->length; i++)
    if(str->words[i] != (char)buffer[i])
      return ERR;
      
  return YES;
}



/*-------------------------------------BYTE-------------------------------- */
typedef struct code_t {
  uint32_t code;
  uint8_t check;
} Code;


static Code readCode()
{
  Serial.readBytes(buffer, 4);
  Code code = {convert4x8to32(buffer), 
               buffer[0] + buffer[1] + buffer[2] + buffer[3]}
  return code;
}

  

static uint32_t convert4x8to32(uint8_t *buffer)
{
  // Big endian conversion
  uint32_t res = buffer[0] << 24;
  res |= (buffer[1] << 16);
  res |= (buffer[2] << 8);
  res |= buffer[3];
  
  return res;
}

static uint


/****************************************************************************
 *                                    MAIN                                  *
 ****************************************************************************/
void setup() {
  Serial.begin(9600);
  Serial.flush();
  state = READY;
}

void loop() {
  currentState();
  
}


/*-------------------------------------READY------------------------------- */
void ready()
{
  LEDs leds = {ON, OFF}
  led(&leds);

  Timer synTimer = newTimer(COM_TIMEOUT_WAITTIME, COM_TIMEOUT_LIMIT);
  Timer ackTimer = newTimer(COM_TIMEOUT_WAITTIME, COM_TIMEOUT_LIMIT);
  Availabilty avb;

  while((avb = isAvailable(SYN)) == NOTYET && !timeout(&synTimer));
  if(avb == ERR || hasTimedOut(&synTimer))
  {
    currentState = &error;
    return;
  }

  Serial.write(SYNACK->word);
  
  while((avb = isAvailable(ACK)) == NOTYET && !timeout(&ackTimer));
  if(avb == ERR || hasTimedOut(&ackTimer))
  {
    currentState = &error;
    return;
  }

  currentState = &waiting;
}




/*------------------------------------WAITING------------------------------ */
void waiting()
{

  LEDs leds = {ON, ON}
  led(&leds);

  Command command = {0, 0, 0};
  Timer waitTimer = newTimer(COM_TIMEOUT_WAITTIME, 600);
  Timer sizeTimer = newTimer(COM_TIMEOUT_WAITTIME, COM_TIMEOUT_LIMIT);
  Timer codeTimer = newTimer(COM_TIMEOUT_WAITTIME, COM_TIMEOUT_LIMIT);
  Timer okTimer = newTimer(COM_TIMEOUT_WAITTIME, COM_TIMEOUT_LIMIT);

  // Protocol: 1 byte
  while(Serial.available() < 0 && !timeout(&waitTimer));
  if(hasTimedOut(&waitTimer))
  {
    currentState = &ready;
    return;
  }
  command->protocol = Serial.read();
  Serial.write(command->protocol);

  // Size: 1 byte
  while(Serial.available() < 0 && !timeout(&sizeTimer));
  if(hasTimedOut(&sizeTimer))
  {
    currentState = &error;
    return;
  }
  command->length = Serial.read();
  Serial.write(command->length);


  // Code: 4 bytes
  while(Serial.available() < 4 && !timeout(&codeTimer));
  if(hasTimedOut(&codeTimer))
  {
    currentState = &error;
    return;
  }
  Code code = readCode();
  Serial.write(code->check);
  command->code = code->code;

  // Confirm: receiving 'ok'
  Availability avb;
  while((avb = isAvailable(OK)) == NOTYET && !timeout(&ackTimer));
  if(avb == ERR || hasTimedOut(&okTimer))
  {
    currentState = &error;
    return;
  }

  // Sending
  irSender.send(command->protocol, command->code, command->length);

  // Changing state
  currentState = &success;
  Serial.write(sent->word);
    
}

/*------------------------------------SUCCESS------------------------------ */
void success()
{
  LEDs leds = {ON, BLINK}
  led(&leds);
  state = &waiting;
  
}

/*-------------------------------------ERROR------------------------------- */
void error()
{
  LEDs leds = {BLINK, BLINK}
  led(&leds);
  state = &ready;
  
}
