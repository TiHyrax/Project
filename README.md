# 握力成神計畫：互動式 AI 復健車

## 專案簡介
結合霍爾感測器、藍牙通訊與自走車的互動式手部復健裝置

## 硬體組成
- 握力器端：Arduino Leonardo + KY-024 + DFPlayer Mini + HC-05
- 自走車端：Arduino UNO + Sensor Shield V5 + L298N + HC-SR04 + IR循跡
- 燈牌端：ESP32 + ILI9488 3.5吋 TFT + HC-05

## 程式碼說明
- 握力器端_Leonardo.ino：霍爾感測、語音播放、藍牙傳送
- 自走車_UNO.ino：馬達控制、循跡避障、藍牙接收
- 燈牌端_ESP32.ino：TFT 圓燈顯示、藍牙接收
