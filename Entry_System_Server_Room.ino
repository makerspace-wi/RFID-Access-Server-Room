#include <Wiegand.h>
#include <TaskScheduler.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

#define DEBUG 1

#define SEC_OPEN 10        // number of seconds to hold door open after valid UID
#define SECONDS 1000      // multiplier for second

// Pin Assignments

#define DATA0 12       // input - Wiegand Data 0
#define DATA1 11       // input - Wiegand Data 1
#define LED 9          // output - LED changes to green if LOW
#define BEEP 8         // output - peep sound on if LOW

#define OPEN_CLOSE 10  // output - opens lock as long as HIGH
#define RIEGEL 7       // input - sense contacts within lock --> FOCUS
#define KLINKE 6       // input - sense contacts within lock

#define MQTT_NAME "Serverroom_Access"
#define MQTT_TOPIC_STATUS_ONLINE "serverroom/status/online" // to Server, payload: 1
#define MQTT_TOPIC_CARD "serverroom/card" // to Server
#define MQTT_TOPIC_DOOR "serverroom/door" // to Server
#define MQTT_TOPIC_UNLOCK "serverroom/unlock" // from Server, payload: 1 = Unlock
#define MQTT_TOPIC_BEEP "serverroom/beep" // from Server, payload: 1 = Beep 1, 2 = Beep 2, 3 = Beep 3

// TODO: Anpassen
byte mac[] = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xA0 };
IPAddress ip(192, 168, 100, 11);
IPAddress server(192, 168, 100, 10);
uint16_t server_port = 1884;


#if DEBUG
  #define DEBUG_PRINT(x)  Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// Callback methods prototypes
void t1Callback();
void t3Callback();
void lock_door();
void led_beep1_0();

WIEGAND wg;
Scheduler runner;

//Tasks
Task t1(500, TASK_FOREVER, &t1Callback, &runner, true); // task checking the RFID-Reader
Task t2(100, TASK_ONCE, &lock_door, &runner, false);      // task to lock the door again
Task t3(250, TASK_FOREVER, &t3Callback, &runner, true);  // check door contacts status
Task t4(1,TASK_ONCE, &led_beep1_0, &runner, false);          // task for Reader beeper

// VARIABLES
bool ahis = true; // Riegel history value
bool bhis = true; // Klinke history value
byte dshis = true; // door status history value

EthernetClient ethClient;
PubSubClient client(ethClient);

void digitalWriteBeep(bool state) {
  DEBUG_PRINT("digitalWrite BEEP: ");
  DEBUG_PRINTLN(state);
  digitalWrite(BEEP, state);
}

void digitalWriteLed(bool state) {
  DEBUG_PRINT("digitalWrite LED: ");
  DEBUG_PRINTLN(state);
  digitalWrite(LED, state);
}

void digitalWriteOpenClose(bool state) {
  DEBUG_PRINT("digitalWrite OPEN_CLOSE: ");
  DEBUG_PRINTLN(state);
  digitalWrite(OPEN_CLOSE, state);
  digitalWrite(LED_BUILTIN, state);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
    Serial.print(" ");
  }
  Serial.println();

  if (strcmp(topic, MQTT_TOPIC_UNLOCK) == 0) {
    if (payload[0] == '1') {
        unlock_door();
    }
  } else if (strcmp(topic, MQTT_TOPIC_BEEP) == 0) {
    switch (payload[0]) {
      case '1':
          Serial.println("Starting Beep 1");
          t4.setCallback(&led_beep1_0);
          t4.restart();
        break;

      case '2':
          Serial.println("Starting Beep 2");
          t4.setCallback(&led_beep2_0);
          t4.restart();
        break;

      case '3':
          Serial.println("Starting Beep 3");
          t4.setCallback(&led_beep3_0);
          t4.restart();
        break;
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_NAME)) {
      Serial.println("Connected to MQTT Server");
      
      client.subscribe(MQTT_TOPIC_UNLOCK);
      client.subscribe(MQTT_TOPIC_BEEP);

      client.publish(MQTT_TOPIC_STATUS_ONLINE, "1");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  wg.begin(); // start Wiegand Bus Control

  pinMode(DATA0, INPUT);
  pinMode(DATA1, INPUT);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(BEEP, OUTPUT);
  pinMode(OPEN_CLOSE, OUTPUT);

  pinMode(RIEGEL, INPUT_PULLUP);
  pinMode(KLINKE, INPUT_PULLUP);

  digitalWriteOpenClose(false);
  digitalWriteBeep(true);
  digitalWriteLed(true);

  Ethernet.init(17);  // WIZnet W5100S-EVB-Pico W5500-EVB-Pico W6100-EVB-Pico


  client.setServer(server, server_port);
  client.setCallback(callback);

  Ethernet.begin(mac, ip);
  delay(1500);

  runner.startNow();

}

// Functions following -------------------------------------------------

// START of Tasks

void t1Callback() {
  if (wg.available())  {          // check for data on Wiegand Bus
      if (wg.getCode() < 1000) return; // exclude strange "5" detection
      t1.disable(); // Stop task

      unsigned long code = wg.getCode();

      Serial.print("Read Card: ");
      Serial.println(code);

      char payload[32];
      sprintf(payload, "%lu", code);
      client.publish(MQTT_TOPIC_CARD, payload);

      t1.enableDelayed(5 * SECONDS); // RFID Reader sleeping for x seconds
      // eventually enable by RASPI?
  }
}

void t3Callback() {
  byte a = digitalRead(RIEGEL);
  byte b = digitalRead(KLINKE);

  if(a != ahis) {
    ahis = a;
    client.publish(MQTT_TOPIC_DOOR, (a ? "1" : "0"));
  }
}
// END of Tasks

void lock_door(void) {              // Lock Door
  Serial.println("Locking door");
  digitalWriteOpenClose(false);
  digitalWriteLed(true);
}

void unlock_door(void) {            // Unlock Door for 'SEC_OPEN' seconds
  Serial.println("Unlocking door");
  digitalWriteOpenClose(true);
  digitalWriteLed(false);
  t2.restartDelayed(SEC_OPEN * SECONDS); // start task in 5 sec to close door
}

void restart_reader(void) {
  t1.enable();
}

void led_beep1_0() {
  digitalWriteBeep(false);
  t4.setCallback(&led_beep1_1);
  t4.restartDelayed(200);
}
void led_beep1_1() {
  digitalWriteBeep(true);
  t4.setCallback(&led_beep1_2);
  t4.restartDelayed(100);
}
void led_beep1_2() {
  t4.setCallback(&led_beep1_0);
}


void led_beep2_0() {
  digitalWriteBeep(false);
  t4.setCallback(&led_beep2_1);
  t4.restartDelayed(200);
}

void led_beep2_1() {
  digitalWriteBeep(true);
  t4.setCallback(&led_beep1_0);
  t4.restartDelayed(100);
}

void led_beep3_0() {
  digitalWriteBeep(false);
  t4.setCallback(&led_beep3_1);
  t4.restartDelayed(200);
}

void led_beep3_1() {
  digitalWriteBeep(true);
  t4.setCallback(&led_beep2_0);
  t4.restartDelayed(100);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  runner.execute();
}
