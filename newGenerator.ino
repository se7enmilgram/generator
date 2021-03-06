#include <Ethernet.h>
//#include <util.h>
#include <TimerOne.h>
#include <SoftwareSerial.h>
#include <WebServer.h>
#include "SPI.h"

P(ok) = "OK\r\n";

/* Ethernet and Web Config */
static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
static uint8_t ip[] = { 10,0,0,25 };
WebServer www("", 80);

/* magic numbers */
#define HISTLEN 10
#define QUEUELEN 40
#define NOOP 0
#define SPIN 1
#define WAIT 2
#define WTCH 3
#define LAST 4

/* globals and setup */
struct pins_config {
  int starter;
  int ignition;
  int flowsensor;
  int panel;
} pins;

/* COMMAND QUEUE
name    inst arg1    arg2    description
noop    0                    do nothing
spin    1    n       val    digitalWrite(n,val)
wait    2    n               descrement n until 0
wtch    3    n               decrement n until 0 or crazy tickhist logic indicates water flow is zero
*/

typedef struct cmd {
    int inst;
    int arg1;
    int arg2;
} cmd;


cmd noop;
cmd last;
  
struct state {
  volatile int ticks;
  int tickhistory[HISTLEN];
  int histidx;
  int totalticks;
  cmd queue[QUEUELEN];
  int queueidx;
  int queueadv;
  bool printState;
  bool printCmdQueue;
  bool collateFlow;
  bool doCommand;
} g;


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

void webPrintInt( WebServer &server, char* name, int x, int indent, int index ) {
    for (int i=0; i<indent; i++){
        server.print("\t");
    }
    server.print(name);
    if (index != -1) {
        server.print("[");
        server.print(index);
        server.print("]");
    }
    server.print(": ");
    server.print(x);
    server.print("\r\n");
}

void webPrintChr( WebServer &server, char* name, char* x, int indent, int index ) {
    for (int i=0; i<indent; i++){
        server.print("\t");
    }
    server.print(name);
    if (index != -1) {
        server.print("[");
        server.print(index);
        server.print("]");
    }
    server.print(": ");
    server.print(x);
    server.print("\r\n");
}

void webPrintPin( WebServer &server, char* name, int x, int indent, int index ) {
    for (int i=0; i<indent; i++){
        server.print("\t");
    }
    server.print(name);
    if (index != -1) {
        server.print("[");
        server.print(index);
        server.print("]");
    }
    server.print(": ");
    server.print(x ? "HIGH" : "LOW");
    server.print("\r\n");
}

void webPrintHist(WebServer &server, WebServer::ConnectionType type, char *, bool) {
    server.httpSuccess();
    webPrintInt( server, "g.histidx", g.histidx, 1, -1);
    server.print("Flow History:\r\n");
    for( int i = 0; i < HISTLEN; i++ ) {
        webPrintInt( server, "g.tickhistory",g.tickhistory[i], 1, i);
    }  
}

void webPrintState(WebServer &server, WebServer::ConnectionType type, char *, bool) {
    server.httpSuccess();
    webPrintPin( server, "pins.starter", digitalRead(pins.starter), 0,-1);
    webPrintPin( server, "pins.ignition", digitalRead(pins.ignition), 0,-1);
    webPrintPin( server, "pins.panel", digitalRead(pins.panel), 0,-1);
    server.print("Flow Sensor:\r\n");
    webPrintInt( server, "g.ticks", g.ticks, 1,-1);
    webPrintInt( server, "g.totalticks", g.totalticks, 1,-1);  
}

void webPrintCmdQueue(WebServer &server, WebServer::ConnectionType type, char *, bool) {
    server.httpSuccess();
    webPrintInt( server, "g.queueidx", g.queueidx, 0, -1);
    webPrintInt( server, "g.queueadv", g.queueadv, 0, -1);
    for ( int i = 0; i < QUEUELEN; i++ ) {
        server.print( i );
        server.print(": ");
        if (g.queue[i].inst == NOOP) {
            server.print("NOOP");
        } else if (g.queue[i].inst == SPIN) {
            server.print("SPIN");
        } else if (g.queue[i].inst == WAIT) {
            server.print("WAIT");
        } else if (g.queue[i].inst == WTCH) {
            server.print("WAIT");
        } else { 
            server.print("BAD");
        }
        server.print("\t");
        server.print(g.queue[i].arg1);
        server.print("\t");
        server.print(g.queue[i].arg2);
        server.print("\r\n");
    }
}

void webFlushQueue(WebServer &server, WebServer::ConnectionType type, char *, bool) {
  for (int i=0; i < QUEUELEN; i++) {
      g.queue[i] = noop;
  }
  g.queueidx = 0;
  g.queueadv = 0;
  digitalWrite(pins.ignition, HIGH);
  digitalWrite(pins.starter, HIGH);
  
  server.httpSuccess();
  server.printP(ok);
}

void webPumpForHour(WebServer &server, WebServer::ConnectionType type, char *, bool){
        if (digitalRead(pins.panel)) {
        server.httpFail();
        server.print("NOT OK: PANEL ALREADY HIGH\r\n");
    } else {
        server.httpSuccess();
        addCommand(SPIN, pins.ignition, LOW);
        addCommand(SPIN, pins.starter, LOW);
        addCommand(WAIT, 3, -1);
        addCommand(SPIN, pins.starter, HIGH);
        addCommand(WAIT, 297, -1);
        addCommand(SPIN, pins.ignition, HIGH);
        addCommand(SPIN, pins.panel, LOW);
        addCommand(WAIT, 60*15, -1);
        addCommand(SPIN, pins.ignition, LOW);
        addCommand(SPIN, pins.starter, LOW);
        addCommand(WAIT, 3, -1);
        addCommand(SPIN, pins.starter, HIGH);
        addCommand(WAIT, 297, -1);
        addCommand(SPIN, pins.ignition, HIGH);
        addCommand(SPIN, pins.panel, LOW);
        addCommand(WAIT, 60*15, -1);
        addCommand(SPIN, pins.ignition, LOW);
        addCommand(SPIN, pins.starter, LOW);
        addCommand(WAIT, 3, -1);
        addCommand(SPIN, pins.starter, HIGH);
        addCommand(WAIT, 297, -1);
        addCommand(SPIN, pins.ignition, HIGH);
        addCommand(SPIN, pins.panel, LOW);
        addCommand(WAIT, 60*15, -1);
        addCommand(SPIN, pins.ignition, LOW);
        addCommand(SPIN, pins.starter, LOW);
        addCommand(WAIT, 3, -1);
        addCommand(SPIN, pins.starter, HIGH);
        addCommand(WAIT, 297, -1);
        addCommand(SPIN, pins.ignition, HIGH);
        addCommand(SPIN, pins.panel, LOW);
        addCommand(WAIT, 60*15, -1);
        server.printP(ok); 
    }
}

void webPrintStatus(WebServer &server, WebServer::ConnectionType type, char *, bool){
    server.httpSuccess();
    if (g.queue[g.queueidx].inst == NOOP) {
        server.print("IDLE\r\n");
    } else if (g.queue[g.queueidx].inst == WAIT) {
        webPrintInt(server, "WAITING", g.queue[g.queueidx].arg1,0,-1);
    } else if (g.queue[g.queueidx].inst == SPIN) {
        webPrintInt(server, "SETTING PIN", g.queue[g.queueidx].arg2,0,g.queue[g.queueidx].arg1);
    }
}

void webPump(WebServer &server, WebServer::ConnectionType type, char *, bool){
    if (digitalRead(pins.panel)) {
        server.httpFail();
        server.print("NOT OK: PANEL ALREADY HIGH\r\n");
    } else {
        server.httpSuccess();
        addCommand(SPIN, pins.ignition, LOW);
        addCommand(SPIN, pins.starter, LOW);
        addCommand(WAIT, 3, -1);
        addCommand(SPIN, pins.starter, HIGH);
        addCommand(WAIT, 297, -1);
        addCommand(SPIN, pins.ignition, HIGH);
        addCommand(SPIN, pins.panel, LOW);
        server.printP(ok); 
    }
}

void webCmd( WebServer &server, WebServer::ConnectionType type, char * url_tail, bool ) {
  URLPARAM_RESULT rc;
  #define NAMELEN 32
  #define VALUELEN 32
  
  char name[NAMELEN];
  char value[VALUELEN];
  
  //char name_a[3][NAMELEN];
  char value_a[3][VALUELEN];
  
  int i = 0;
  cmd c;
  server.httpSuccess();
  if (strlen(url_tail)) {
    while (strlen(url_tail)) {
     rc = server.nextURLparam(&url_tail, name, NAMELEN, value, VALUELEN);
     if (rc == URLPARAM_EOS) {}
        else {
            //name_a[i] = name;
            /*
            printChr("name",name,0,-1);
            printInt("name sizeof", sizeof(name), 0, -1);
            printChr("val",value,0,-1);
            printInt("val sizeof", sizeof(value), 0, -1);
            */
            strcpy(value_a[i],value);
            i++;
        }
    }
    /* time to check that insanity for appropriate input */
    if (i != 3) {
        printInt("BAD NEWS FOR URL PARAMS", i, 0, -1);
    } else {
        addCommand( atoi(&value_a[0][0]), atoi(&value_a[1][0]), atoi(&value_a[2][0]) );
    }
  }
  P(navMsg) = "";
  server.printP(navMsg);
}


void setup() {
  Serial.begin(9600);
  /* state init */
  g.ticks = 0;
  g.totalticks = 0;
  memset(g.tickhistory, -1, sizeof(g.tickhistory));
  g.queueidx = 0;
  g.queueadv = 0;
  
  /* useful global commands */
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
  
  digitalWrite(pins.starter, HIGH);
  digitalWrite(pins.ignition, HIGH);
  
  /* interrupt setup */
  Timer1.initialize(1000000);
  Timer1.attachInterrupt(secondlyInterrupt);
  
  // set to RISING for actual hall sensor.
  attachInterrupt(1, flowsensor, RISING);

  /* ethernet and web setup oh god now */
  Ethernet.begin(mac, ip);
  www.setDefaultCommand(&webIndex);
  www.addCommand("cmd", &webCmd);
  www.addCommand("pumpFor5", &webPump);
  www.addCommand("pumpFor60", &webPumpForHour);
  www.addCommand("cmdQueue", &webPrintCmdQueue);
  www.addCommand("state", &webPrintState);
  www.addCommand("status", &webPrintStatus);
  www.addCommand("tickhist", &webPrintHist);
  www.addCommand("flush", &webFlushQueue);
  www.begin();
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

void insertCommand( int idx, int inst, int arg1, int arg2) {
    if ( (inst >= 0) && (inst < LAST) ) {
        cmd c;
        c.inst = inst;
        c.arg1 = arg1;
        c.arg2 = arg2;
        g.queue[idx] = c;
    } else {
        /* invalid inst sent, print an error and return */
        printInt("ERROR: inst out of bounds", inst, 0, -1);
    }
    
}

void addCommand( int inst, int arg1, int arg2 ) {
    /* if adv = idx and we're on a noop, we assume we're idle
       and insert the command into the current index, then advance.
       if adv = idx and we're not on a noop, assume we've wrapped
       the queue and print an error, then do nothing */
    if ( (g.queueidx == g.queueadv) && (g.queue[g.queueidx].inst == NOOP) ) {
        insertCommand( g.queueadv, inst, arg1, arg2);
        g.queueadv = (g.queueadv + 1) % QUEUELEN;
    } else if ( g.queueidx == g.queueadv ) {
        printInt("ERROR: CMDQUEUE FULL", g.queueidx,0,-1);
    } else {
        insertCommand( g.queueadv, inst, arg1, arg2);
        g.queueadv = (g.queueadv + 1) % QUEUELEN;
    }
}

void doCommands() {
    if( g.queue[g.queueidx].inst == NOOP ) {
    } else 
    if ( g.queue[g.queueidx].inst == SPIN ) {
        Serial.println("Setting a pin...");
        digitalWrite(g.queue[g.queueidx].arg1, g.queue[g.queueidx].arg2);
        g.queue[g.queueidx] = noop;
        advQueueIdx();
    } 
    if (g.queue[g.queueidx].inst == WAIT ){
        Serial.println("Waiting...");
        g.queue[g.queueidx].arg1--;
        if (g.queue[g.queueidx].arg1 == 1) {
            g.queue[g.queueidx] = noop;
            advQueueIdx();
        }
        g.doCommand=false;
    }
}

void advQueueIdx () {
    if (g.queueadv == g.queueidx) {
        g.queueadv = (g.queueadv + 1) % QUEUELEN;
    }
    g.queueidx = (g.queueidx + 1) % QUEUELEN;
}

void secondlyInterrupt() {
  g.printState = false;
  g.collateFlow = true;
  g.printCmdQueue = false;
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
  char buff[64];
  int len = 64;

  /* process incoming connections one at a time forever */
  www.processConnection(buff, &len);
}

