#include <Ethernet.h>
#include <util.h>
#include <TimerOne.h>
#include <SoftwareSerial.h>
#include <WebServer.h>
/* Ethernet and Web Config */
static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
static uint8_t ip[] = { 10,0,1,25 };
WebServer www("", 80);

/* magic numbers */
#define HISTLEN 5
#define QUEUELEN 5

/* globals and setup */
struct pins_config {
  int starter;
  int ignition;
  int flowsensor;
  int panel;
  int fakesensor;
} pins;

/* COMMAND QUEUE
name    inst arg1    arg2    description
noop    0                    do nothing
spin    1    n       val    digitalWrite(n,val)
wait    2    n               descrement n until 0
wtch    3    n               decrement n until 0 or crazy tickhist logic indicates water flow is zero
*/

typedef enum command {
  NOOP,
  SPIN,
  WAIT,
  WTCH,
  LAST_COMMAND
} command;

typedef struct cmd {
    command inst;
    int arg1;
    int arg2;
} cmd;


cmd noop;
  
struct state {
  volatile int ticks;
  int tickhistory[HISTLEN];
  int histidx;
  int totalticks;
  cmd queue[QUEUELEN];
  int queueidx;
  bool printState;
  bool printCmdQueue;
  bool collateFlow;
  bool doCommand;
} g;


void setup() {
  Serial.begin(9600);
  /* state init */
  g.ticks = 0;
  g.totalticks = 0;
  memset(g.tickhistory, -1, sizeof(g.tickhistory));
  g.queueidx = 0;
  noop.inst = NOOP;
  noop.arg1 = -1;
  noop.arg2 = -1;
  for (int i=0; i < QUEUELEN; i++) {
      g.queue[i] = noop;
  }
  g.doCommand = false;
  pins.starter = 5;
  pins.ignition = 6;
  pins.flowsensor = 3;
  pins.panel = 8;
  
  /* io config */
  pinMode(pins.starter, OUTPUT);
  pinMode(pins.ignition, OUTPUT);
  pinMode(pins.flowsensor, INPUT);
  pinMode(pins.panel, INPUT);
  //pinMode(pins.fakesensor, INPUT_PULLUP);
  
  digitalWrite(pins.starter, HIGH);
  digitalWrite(pins.ignition, HIGH);
  
  /* interrupt setup */
  Timer1.initialize(1000000);
  Timer1.attachInterrupt(secondlyInterrupt);
  // set to RISING for actual hall sensor.
  attachInterrupt(1, flowsensor, RISING);
  addCommand(0, WAIT, 3, -1);
  addCommand(1, SPIN, pins.starter, LOW);
  addCommand(2, WAIT, 3, -1);
  addCommand(3, SPIN, pins.starter, HIGH);
}
  
/* interrupt functions */
void flowsensor() {
  g.ticks++;
}

void printState() {
    printInt("pins.starter", digitalRead(pins.starter), 0,-1);
    printInt("pins.starter", digitalRead(pins.starter), 0,-1);
    printInt("pins.starter", digitalRead(pins.starter), 0,-1);
    printInt("pins.starter", digitalRead(pins.starter), 0,-1);
    printInt("pins.starter", digitalRead(pins.starter), 0,-1);
    Serial.println("Flow Sensor:");
    printInt("g.ticks", g.ticks, 1,-1);
    printInt("g.totalticks", g.totalticks, 1,-1);
    printInt("g.histidx", g.histidx, 1, -1);
    Serial.println("Flow History:");
    for( int i = 0; i < HISTLEN; i++ ) {
        printInt("g.tickhistory",g.tickhistory[i], 1, i);
    }    
}

void printCmdQueue() {
    printInt("g.queueidx", g.queueidx, 0, -1);
    for ( int i = 0; i < QUEUELEN; i++ ) {
        printInt("g.queue.inst", g.queue[i].inst, 1, i);
        printInt("g.queue.arg1", g.queue[i].arg1, 1, i);
        printInt("g.queue.arg2", g.queue[i].arg2, 1, i);
  }
  g.printCmdQueue = false;
}

void printInt( char* name, int x, int indent, int index ) {
    for (int i=0; i<indent; i++){
        Serial.print("\t");
    }
    Serial.print(name);
    if (index != -1) {
        Serial.print("[");
        Serial.print(index);
        Serial.print("]");
    }
    Serial.print(": ");
    Serial.print(x);
    Serial.print("\r\n");
}

void printChr( char* name, char* x, int indent, int index ) {
    for (int i=0; i<indent; i++){
        Serial.print("\t");
    }
    Serial.print(name);
    if (index != -1) {
        Serial.print("[");
        Serial.print(index);
        Serial.print("]");
    }
    Serial.print(": ");
    Serial.print(x);
    Serial.print("\r\n");
}

void printFloat( char* name, float x, int indent, int index ) {
    for (int i=0; i<indent; i++){
        Serial.print("\t");
    }
    Serial.print(name);
    if (index != -1) {
        Serial.print("[");
        Serial.print(index);
        Serial.print("]");
    }
    Serial.print(": ");
    Serial.print(x);
    Serial.print("\r\n");
}


void collateFlow() {
  /* we need to collect the ticks up to this point */
  g.totalticks += g.ticks;
  g.tickhistory[g.histidx] = g.ticks;
  g.histidx = (g.histidx + 1) % HISTLEN;
  /* clear secondly counter */
  g.ticks = 0;
  g.collateFlow = false;
}

void addCommand( int idx, command inst, int arg1, int arg2) {
    cmd c;
    c.inst = inst;
    c.arg1 = arg1;
    c.arg2 = arg2;
    g.queue[idx] = c;
}

void doCommands() {
    if( g.queue[g.queueidx].inst == NOOP ) {
        Serial.println("NOOP encountered...");
    } else 
    if ( g.queue[g.queueidx].inst == SPIN ) {
        Serial.println("Setting a pin...");
        digitalWrite(g.queue[g.queueidx].arg1, g.queue[g.queueidx].arg2);
        g.queue[g.queueidx] = noop;
            g.queueidx = (g.queueidx + 1) % QUEUELEN;
    } 
    if (g.queue[g.queueidx].inst == WAIT ){
        Serial.println("Waiting...");
        g.queue[g.queueidx].arg1--;
        if (g.queue[g.queueidx].arg1 == 0) {
            g.queue[g.queueidx] = noop;
            g.queueidx = (g.queueidx + 1) % QUEUELEN;
        }
    }
    g.doCommand=false;
}

void secondlyInterrupt() {
  g.printState = false;
  g.collateFlow = true;
  g.printCmdQueue = true;
  if (g.queue[g.queueidx].inst != NOOP){ 
      g.doCommand = true; 
  }
}

/* procedural functions */
void loop () {
  if( g.collateFlow ){
    collateFlow();
  }
  if( g.printState ){
    printState();
  }
  if( g.printCmdQueue ){
    printCmdQueue();
  }
  if (g.doCommand) {
      doCommands();
  }
  delay(100);
}

/* web functions */
void webIndex(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
  server.httpSuccess();
  if (type != WebServer::HEAD)
  {
    P(indexHTML) = "<html><a href='run' target='tgt'>Turn on Ignition</a> | <a href='start' target='tgt'>Start Engine</a> | <a href='stop' target='tgt'>Stop Engine</a><br/><iframe src='ready' name='tgt'/></html>";
    server.printP(indexHTML);
  }
}
void webNav( WebServer &server, WebServer::ConnectionType type, char *, bool ) {
  server.httpSuccess();
  P(navMsg) = "";
  server.printP(navMsg);
}

/*void webCmd( WebServer &server, WebServer::ConnectionType type, char * url_tail, bool ) {
  server.httpSuccess();
  if (strlen(url_tail))
    {
    while (strlen(url_tail))
      {
      rc = server.nextURLparam(&url_tail, name, NAMELEN, value, VALUELEN);
      if (rc == URLPARAM_EOS)
        server.printP(Params_end);
       else
        {
        server.print(name);
        server.printP(Parsed_item_separator);
        server.print(value);
        server.printP(Tail_end);
        }
      }
    }
  P(navMsg) = "";
  server.printP(navMsg);
}
*/

