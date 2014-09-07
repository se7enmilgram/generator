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

/* magic numbers */
#define HISTLEN 5

/* globals and setup */
struct pins_config {
  int starter;
  int ignition;
  int flowsensor;
  int panel;
} pins;

struct state {
  volatile int ticks;
  int tickhistory[HISTLEN];
  int histidx;
  int totalticks;
} g;


void setup() {
  Ethernet.begin(mac, ip);
  Serial.begin(9600);
  /* state init */
  g.ticks = 0;
  g.totalticks = 0;
  memset(g.tickhistory, -1, sizeof(g.tickhistory));
  
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
  Timer1.attachInterrupt(interruptSecondlyLoop);
  attachInterrupt(pins.flowsensor, flowsensor, RISING);
}
  
/* interrupt functions */
void flowsensor() {
  g.ticks++;
  Serial.print("tick!\r\b");
}

void flowSecondlyInterrupt () {
  /* we need to collect the ticks up to this point */
  g.totalticks += g.ticks;
  g.tickhistory[g.histidx] = g.ticks;
  g.histidx = (g.histidx + 1) % HISTLEN;
  /* clear secondly counter */
  g.ticks = 0;
  g.histidx = 0;
}

void interruptSecondlyLoop () {
  flowSecondlyInterrupt();
}

/* procedural functions */
void loop () {
};
