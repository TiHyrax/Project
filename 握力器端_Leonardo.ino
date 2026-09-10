// ============================================================
//  握力成神系統 — Arduino Leonardo
//  通訊：Serial1（D0/D1）→ HC-05 A Master → 自走車 UNO
//  DFPlayer：SoftwareSerial D10(RX) / D11(TX)
//  USB：接電腦，Serial 顯示霍爾數值
// ============================================================
//  SD 卡（FAT32，mp3 資料夾）：
//    0001.mp3 → 系統啟動
//    0002.mp3 → 加速
//    0003.mp3 → 減速
//    0004.mp3 → 疲勞警告
//    0005.mp3 → 障礙物
//    0006.mp3 → 放開握力
// ============================================================

#include <SoftwareSerial.h>
#include <DFPlayerMini_Fast.h>

SoftwareSerial dfSerial(10, 11);
DFPlayerMini_Fast dfPlayer;

// ── 接腳 ──
const byte HALL_PIN   = A0;
const byte LED_STATUS = 13;

// ── 霍爾閾值（依實測）──
const int HALL_FAST    = 680;   // > 680 快速
const int HALL_MID     = 528;   // > 528 中速
const int HALL_SLOW    = 518;   // > 518 慢速
const int HALL_STARTUP = 518;   // 啟動閾值（獨立，不影響放開判斷）
const int HALL_RELEASE = 200;   // < 200 才算真正放開（靜止約 105）

// ── 語音檔編號 ──
const byte SFX_START    = 1;
const byte SFX_FASTER   = 2;
const byte SFX_SLOWER   = 3;
const byte SFX_FATIGUE  = 4;
const byte SFX_OBSTACLE = 5;
const byte SFX_RELEASE  = 6;

const unsigned long SFX_DURATION[] = {
  0, 3000, 2500, 3000, 4000, 3000, 2500
};

// ── 計時常數 ──
const unsigned long FATIGUE_TIME   = 20000UL;
const unsigned long START_DURATION = 2000UL;
const unsigned long PRINT_INTERVAL = 100UL;

// ── 狀態機 ──
enum SystemState { STATE_WAIT, STATE_ACTIVE, STATE_FINISHED };
SystemState sysState = STATE_WAIT;

bool systemActive = false;

// ── 語音（非阻塞）──
bool          voicePlaying = false;
unsigned long voiceEndTime = 0;
byte          voiceQueue   = 0;

// ── 啟動偵測 ──
bool          isHallHigh        = false;
unsigned long hallHighStartTime = 0;

// ── 疲勞計時 ──
bool          fatiguePlayed  = false;
unsigned long hallActiveStart = 0;

// ── 放開提示（true = 已播，等下次握力才重置）──
bool releasePlayed    = false;
bool hasGrippedOnce  = false;  // 啟動後至少握過一次

// ── 速度段追蹤 ──
byte lastSpeedLevel = 255;
byte lastSentLevel  = 255;

unsigned long lastPrintTime = 0;
unsigned long lastBlinkTime = 0;

// ============================================================
void requestVoice(byte track, bool urgent = false) {
  if (urgent) {
    dfPlayer.stop();
    delay(30);
    dfPlayer.play(track);
    voicePlaying = true;
    voiceEndTime = millis() + SFX_DURATION[track];
    voiceQueue   = 0;
    Serial.print(F("[語音-強插] 軌道 ")); Serial.println(track);
  } else if (!voicePlaying) {
    dfPlayer.play(track);
    voicePlaying = true;
    voiceEndTime = millis() + SFX_DURATION[track];
    Serial.print(F("[語音] 軌道 ")); Serial.println(track);
  } else {
    if (voiceQueue == 0) voiceQueue = track;
  }
}

void updateVoice() {
  if (voicePlaying && millis() >= voiceEndTime) {
    voicePlaying = false;
    if (voiceQueue != 0) {
      byte next = voiceQueue;
      voiceQueue = 0;
      requestVoice(next);
    }
  }
}

void sendSpeedLevel(byte level) {
  if (level != lastSentLevel) {
    Serial1.write('0' + level);
    lastSentLevel = level;
  }
}

byte getSpeedLevel(int val) {
  if (val > HALL_FAST) return 3;
  if (val > HALL_MID)  return 2;
  if (val > HALL_SLOW) return 1;
  return 0;
}

void printHallValue(int val) {
  if (millis() - lastPrintTime < PRINT_INTERVAL) return;
  lastPrintTime = millis();

  String bar = "[";
  int barLen = 0;
  const char* speedText;

  if      (val > HALL_FAST) { barLen = 20; speedText = "快速 <<<"; }
  else if (val > HALL_MID)  { barLen = 13; speedText = "中速 <<";  }
  else if (val > HALL_SLOW) { barLen = 7;  speedText = "慢速 <";   }
  else                      { barLen = 0;  speedText = "停止";     }

  for (int i = 0; i < 20; i++) bar += (i < barLen) ? '#' : '-';
  bar += "]";

  Serial.print(F("霍爾值：")); Serial.print(val);
  Serial.print('\t'); Serial.print(bar);
  Serial.print('\t'); Serial.println(speedText);
}

void blinkLED(unsigned long interval) {
  if (millis() - lastBlinkTime >= interval) {
    lastBlinkTime = millis();
    digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
  }
}

// ============================================================
void setup() {
  Serial1.begin(9600);
  Serial.begin(9600);
  while (!Serial);

  pinMode(HALL_PIN,   INPUT);
  pinMode(LED_STATUS, OUTPUT);

  dfSerial.begin(9600);
  delay(1000);

  if (!dfPlayer.begin(dfSerial)) {
    Serial.println(F("DFPlayer 初始化失敗！"));
    for (int i = 0; i < 20; i++) {
      digitalWrite(LED_STATUS, HIGH); delay(80);
      digitalWrite(LED_STATUS, LOW);  delay(80);
    }
  } else {
    Serial.println(F("DFPlayer 正常！"));
  }
  dfPlayer.volume(28);

  lastSentLevel = 255;
  sendSpeedLevel(0);

  Serial.println(F("========================================"));
  Serial.println(F("  Leonardo Ready！持握 2 秒啟動系統"));
  Serial.println(F("========================================"));
}

// ============================================================
void loop() {
  int hallValue = analogRead(HALL_PIN);

  updateVoice();
  printHallValue(hallValue);

  // ── 障礙物訊號 ──
  if (Serial1.available()) {
    char msg = Serial1.read();
    if (msg == 'O' && sysState != STATE_FINISHED) {
      sendSpeedLevel(0);
      requestVoice(SFX_OBSTACLE, true);
      sysState     = STATE_FINISHED;
      systemActive = false;
      Serial.println(F("⚠ 障礙物！系統結束"));
    }
  }

  switch (sysState) {

    // ── 等待啟動 ──
    case STATE_WAIT: {
      blinkLED(800);
      sendSpeedLevel(0);

      if (hallValue > HALL_STARTUP) {
        if (!isHallHigh) {
          isHallHigh        = true;
          hallHighStartTime = millis();
          Serial.println(F(">>> 偵測到握力，計時中..."));
        } else if (millis() - hallHighStartTime >= START_DURATION) {
          sysState        = STATE_ACTIVE;
          systemActive    = true;
          hasGrippedOnce  = false;
          hallActiveStart = millis();
          fatiguePlayed   = false;
          releasePlayed   = false;
          lastSentLevel   = 255;
          lastSpeedLevel  = 255;
          sendSpeedLevel(0);
          requestVoice(SFX_START);
          digitalWrite(LED_STATUS, HIGH);
          Serial.println(F("✓ 系統啟動！"));
        }
      } else {
        if (isHallHigh) {
          isHallHigh = false;
          Serial.println(F("✗ 握力中斷，重新計時"));
        }
        digitalWrite(LED_STATUS, LOW);
      }
      break;
    }

    // ── 運行中 ──
    case STATE_ACTIVE: {
      byte speedLevel = getSpeedLevel(hallValue);

      if (speedLevel > 0) {
        // 有握力
        hasGrippedOnce  = true;
        releasePlayed   = false;
        if (hallActiveStart == 0) hallActiveStart = millis();
        unsigned long activeDuration = millis() - hallActiveStart;

        // 疲勞保護
        if (activeDuration >= FATIGUE_TIME && !fatiguePlayed) {
          fatiguePlayed   = true;
          sendSpeedLevel(0);
          requestVoice(SFX_FATIGUE, true);
          hallActiveStart = millis();
          Serial.println(F("⚠ 疲勞警告！"));
        }

        // 加速語音
        if (!voicePlaying &&
            lastSpeedLevel != 255 &&
            lastSpeedLevel != 0 &&
            speedLevel > lastSpeedLevel) {
          requestVoice(SFX_FASTER);
          Serial.println(F("↑ 加速！"));
        }

        // 減速語音
        if (!voicePlaying &&
            lastSpeedLevel != 255 &&
            speedLevel < lastSpeedLevel &&
            speedLevel > 0) {
          requestVoice(SFX_SLOWER);
          Serial.println(F("↓ 減速！"));
        }

        lastSpeedLevel = speedLevel;
        sendSpeedLevel(speedLevel);

      } else {
        // 放開握力
        // 【關鍵修正】用 HALL_RELEASE 判斷真正放開（靜止約 105）
        // 且必須 hasGrippedOnce 才播，避免啟動後尚未握就觸發
        if (hallValue < HALL_RELEASE && !releasePlayed && hasGrippedOnce) {
          releasePlayed = true;
          sendSpeedLevel(0);
          requestVoice(SFX_RELEASE);
          Serial.println(F("○ 放開握力"));
        }
        hallActiveStart = 0;
        fatiguePlayed   = false;
        lastSpeedLevel  = 0;
        sendSpeedLevel(0);
      }
      break;
    }

    // ── 系統結束 ── 長按 5 秒重啟
    case STATE_FINISHED: {
      sendSpeedLevel(0);
      blinkLED(300);

      if (hallValue > HALL_STARTUP) {
        if (!isHallHigh) {
          isHallHigh        = true;
          hallHighStartTime = millis();
        } else if (millis() - hallHighStartTime >= 5000UL) {
          sysState       = STATE_WAIT;
          systemActive   = false;
          isHallHigh     = false;
          lastSentLevel  = 255;
          lastSpeedLevel = 255;
          sendSpeedLevel(0);
          Serial.println(F("↺ 系統重啟"));
        }
      } else {
        isHallHigh = false;
      }
      break;
    }
  }
}
