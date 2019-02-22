#include <Wiegand.h>
#include <TaskScheduler.h>

#define SEC_OPEN 10        // number of seconds to hold door open after valid UID
#define SECONDS 1000      // multiplier for second

// Pin Assignments

#define DATA0 2       // input - Wiegand Data 0
#define DATA1 3       // input - Wiegand Data 1
#define LED 4         // output - LED changes to green if LOW
#define BEEP 5        // output - peep sound on if LOW

#define OPEN_CLOSE 6  // output - opens lock as long as HIGH
#define RIEGEL 7      // input - sense contacts within lock --> FOCUS
#define KLINKE 8      // input - sense contacts within lock

// Callback methods prototypes
void t1Callback();
void t3Callback();
void LOCK_DOOR();
void LED_BEEP();

//Tasks
Task t1(500, TASK_FOREVER, &t1Callback); // task checking the RFID-Reader
Task t2(100, TASK_ONCE, &LOCK_DOOR);      // task to lock the door again
Task t3(250, TASK_FOREVER, &t3Callback);  // check door contacts status
Task t4(1,TASK_ONCE, &LED_BEEP);          // task for Reader beeper

WIEGAND wg;
Scheduler runner;

// VARIABLES
bool ahis = true; // Riegel history value
bool bhis = true; // Klinke history value
byte dshis = true; // door status history value

int bufferCount;    // Anzahl der eingelesenen Zeichen
char buffer[10];    // Serial Input-Buffer

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

        digitalWrite(OPEN_CLOSE, false);
        digitalWrite(BEEP, true);
        digitalWrite(LED, true);

        runner.init();
        runner.addTask(t1);
        runner.addTask(t2);
        runner.addTask(t3);
        runner.addTask(t4);

        t1.enable(); // start cyclic readout of reader
        t3.enable(); // start cyclic readout of door status
}

// Functions following -------------------------------------------------

// START of Tasks

void t1Callback() {
        if (wg.available())  {          // check for data on Wiegand Bus
                if (wg.getCode() < 1000) return; // exclude strange "5" detection
                t1.disable(); // Stop task
                Serial.println((String)"card;" + wg.getCode());
                t1.enableDelayed(5 * SECONDS); // RFID Reader sleeping for x seconds
                // eventually enable by RASPI?
        }
}

void t3Callback() {
        byte a = digitalRead(RIEGEL);
        byte b = digitalRead(KLINKE);

        if(a != ahis) {
                ahis = a;
                Serial.println((String)"door;" + a);
        }
}
// END of Tasks

void LOCK_DOOR(void) {              // Lock Door
        digitalWrite(OPEN_CLOSE, false);
        digitalWrite(LED, true);
}

void UNLOCK_DOOR(void) {            // Unlock Door for 'SEC_OPEN' seconds
        digitalWrite(OPEN_CLOSE, true);
        digitalWrite(LED, false);
        t2.restartDelayed(SEC_OPEN * SECONDS); // start task in 5 sec to close door
}

void RESTART_READER(void) {
        t1.enable();
}

void LED_BEEP() {
        digitalWrite(BEEP, false);
        t4.setCallback(&LED_BEEP2);
        t4.restartDelayed(200);
}
void LED_BEEP2() {
        digitalWrite(BEEP, true);
        t4.setCallback(&LED_BEEP3);
        t4.restartDelayed(100);
}
void LED_BEEP3() {
        t4.setCallback(&LED_BEEP);
}

void LED_2TBEEP() {
        digitalWrite(BEEP, false);
        t4.setCallback(&LED_2TBEEP2);
        t4.restartDelayed(200);
}

void LED_2TBEEP2() {
        digitalWrite(BEEP, true);
        t4.setCallback(&LED_BEEP);
        t4.restartDelayed(100);
}

void LED_3TBEEP() {
        digitalWrite(BEEP, false);
        t4.setCallback(&LED_3TBEEP2);
        t4.restartDelayed(200);
}

void LED_3TBEEP2() {
        digitalWrite(BEEP, true);
        t4.setCallback(&LED_2TBEEP);
        t4.restartDelayed(100);
}

void evalSerialData() {
        if ((buffer[0] == '>') && (buffer[2] == '<')) {
                switch (buffer[1]) {
                case 'a': // UNLOCK '>a<'
                case 'A': // UNLOCK
                        UNLOCK_DOOR();
                        break;
                case 'b': // BEEP1  '>b<'
                case 'B': // BEEP1
                        t4.setCallback(&LED_BEEP);
                        t4.restart();
                        break;
                case 'c': // BEEP2  '>c<'
                case 'C': // BEEP2
                        t4.setCallback(&LED_2TBEEP);
                        t4.restart();
                        break;
                case 'd': // BEEP3  '>d<'
                case 'D': // BEEP3
                        t4.setCallback(&LED_3TBEEP);
                        t4.restart();
                        break;
                }
        }
        bufferCount = 0;
}

void loop() {
        runner.execute();
}
void serialEvent() {
        char ch = (char)Serial.read();
        buffer[bufferCount] = ch;
        bufferCount++;
        if (ch == '\x0d') {
                evalSerialData();
        }
}
