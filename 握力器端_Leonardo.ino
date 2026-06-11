// ============================================================
//  握力器端 — Arduino Leonardo（最終版）
//  通訊：Serial1（D0/D1）→ HC-05 A Master → 自走車 UNO
//  DFPlayer：SoftwareSerial D10(RX) / D11(TX)
//  USB：接電腦，Serial 顯示霍爾數值
// ============================================================
//  接線：
//    KY-024 AO  → A0
//    KY-024 VCC → 麵包板 + 排（5V）
//    KY-024 GND → 麵包板 - 排（GND）
//    HC-05 A TX → D0（Serial1 RX，直接接）
//    HC-05 A RX → D1（Serial1 TX，串 1kΩ 電阻）
//    HC-05 A VCC → 麵包板 + 排
//    HC-05 A GND → 麵包板 - 排
//    DFPlayer TX → D10（直接接）
//    DFPlayer RX → D11（串 1kΩ 電阻）
//    DFPlayer VCC → 麵包板 + 排
//    DFPlayer GND → 麵包板 - 排
//    DFPlayer SPK_1 → 喇叭正極
//    DFPlayer SPK_2 → 喇叭負極
//    麵包板 + 排 → Leonardo 5V
//    麵包板 - 排 → Leonardo GND
//    USB → 電腦（供電 + 顯示霍爾數值）
// ============================================================
//  SD 卡（FAT32，建立 mp3 資料夾）：
//    mp3/0001.mp3 → 前臂肌群成神系統已啟動…
//    mp3/0002.mp3 → 加速中！繼續保持…
//    mp3/0003.mp3 → 哎呀，您累了嗎？…
//    mp3/0004.mp3 → 您已連續努力 20 秒…
//    mp3/0005.mp3 → 偵測到前方障礙物！…
//    mp3/0006.mp3 → 隨時可以再握…
// ============================================================

#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ── DFPlayer（SoftwareSerial D10 RX / D11 TX）──
SoftwareSerial dfSerial(10, 11);
DFRobotDFPlayerMini dfPlayer;

// ── 接腳 ──
const byte HALL_PIN   = A0;
const byte LED_STATUS = 13;

// ── 霍爾閾值 ──
const int HALL_FAST = 800;
const int HALL_MID  = 600;
const int HALL_SLOW = 530;

// ── 語音檔編號 ──
const byte SFX_START    = 1;
const byte SFX_FASTER   = 2;
const byte SFX_SLOWER   = 3;
const byte SFX_FATIGUE  = 4;
const byte SFX_OBSTACLE = 5;
const byte SFX_RELEASE  = 6;

// ── 計時 ──
const unsigned long FATIGUE_TIME   = 20000UL;  // 20 秒疲勞保護
const unsigned long START_DURATION = 2000UL;   // 持握 2 秒啟動
const unsigned long PRINT_INTERVAL = 100UL;    // 霍爾數值顯示間隔

// ── 狀態 ──
bool systemActive   = false;
bool systemFinished = false;
bool isHallHigh     = false;

unsigned long hallHighStartTime  = 0;
unsigned long hallActiveStart    = 0;
unsigned long hallActiveDuration = 0;
unsigned long lastPrintTime      = 0;

bool fatiguePlayed = false;
bool releasePlayed = false;

byte lastSpeedLevel = 255;
byte lastSentLevel  = 255;

// ── 語音播放 ──
void playVoice(byte track) {
  dfPlayer.play(track);
  switch (track) {
    case SFX_START:    delay(3000); break;
    case SFX_FASTER:   delay(2500); break;
    case SFX_SLOWER:   delay(3000); break;
    case SFX_FATIGUE:  delay(4000); break;
    case SFX_OBSTACLE: delay(3000); break;
    case SFX_RELEASE:  delay(2500); break;
    default:           delay(2500); break;
  }
}

// ── 傳送速度段給 UNO ──
void sendSpeedLevel(byte level) {
  if (level != lastSentLevel) {
    Serial1.write('0' + level);
    lastSentLevel = level;
  }
}

// ── 霍爾值 → 速度段 ──
byte getSpeedLevel(int val) {
  if (val > HALL_FAST) return 3;
  if (val > HALL_MID)  return 2;
  if (val > HALL_SLOW) return 1;
  return 0;
}

// ── 霍爾值視覺化顯示 ──
void printHallValue(int val) {
  unsigned long now = millis();
  if (now - lastPrintTime < PRINT_INTERVAL) return;
  lastPrintTime = now;

  String bar = "[";
  int barLen = 0;
  String speedText;

  if (val > 800)      { barLen = 20; speedText = "快速 <<<"; }
  else if (val > 600) { barLen = 13; speedText = "中速 <<";  }
  else if (val > 530) { barLen = 7;  speedText = "慢速 <";   }
  else                { barLen = 0;  speedText = "停止";     }

  for (int i = 0; i < 20; i++) bar += (i < barLen) ? "#" : "-";
  bar += "]";

  Serial.print("霍爾值："); Serial.print(val);
  Serial.print("\t"); Serial.print(bar);
  Serial.print("\t"); Serial.println(speedText);
}

// ============================================================
void setup() {
  Serial1.begin(9600);   // HC-05 藍牙
  Serial.begin(9600);    // USB 除錯

  pinMode(HALL_PIN,   INPUT);
  pinMode(LED_STATUS, OUTPUT);

  dfSerial.begin(9600);
  delay(1000);
  if (!dfPlayer.begin(dfSerial)) {
    Serial.println("DFPlayer 初始化失敗！請檢查接線和SD卡");
    for (int i = 0; i < 20; i++) {
      digitalWrite(LED_STATUS, HIGH); delay(80);
      digitalWrite(LED_STATUS, LOW);  delay(80);
    }
  } else {
    Serial.println("DFPlayer 正常！");
  }
  dfPlayer.volume(28);

  sendSpeedLevel(0);
  Serial.println("========================================");
  Serial.println("  Leonardo Ready！");
  Serial.println("  持握 2 秒啟動系統");
  Serial.println("  速度：>800=快速 >600=中速 >530=慢速");
  Serial.println("========================================");
}

// ============================================================
void loop() {
  int hallValue = analogRead(HALL_PIN);

  // 顯示霍爾數值
  printHallValue(hallValue);

  // 接收 UNO 回傳事件（障礙物 'O'）
  if (Serial1.available()) {
    char msg = Serial1.read();
    if (msg == 'O') {
      playVoice(SFX_OBSTACLE);
      systemFinished = true;
      Serial.println("⚠ 障礙物偵測！系統結束");
    }
  }

  // ── 階段 1：等待啟動 ──
  if (!systemActive && !systemFinished) {
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 800) {
      digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
      lastBlink = millis();
    }

    if (hallValue > HALL_MID) {
      if (!isHallHigh) {
        isHallHigh        = true;
        hallHighStartTime = millis();
        Serial.println(">>> 偵測到握力，開始計時...");
      } else if (millis() - hallHighStartTime >= START_DURATION) {
        systemActive    = true;
        hallActiveStart = millis();
        fatiguePlayed   = false;
        releasePlayed   = false;
        lastSpeedLevel  = 0;
        sendSpeedLevel(0);
        playVoice(SFX_START);
        digitalWrite(LED_STATUS, HIGH);
        Serial.println("✓ 系統啟動！");
      }
    } else {
      if (isHallHigh) {
        isHallHigh = false;
        Serial.println("✗ 握力中斷，重新計時");
      }
      digitalWrite(LED_STATUS, LOW);
    }

    sendSpeedLevel(0);
    delay(50);
    return;
  }

  // ── 階段 2：運行中 ──
  if (systemActive && !systemFinished) {
    byte speedLevel = getSpeedLevel(hallValue);

    if (speedLevel > 0) {
      if (hallActiveStart == 0) hallActiveStart = millis();
      hallActiveDuration = millis() - hallActiveStart;

      // 疲勞保護
      if (hallActiveDuration >= FATIGUE_TIME && !fatiguePlayed) {
        fatiguePlayed = true;
        sendSpeedLevel(0);
        playVoice(SFX_FATIGUE);
        hallActiveStart    = millis();
        hallActiveDuration = 0;
        Serial.println("⚠ 疲勞警告！休息提醒播放");
      }

      // 加速語音
      if (lastSpeedLevel != 255 && speedLevel > lastSpeedLevel && lastSpeedLevel != 0) {
        playVoice(SFX_FASTER);
        Serial.println("↑ 加速！播放鼓勵語音");
      }

      // 減速語音
      if (lastSpeedLevel != 255 && speedLevel < lastSpeedLevel && speedLevel > 0) {
        playVoice(SFX_SLOWER);
        Serial.println("↓ 減速！播放安慰語音");
      }

      releasePlayed  = false;
      lastSpeedLevel = speedLevel;

    } else {
      if (!releasePlayed) {
        releasePlayed = true;
        sendSpeedLevel(0);
        playVoice(SFX_RELEASE);
        Serial.println("○ 放開握力，播放放開語音");
      }
      hallActiveStart    = 0;
      hallActiveDuration = 0;
      fatiguePlayed      = false;
      lastSpeedLevel     = 0;
      sendSpeedLevel(0);
      delay(50);
      return;
    }

    sendSpeedLevel(speedLevel);
  }

  // ── 階段 3：系統已結束 ──
  if (systemFinished) {
    sendSpeedLevel(0);
    static unsigned long lastBlink2 = 0;
    if (millis() - lastBlink2 > 300) {
      digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
      lastBlink2 = millis();
    }
  }

  delay(50);
}
