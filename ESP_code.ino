// Viral Science www.viralsciencecreativity.com www.youtube.com/c/viralscience
// Smart Attendance System with Google Sheets and LCD Display
#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <LiquidCrystal_I2C.h>
//-----------------------------------------
#define RST_PIN  D3
#define SS_PIN   D4
#define BUZZER   D8
//-----------------------------------------
MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;
//-----------------------------------------
byte bufferLen = 18;
byte readBlockData[18];
//-----------------------------------------
const String sheet_url = "https://script.google.com/macros/s/AKfycbxgZYxpewmrrssZx-_9Y29oIeAtcqd2TpUkSwsjBW6Oo2QXwcL6CXu7o8ClWBlKoOFd3g/exec?REGNO=";

#define WIFI_SSID "Envision"
#define WIFI_PASSWORD "Envision"
//-----------------------------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);


/****************************************************************************************************
 * lcdMsg() — prints two rows cleanly in one call
 ****************************************************************************************************/
void lcdMsg(String row0, String row1) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(row0);
  lcd.setCursor(0, 1);
  lcd.print(row1);
}


/****************************************************************************************************
 * CleanString() — strips null bytes and non-printable chars from RFID block data
 ****************************************************************************************************/
String CleanString(byte* data, int len) {
  String result = "";
  for (int i = 0; i < len; i++) {
    if (data[i] == 0x00) break;
    if (data[i] >= 0x20 && data[i] < 0x7F) {
      result += (char)data[i];
    }
  }
  result.trim();
  return result;
}


/****************************************************************************************************
 * ReadBlock() — authenticates and reads a single block, returns false on failure
 ****************************************************************************************************/
bool ReadBlock(int blockNum, byte blockData[]) {
  bufferLen = 18;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid)
  );
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Auth failed block ");
    Serial.print(blockNum);
    Serial.print(": ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  status = mfrc522.MIFARE_Read(blockNum, blockData, &bufferLen);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Read failed block ");
    Serial.print(blockNum);
    Serial.print(": ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  return true;
}


/****************************************************************************************************
 * setup() function
 ****************************************************************************************************/
void setup() {
  Serial.begin(9600);

  Wire.begin(D2, D1);   // SDA = D2, SCL = D1 — mandatory on ESP8266

  lcd.init();
  lcd.backlight();
  lcdMsg("  Initializing  ", "                ");
  for (int a = 5; a <= 10; a++) {
    lcd.setCursor(a, 1);
    lcd.print(".");
    delay(300);   // Reduced from 500ms — initializing animation faster
  }

  Serial.println();
  Serial.print("Connecting to AP");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" WiFi Connected ");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(1500);   // Reduced from 2000ms

  pinMode(BUZZER, OUTPUT);
  SPI.begin();
}


/****************************************************************************************************
 * loop() function
 ****************************************************************************************************/
void loop() {

  // ── SCREEN 1: Ready to scan ──────────────────────────────
  lcdMsg(" Scan your Card ", "                ");

  mfrc522.PCD_Init();
  if (!mfrc522.PICC_IsNewCardPresent()) { return; }
  if (!mfrc522.PICC_ReadCardSerial())   { return; }

  Serial.println("\nCard detected.");

  // Halt card immediately after serial read — prevents re-triggering the same card
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  // ── SCREEN 2: Reading card ────────────────────────────────
  lcdMsg("  Reading Card  ", "  Please wait.. ");

  // Re-init RFID to read blocks after halt
  mfrc522.PCD_Init();
  if (!mfrc522.PICC_IsNewCardPresent()) {
    lcdMsg("  Read Failed!  ", "  Try Again...  ");
    delay(800);
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    lcdMsg("  Read Failed!  ", "  Try Again...  ");
    delay(800);
    return;
  }

  // Read Block 1 — first part of reg no (e.g. "RA2311054")
  byte block1Data[18];
  if (!ReadBlock(1, block1Data)) {
    lcdMsg("  Read Failed!  ", "  Try Again...  ");
    delay(800);
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }
  String part1 = CleanString(block1Data, 8);

  // Read Block 2 — second part of reg no (e.g. "4010030")
  byte block2Data[18];
  if (!ReadBlock(2, block2Data)) {
    lcdMsg("  Read Failed!  ", "  Try Again...  ");
    delay(800);
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }
  String part2 = CleanString(block2Data, 7);

  // Halt card now that we're done reading
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  // Combine both parts
  String fullRegNo = part1 + part2;
  Serial.print("REG_NO: ");
  Serial.println(fullRegNo);

  // ── SCREEN 3: Show reg no + single short beep ─────────────
  lcdMsg("    REG  NO:    ", fullRegNo);

  // OPTIMIZED: Single short beep instead of double (saves 400ms)
  digitalWrite(BUZZER, HIGH); delay(100);
  digitalWrite(BUZZER, LOW);

  // OPTIMIZED: No pre-upload delay — go straight to upload
  // Screen updates to show uploading status
  lcdMsg("   Uploading... ", fullRegNo);

  // ── Upload to Google Sheets ───────────────────────────────
  if (WiFi.status() == WL_CONNECTED) {

    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();

    String fullURL = sheet_url + fullRegNo;
    Serial.println("Sending to: " + fullURL);

    HTTPClient https;
    if (https.begin(*client, fullURL)) {

      https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      https.setTimeout(8000);   // OPTIMIZED: Reduced from 15s — fail faster if no response

      int httpCode = https.GET();
      Serial.print("[HTTPS] Response code: ");
      Serial.println(httpCode);

      if (httpCode > 0) {
        String response = https.getString();
        Serial.println("Server response: " + response);

        // OPTIMIZED: 800ms display instead of 2000ms — just enough to read
        lcdMsg(" Data Recorded! ", fullRegNo);
        delay(800);
      }
      else {
        Serial.print("[HTTPS] Error: ");
        Serial.println(https.errorToString(httpCode));

        // OPTIMIZED: 800ms display instead of 2000ms
        lcdMsg("  Upload Failed ", fullRegNo);
        delay(800);
      }

      https.end();
    }
    else {
      Serial.println("[HTTPS] Unable to connect");
      lcdMsg(" Connect Failed ", fullRegNo);
      delay(800);
    }
  }
  else {
    Serial.println("WiFi lost — reconnecting...");
    lcdMsg("   No  WiFi!    ", " Reconnecting.. ");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(3000);
  }

  // OPTIMIZED: 200ms cooldown — just enough to prevent ghost reads
  delay(200);
}
