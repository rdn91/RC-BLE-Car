// ================================================================
//  M1 = Front Left
//  M2 = Back Left
//  M3 = Back Right
//  M4 = Front Right
// ================================================================

#include <ArduinoBLE.h>
#include <AFMotor_R4.h>
#include <Servo.h>

// ── Pin assignments ──────────────────────────────────────────
#define SERVO_PIN     3
#define TRIG_PIN      A0
#define ECHO_PIN      A1
#define ISD_PLAY_PIN  A4
#define ISD_REC_PIN   A5

// ── Auto speed limits ────────────────────────────────────────
#define AUTO_SPEED_START  110
#define AUTO_SPEED_MAX    155
#define AUTO_SPEED_SLOW   85
#define AUTO_SPEED_TURN   150
#define AUTO_SPEED_REV    100

// ── Distance thresholds (cm) ─────────────────────────────────
#define DIST_STOP     22
#define DIST_SLOW     42
#define DIST_CLEAR    58
#define DIST_OPEN     80

// ── Timing ───────────────────────────────────────────────────
#define CMD_TIMEOUT       350
#define SERVO_SETTLE_MS   500
#define REVERSE_DURATION  650
#define TURN_BASE_MS      520
#define TURN_PER_BLOCK    100
#define TURN_MAX_MS       950

// ── Motors ───────────────────────────────────────────────────
AF_DCMotor m1(1), m2(2), m3(3), m4(4);
int baseSpeed  = 175;
int autoSpeed  = AUTO_SPEED_START;

// ── Servo ────────────────────────────────────────────────────
Servo scanServo;
#define SERVO_CENTER   90
#define SERVO_LEFT    148
#define SERVO_RIGHT    32

// ── BLE ──────────────────────────────────────────────────────
BLEService rcService("12345678-1234-1234-1234-123456789012");
BLEStringCharacteristic cmdChar(
  "12345678-1234-1234-1234-123456789013",
  BLEWrite | BLEWriteWithoutResponse, 20
);

// ── Modes ────────────────────────────────────────────────────
enum Mode { MANUAL, AUTO_NAV };
Mode currentMode = MANUAL;

// ── Manual state ─────────────────────────────────────────────
String activeCmd      = "S";
unsigned long lastCmdTime = 0;

// ── Auto nav states ───────────────────────────────────────────
enum NavState {
  NAV_MOVING,
  NAV_BRAKING,
  NAV_SCAN_LEFT,
  NAV_READ_LEFT,
  NAV_READ_RIGHT,
  NAV_DECIDE,
  NAV_REVERSE,
  NAV_TURNING,
  NAV_VERIFY
};

NavState      navState          = NAV_MOVING;
unsigned long navTimer          = 0;
int           distLeft          = 0;
int           distRight         = 0;
int           turnDir           = 0;
int           consecutiveBlocks = 0;
unsigned long autoStartTime     = 0;

// ================================================================
//  MOTOR FUNCTIONS
//
//  Layout:
//  M1 = Front Left   M4 = Front Right
//  M2 = Back Left    M3 = Back Right
//
//  Left side  = M1 + M2
//  Right side = M3 + M4
// ================================================================

void setAllSpeed(int spd) {
  spd = constrain(spd, 0, 255);
  m1.setSpeed(spd); m2.setSpeed(spd);
  m3.setSpeed(spd); m4.setSpeed(spd);
}

void driveForward(int spd) {
  setAllSpeed(spd);
  m1.run(FORWARD);   // front left — faces inward
  m2.run(BACKWARD);  // back left  — faces inward opposite
  m3.run(FORWARD);   // back right — faces inward
  m4.run(BACKWARD);  // front right — faces inward opposite
}

void driveBackward(int spd) {
  setAllSpeed(spd);
  m1.run(BACKWARD);
  m2.run(FORWARD);
  m3.run(BACKWARD);
  m4.run(FORWARD);
}

void driveLeft(int spd) {
  setAllSpeed(spd);
  m1.run(BACKWARD);  // left side back
  m2.run(FORWARD);
  m3.run(FORWARD);   // right side forward
  m4.run(BACKWARD);
}

void driveRight(int spd) {
  setAllSpeed(spd);
  m1.run(FORWARD);   // left side forward
  m2.run(BACKWARD);
  m3.run(BACKWARD);  // right side back
  m4.run(FORWARD);
}



void driveStop() {
  setAllSpeed(0);
  m1.run(RELEASE); m2.run(RELEASE);
  m3.run(RELEASE); m4.run(RELEASE);
}

// ================================================================
//  ULTRASONIC
// ================================================================
long singleRead() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(4);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  if (dur == 0) return 200;
  return constrain(dur / 58L, 2, 200);
}

long readDistance() {
  long a = singleRead(); delay(12);
  long b = singleRead(); delay(12);
  long c = singleRead();
  if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
  if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
  return c;
}

// ================================================================
//  ISD1820
// ================================================================
void isdPlay() {
  digitalWrite(ISD_PLAY_PIN, LOW);
  delayMicroseconds(50);
  digitalWrite(ISD_PLAY_PIN, HIGH);
  delay(80);
  digitalWrite(ISD_PLAY_PIN, LOW);
}

void isdRecordStart() { digitalWrite(ISD_REC_PIN, HIGH); }
void isdRecordStop()  { digitalWrite(ISD_REC_PIN, LOW);  }

// ================================================================
//  SERVO
// ================================================================
void servoTo(int angle) {
  scanServo.write(constrain(angle, 0, 180));
}

// ================================================================
//  MANUAL MODE
// ================================================================
void handleManual() {
  if (millis() - lastCmdTime > CMD_TIMEOUT) {
    driveStop();
    return;
  }
  if      (activeCmd == "F") driveForward(baseSpeed);
  else if (activeCmd == "B") driveBackward(baseSpeed);
  else if (activeCmd == "L") driveLeft(baseSpeed);
  else if (activeCmd == "R") driveRight(baseSpeed);
  else                       driveStop();
}

// ================================================================
//  AUTO NAV
// ================================================================
void rampAutoSpeed() {
  unsigned long elapsed = millis() - autoStartTime;
  autoSpeed = map(constrain(elapsed, 0, 2000),
                  0, 2000,
                  AUTO_SPEED_START, AUTO_SPEED_MAX);
}

void handleAuto() {
  unsigned long now = millis();

  switch (navState) {

    case NAV_MOVING: {
      long dist = readDistance();
      rampAutoSpeed();

      if (dist > DIST_OPEN) {
        driveForward(autoSpeed);
        servoTo(SERVO_CENTER);
      }
      else if (dist > DIST_SLOW) {
        driveForward(min(autoSpeed, AUTO_SPEED_MAX));
      }
      else if (dist > DIST_STOP) {
        int spd = map(dist, DIST_STOP, DIST_SLOW, AUTO_SPEED_SLOW, AUTO_SPEED_MAX);
        driveForward(spd);
      }
      else {
        driveStop();
        consecutiveBlocks++;
        navState = NAV_BRAKING;
        navTimer = now;
        Serial.print("Obstacle! dist=");
        Serial.print(dist);
        Serial.print(" blocks=");
        Serial.println(consecutiveBlocks);
      }
      break;
    }

    case NAV_BRAKING: {
      driveStop();
      if (now - navTimer > 200) {
        servoTo(SERVO_LEFT);
        navState = NAV_SCAN_LEFT;
        navTimer = now;
      }
      break;
    }

    case NAV_SCAN_LEFT: {
      driveStop();
      if (now - navTimer > SERVO_SETTLE_MS) {
        distLeft = readDistance();
        Serial.print("distLeft="); Serial.println(distLeft);
        servoTo(SERVO_RIGHT);
        navState = NAV_READ_LEFT;
        navTimer = now;
      }
      break;
    }

    case NAV_READ_LEFT: {
      driveStop();
      if (now - navTimer > SERVO_SETTLE_MS) {
        distRight = readDistance();
        Serial.print("distRight="); Serial.println(distRight);
        servoTo(SERVO_CENTER);
        navState = NAV_READ_RIGHT;
        navTimer = now;
      }
      break;
    }

    case NAV_READ_RIGHT: {
      driveStop();
      if (now - navTimer > 250) {
        navState = NAV_DECIDE;
        navTimer = now;
      }
      break;
    }

    case NAV_DECIDE: {
      driveStop();
      bool leftClear  = distLeft  > DIST_STOP;
      bool rightClear = distRight > DIST_STOP;

      if (!leftClear && !rightClear) {
        Serial.println("Both blocked — reversing");
        navState = NAV_REVERSE;
        navTimer = now;
      }
      else if (leftClear && !rightClear) {
        turnDir  = -1;
        navState = NAV_TURNING;
        navTimer = now;
        Serial.println("Turning LEFT");
      }
      else if (rightClear && !leftClear) {
        turnDir  = 1;
        navState = NAV_TURNING;
        navTimer = now;
        Serial.println("Turning RIGHT");
      }
      else {
        if (distLeft > distRight + 10) {
          turnDir = -1;
          Serial.println("Turning LEFT (more space)");
        } else if (distRight > distLeft + 10) {
          turnDir = 1;
          Serial.println("Turning RIGHT (more space)");
        } else {
          turnDir = -1;
          Serial.println("Turning LEFT (default)");
        }
        navState = NAV_TURNING;
        navTimer = now;
      }
      break;
    }

    case NAV_REVERSE: {
      driveBackward(AUTO_SPEED_REV);
      if (now - navTimer > REVERSE_DURATION) {
        driveStop();
        servoTo(SERVO_LEFT);
        navState = NAV_SCAN_LEFT;
        navTimer = now;
        Serial.println("Reverse done — re-scanning");
      }
      break;
    }

    case NAV_TURNING: {
      int turnDur = constrain(
        TURN_BASE_MS + (consecutiveBlocks * TURN_PER_BLOCK),
        TURN_BASE_MS, TURN_MAX_MS
      );
      if (turnDir == 1) driveRight(AUTO_SPEED_TURN);
      else              driveLeft(AUTO_SPEED_TURN);

      if (now - navTimer > turnDur) {
        driveStop();
        navState = NAV_VERIFY;
        navTimer = now;
        Serial.println("Turn done — verifying");
      }
      break;
    }

    case NAV_VERIFY: {
      driveStop();
      if (now - navTimer > 200) {
        long ahead = readDistance();
        Serial.print("Verify dist="); Serial.println(ahead);

        if (ahead > DIST_CLEAR) {
          consecutiveBlocks = 0;
          autoSpeed         = AUTO_SPEED_START;
          autoStartTime     = millis();
          navState          = NAV_MOVING;
          Serial.println("Clear — resuming");
        }
        else if (consecutiveBlocks >= 4) {
          consecutiveBlocks = 0;
          navState = NAV_REVERSE;
          navTimer = now;
          Serial.println("Stuck — forced reverse");
        }
        else {
          servoTo(SERVO_LEFT);
          navState = NAV_SCAN_LEFT;
          navTimer = now;
          Serial.println("Still blocked — re-scanning");
        }
      }
      break;
    }
  }
}

// ================================================================
//  MODE RESET
// ================================================================
void resetToManual() {
  driveStop();
  servoTo(SERVO_CENTER);
  activeCmd         = "S";
  lastCmdTime       = millis();
  navState          = NAV_MOVING;
  consecutiveBlocks = 0;
  currentMode       = MANUAL;
}

void resetToAuto() {
  driveStop();
  servoTo(SERVO_CENTER);
  navState          = NAV_MOVING;
  navTimer          = millis();
  autoSpeed         = AUTO_SPEED_START;
  autoStartTime     = millis();
  consecutiveBlocks = 0;
  currentMode       = AUTO_NAV;
}

// ================================================================
//  BLE COMMAND HANDLER
// ================================================================
void handleCommand(String cmd) {
  cmd.trim();
  lastCmdTime = millis();

  if (cmd == "MODE_MANUAL") { resetToManual(); return; }
  if (cmd == "MODE_AUTO")   { resetToAuto();   return; }
  if (cmd == "PLAY")        { isdPlay();        return; }
  if (cmd == "REC_START")   { isdRecordStart(); return; }
  if (cmd == "REC_STOP")    { isdRecordStop();  return; }
  if (cmd == "K")           { return; }

  if (cmd.startsWith("V")) {
    baseSpeed = constrain(cmd.substring(1).toInt(), 80, 255);
    return;
  }

  if (currentMode == MANUAL) {
    activeCmd = cmd;
  }
}

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  pinMode(ISD_PLAY_PIN, OUTPUT);
  pinMode(ISD_REC_PIN,  OUTPUT);
  digitalWrite(ISD_PLAY_PIN, LOW);
  digitalWrite(ISD_REC_PIN,  LOW);

  scanServo.attach(SERVO_PIN);
  delay(300);
  servoTo(SERVO_CENTER);
  delay(600);
  servoTo(SERVO_LEFT);
  delay(700);
  servoTo(SERVO_RIGHT);
  delay(700);
  servoTo(SERVO_CENTER);
  delay(500);
  Serial.println("Servo sweep done");

  setAllSpeed(0);
  driveStop();

  if (!BLE.begin()) {
    Serial.println("BLE failed");
    while (1);
  }
  BLE.setLocalName("RC Car");
  BLE.setAdvertisedService(rcService);
  rcService.addCharacteristic(cmdChar);
  BLE.addService(rcService);
  BLE.advertise();
  Serial.println("BLE ready");
}

// ================================================================
//  MAIN LOOP
// ================================================================
void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.println("Connected: " + central.address());
    resetToManual();

    while (central.connected()) {
      if (cmdChar.written()) {
        handleCommand(cmdChar.value());
      }
      if      (currentMode == MANUAL)   handleManual();
      else if (currentMode == AUTO_NAV) handleAuto();
    }

    resetToManual();
    Serial.println("Disconnected");
  }
}