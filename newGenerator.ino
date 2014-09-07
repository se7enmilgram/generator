
#include <util.h>
#include <TimerOne.h>
#include <SoftwareSerial.h>

/* Ethernet and Web Config
static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
static uint8_t ip[] = { 10,0,1,25 };
WebServer www("", 80);
*/
/* magic numbers */
#define HISTLEN 5
#define QUEUELEN 20

/* globals and setup */
struct pins_config {
  int starter;
  int ignition;
  int flowsensor;
  int panel;
} pins;

typedef struct cmd {
    char instr[5];
    int arg1;
    int arg2;
} cmd;

struct state {
  volatile int ticks;
  int tickhistory[HISTLEN];
  int histidx;
  int totalticks;
  cmd queue[QUEUELEN];
  int queueidx;
} g;


void setup() {
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
  Timer1.initialize(1000000);
  Timer1.attachInterrupt(interruptSecondlyLoop);
  attachInterrupt(pins.flowsensor, flowsensor, RISING);
}
  
/* interrupt functions */
void flowsensor() {
  g.ticks++;
  Serial.print("tick!\r\n");
}

void printState() {
  Serial.print("pins.starter: ");
  Serial.print(digitalRead(pins.starter));
  Serial.print("\r\n");
  Serial.print("pins.ignition: ");
  Serial.print(digitalRead(pins.ignition));
  Serial.print("\r\n");
  Serial.print("pins.panel: ");
  Serial.print(digitalRead(pins.panel));
  Serial.print("\r\n");
  Serial.print("flow sensor:\r\b");
  Serial.print("\tg.ticks = ");
  Serial.print(g.ticks);
  Serial.print("\r\n");
  Serial.print("\tg.totalticks = ");
  Serial.print(g.totalticks);
  Serial.print("\r\n");
  Serial.print("\tg.histidx = ");
  Serial.print(g.histidx);
  Serial.print("\r\n");
  for( int i = 0; i < HISTLEN; i++ ) {
    Serial.print("\t tickhistory[");
    Serial.print(i);
    Serial.print("]: ");
    Serial.print(g.tickhistory[i]);
    Serial.print("\r\n");
  }
}

void secondlyFlow() {
  /* we need to collect the ticks up to this point */
  g.totalticks += g.ticks;
  g.tickhistory[g.histidx] = g.ticks;
  g.histidx = (g.histidx + 1) % HISTLEN;
  /* clear secondly counter */
  g.ticks = 0;
  g.histidx = 0;
}

void secondlyIgnition() {
}

void secondlyPanel() {
}

void secondlyStarter() {
}

void interruptSecondlyLoop () {
    g.ticks++;
  printState();
  //secondlyFlow();
  //secondlyIgnition();
  //secondlyPanel();
  //secondlyStarter();
}

/* procedural functions */
void loop () {
    Serial.println(g.ticks);
    delay(1000);
}
