// ============================================================
//  自走車端 — Arduino UNO + Sensor Shield V5（最終版）
//  HC-05 B（收握力器）：SoftwareSerial A1(RX) / A0(TX)
//  HC-05 C（傳燈牌）  ：SoftwareSerial A13(RX) / A11(TX)
// ============================================================
//  接線（Sensor Shield V5）：
//    IR 循跡左   → D2 的 S/V/G（直接插）
//    IR 循跡右   → D3 的 S/V/G（直接插）
//    超音波 TRIG → D4 的 S，VCC → D4 的 V，GND → D4 的 G
//    超音波 ECHO → D13 的 S
//    L298N IN1   → D8 的 S
//    L298N IN2   → D9 的 S
//    L298N ENA   → D10 的 S
//    L298N IN3   → D7 的 S
//    L298N IN4   → D6 的 S
//    L298N ENB   → D5 的 S
//    L298N GND   → 任意 G
//    HC-05 B VCC → A0 的 V
//    HC-05 B GND → A0 的 G
//    HC-05 B TX  → A1 的 S（直接接）
//    HC-05 B RX  → A0 的 S（串 1kΩ 電阻）
//    HC-05 C VCC → A11 的 V
//    HC-05 C GND → A11 的 G
//    HC-05 C TX  → A13 的 S（直接接）
//    HC-05 C RX  → A11 的 S（串 1kΩ 電阻）
//    左馬達      → L298N OUT1/OUT2
//    右馬達      → L298N OUT3/OUT4
//    7.4V 電池   → L298N 電源輸入
//    行動電源    → UNO USB 口
// ============================================================

#include <SoftwareSerial.h>

// ── HC-05 B（收握力器，Slave）──
SoftwareSerial btGrip(A1, A0);   // RX=A1, TX=A0

// ── HC-05 C（傳燈牌，Master）──
SoftwareSerial btSign(A13, A11); // RX=A13, TX=A11

// ── 接腳 ──
const byte LEFT1     = 8;
const byte LEFT2     = 9;
const byte LEFT_PWM  = 10;
const byte RIGHT1    = 7;
const byte RIGHT2    = 6;
const byte RIGHT_PWM = 5;
const byte IR_LEFT   = 2;
const byte IR_RIGHT  = 3;
const byte TRIG_PIN  = 4;
const byte ECHO_PIN  = 13;

// ── 霍爾速度閾值 ──
const int HALL_FAST = 800;
const int HALL_MID  = 600;
const int HALL_SLOW = 530;

// ── 馬達速度（三段）──
const byte SPD_FAST_L = 180;
const byte SPD_FAST_R = 195;
const byte SPD_MID_L  = 130;
const byte SPD_MID_R  = 143;
const byte SPD_SLOW_L = 95;
const byte SPD_SLOW_R = 108;
const byte SPD_TURN   = 90;

// ── 參數 ──
const int STOP_DISTANCE = 15;

// ── 狀態 ──
int    hallValue     = 0;
String btBuffer      = "";
byte   lastSentLevel = 255;
bool   obstacleHit   = false;

// ── 藍牙接收霍爾值（H<數值>\n）──
void readBluetooth() {
  btGrip.listen();
  while (btGrip.available()) {
    char c = btGrip.read();
    if (c == '\n') {
      if (btBuffer.startsWith("H")) {
        hallValue = btBuffer.substring(1).toInt();
      }
      btBuffer = "";
    } else {
      btBuffer += c;
      if (btBuffer.length() > 10) btBuffer = "";
    }
  }
}

// ── 傳速度段給燈牌 ──
void sendSpeedLevel(byte level) {
  if (level != lastSentLevel) {
    btSign.listen();
    btSign.write('0' + level);
    lastSentLevel = level;
    btGrip.listen();
  }
}

// ── 傳障礙物事件給 Leonardo ──
void sendObstacle() {
  btSign.listen();
  btSign.write('O');
  btGrip.listen();
}

// ── 馬達控制 ──
void motorForward(byte sL, byte sR) {
  digitalWrite(LEFT1,  LOW);  digitalWrite(LEFT2,  HIGH); analogWrite(LEFT_PWM,  sL);
  digitalWrite(RIGHT1, HIGH); digitalWrite(RIGHT2, LOW);  analogWrite(RIGHT_PWM, sR);
}

void motorTurnLeft(byte sL, byte sR) {
  digitalWrite(LEFT1,  LOW);  digitalWrite(LEFT2,  HIGH); analogWrite(LEFT_PWM,  SPD_TURN);
  digitalWrite(RIGHT1, HIGH); digitalWrite(RIGHT2, LOW);  analogWrite(RIGHT_PWM, sR);
}

void motorTurnRight(byte sL, byte sR) {
  digitalWrite(LEFT1,  LOW);  digitalWrite(LEFT2,  HIGH); analogWrite(LEFT_PWM,  sL);
  digitalWrite(RIGHT1, HIGH); digitalWrite(RIGHT2, LOW);  analogWrite(RIGHT_PWM, SPD_TURN);
}

void stopMotor() {
  analogWrite(LEFT_PWM, 0);
  analogWrite(RIGHT_PWM, 0);
}

// ── 超音波測距 ──
int getDistance() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur  = pulseIn(ECHO_PIN, HIGH, 60000);
  int  dist = dur * 0.034 / 2;
  if (dist == 0 || dist > 400) return 999;
  return dist;
}

// ── 速度段判斷 ──
byte getSpeedLevel(int val) {
  if (val > HALL_FAST) return 3;
  if (val > HALL_MID)  return 2;
  if (val > HALL_SLOW) return 1;
  return 0;
}

void getSpeedPWM(byte level, byte &sL, byte &sR) {
  switch (level) {
    case 3: sL = SPD_FAST_L; sR = SPD_FAST_R; break;
    case 2: sL = SPD_MID_L;  sR = SPD_MID_R;  break;
    case 1: sL = SPD_SLOW_L; sR = SPD_SLOW_R; break;
    default: sL = 0; sR = 0; break;
  }
}

// ============================================================
void setup() {
  pinMode(LEFT1, OUTPUT); pinMode(LEFT2, OUTPUT); pinMode(LEFT_PWM, OUTPUT);
  pinMode(RIGHT1, OUTPUT); pinMode(RIGHT2, OUTPUT); pinMode(RIGHT_PWM, OUTPUT);
  pinMode(IR_LEFT, INPUT); pinMode(IR_RIGHT, INPUT);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  stopMotor();
  btGrip.begin(9600);
  btSign.begin(9600);
  btGrip.listen();

  Serial.begin(9600);
  Serial.println("UNO Ready");
  sendSpeedLevel(0);
}

// ============================================================
void loop() {

  readBluetooth();

  // 障礙物偵測
  int distance = getDistance();
  if (distance > 0 && distance < STOP_DISTANCE && !obstacleHit) {
    obstacleHit = true;
    stopMotor();
    sendSpeedLevel(0);
    sendObstacle();
    Serial.println("⚠ 障礙物！停車");
    for (int i = 0; i < 6; i++) {
      digitalWrite(LED_BUILTIN, HIGH); delay(150);
      digitalWrite(LED_BUILTIN, LOW);  delay(120);
    }
    return;
  }

  if (obstacleHit) {
    stopMotor();
    sendSpeedLevel(0);
    return;
  }

  // 速度段判斷
  byte speedLevel = getSpeedLevel(hallValue);
  sendSpeedLevel(speedLevel);

  Serial.print("霍爾:"); Serial.print(hallValue);
  Serial.print(" 速度段:"); Serial.println(speedLevel);

  if (speedLevel == 0) {
    stopMotor();
    return;
  }

  // 馬達控制（IR 循跡）
  byte spdL, spdR;
  getSpeedPWM(speedLevel, spdL, spdR);

  int leftIR  = digitalRead(IR_LEFT);
  int rightIR = digitalRead(IR_RIGHT);

  if      (leftIR == HIGH && rightIR == LOW)  motorTurnLeft(spdL, spdR);
  else if (rightIR == HIGH && leftIR == LOW)  motorTurnRight(spdL, spdR);
  else                                         motorForward(spdL, spdR);

  delay(20);
}
