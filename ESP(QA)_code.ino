// Smart Attendance System — RFID + AWS API Gateway
// Based on original sketch by Viral Science
// Modified for AWS Lambda API (POST JSON instead of Google Sheets GET)

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
// ── CHANGED: URL does NOT include regNo — it goes in the JSON body
const String API_URL = "https://e3q8is1xck.execute-api.ap-south-1.amazonaws.com/tap";

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
    delay(300);
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
  delay(1500);

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

  // Halt card immediately after serial read
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

  digitalWrite(BUZZER, HIGH); delay(100);
  digitalWrite(BUZZER, LOW);

  lcdMsg("   Uploading... ", fullRegNo);

  // ── Upload to AWS API Gateway ─────────────────────────────
  if (WiFi.status() == WL_CONNECTED) {

    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();

    // ── CHANGED: Use the base URL directly — don't append regNo to it
    Serial.println("Sending POST to: " + API_URL);

    HTTPClient https;
    if (https.begin(*client, API_URL)) {

      // ── CHANGED: Add Content-Type header for JSON
      https.addHeader("Content-Type", "application/json");

      https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      https.setTimeout(8000);

      // ── CHANGED: Build JSON body and use POST instead of GET
      String jsonPayload = "{\"regNo\":\"" + fullRegNo + "\"}";
      Serial.println("Payload: " + jsonPayload);

      int httpCode = https.POST(jsonPayload);
      Serial.print("[HTTPS] Response code: ");
      Serial.println(httpCode);

      if (httpCode > 0) {
        String response = https.getString();
        Serial.println("Server response: " + response);

        https.end();
        client.reset();              // free BearSSL memory before LCD writes
        delay(50);
        Wire.begin(D2, D1);          // re-init I2C — BearSSL can disrupt it
        lcd.init();
        lcd.backlight();

        // Show result on LCD based on response
        if (httpCode == 200) {
          // Check if action is "in" or "out" from response
          if (response.indexOf("\"in\"") > 0) {
            lcdMsg("   Marked IN!   ", fullRegNo);
          } else {
            lcdMsg("  Checked OUT!  ", fullRegNo);
          }
        } else if (httpCode == 404) {
          lcdMsg(" Not Registered ", fullRegNo);
        } else {
          lcdMsg("  Server Error  ", fullRegNo);
        }
        delay(800);
      }
      else {
        Serial.print("[HTTPS] Error: ");
        Serial.println(https.errorToString(httpCode));

        https.end();
        client.reset();
        delay(50);
        Wire.begin(D2, D1);
        lcd.init();
        lcd.backlight();

        lcdMsg("  Upload Failed ", fullRegNo);
        delay(800);
      }
    }
    else {
      Serial.println("[HTTPS] Unable to connect");
      client.reset();
      delay(50);
      Wire.begin(D2, D1);
      lcd.init();
      lcd.backlight();
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

  delay(200);
}
