#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>
#include <util.h>
#include <TimerOne.h>
#include <WebServer.h>
#include <SoftwareSerial.h>

/* Ethernet and Web Config */
static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
static uint8_t ip[] = { 10,0,1,25 };
WebServer www("", 80);

/* globals and setup */
struct pins_config {
  int starter;
  int ignition;
  int flowsensor;
  int panel;
} pins;

struct state {
  volatile int ticks;
  int tickhistory[5];
  int totalticks;
} g;


void setup() {
  Ethernet.begin(mac, ip);
  Serial.begin(9600);
  /* state init */
  g.ticks = 0;
  g.totalticks = 0;
  int histinit[5] = {-1,-1,-1,-1,-1};
  memcpy(g.tickhistory, histinit, sizeof(g.tickhistory));
  
  pins.starter = 5;
  pins.ignition = 6;
  pins.flowsensor = 7;
  pins.panel = 8;
  
  /* io config */
  pinMode(pins.starter, OUTPUT);
  pinMode(pins.ignition, OUTPUT);
  pinMode(pins.flowsensor, INPUT);
  pinMode(pins.panel, INPUT);
  
  digitalWrite(pins.starter, HIGH);
  digitalWrite(pins.ignition, HIGH);
  
  /* interrupt setup */
  Timer1.initialize(1000);
  Timer1.attachInterrupt(interruptLoop);
  attachInterrupt(pins.flowsensor, flowsensor, RISING);
}
  
/* interrupt functions */
void flowsensor() {
  g.ticks++;
  Serial.print("tick!\r\b");
}

void interruptLoop () {
  /* we need to collect the ticks up to this point */
  /* reset the current ticks counter to zero */
  g.totalticks += g.ticks;
  /* shuffle the historic ticks down the line */
  g.tickhistory[4] = g.tickhistory[3];
  g.tickhistory[3] = g.tickhistory[2];
  g.tickhistory[2] = g.tickhistory[1];
  g.tickhistory[1] = g.tickhistory[0];
  g.tickhistory[0] = g.ticks;
  g.ticks = 0;
}

/* procedural functions */
void loop () {
};
