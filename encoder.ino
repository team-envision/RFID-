#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>

//-----------------------------------------
#define RST_PIN  D3
#define SS_PIN   D4
#define BUZZER   D8
//-----------------------------------------
MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;  
MFRC522::StatusCode status;      
//-----------------------------------------
String formattedValue = "";  
//-----------------------------------------
String sheet_url = "https://script.google.com/macros/s/AKfycbyCQuydtg8SVamhz9TO0cLjEYNBOPBKNC1XmZFkpZM2btEZxuwAG3ewf58ZNBAUGvLAFQ/exec?name=";  
//-----------------------------------------
#define WIFI_SSID "Rewant"  
#define WIFI_PASSWORD "12345678"  
//-----------------------------------------
LiquidCrystal_PCF8574 lcd(0x27);  // Use correct I2C address

/****************************************************************************************************
 * URL encoding function to ensure valid URL format
 ****************************************************************************************************/
String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

/****************************************************************************************************
 * Function to read RFID data and format it
 ****************************************************************************************************/
void readAndFormatRFIDData() {
  byte block;
  byte len;
  
  // Prepare key
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  
  byte buffer1[18];
  block = 1;
  len = 18;
  
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  
  status = mfrc522.MIFARE_Read(block, buffer1, &len);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Reading failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  
  String value = "";
  for (uint8_t i = 0; i < 8; i++) {
    value += (char)buffer1[i];
  }
  value.trim();

  byte buffer2[18];
  block = 2;
  len = 18;

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  status = mfrc522.MIFARE_Read(block, buffer2, &len);
  
  String valuet = "";
  for (uint8_t i = 0; i < 7; i++) {
    valuet += (char)buffer2[i];
  }
  valuet.trim();

  // Format the final value
  formattedValue = value + valuet;
  
  Serial.println(F("RFID Data formatted successfully"));
}

/****************************************************************************************************
 * setup() function
 ****************************************************************************************************/
void setup() {
  Serial.begin(115200);

  Wire.begin(D2, D1);  // Ensure correct SDA and SCL connections
  
  lcd.begin(16, 2);    // Initialize LCD
  lcd.setBacklight(255);  // Ensure the backlight is turned on
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TEAM ENVISION");
  delay(2000);  // Display for 2 seconds

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Scan your Card ");

  Serial.print("Connecting to AP");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
  }
  Serial.println();
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  pinMode(BUZZER, OUTPUT);
  SPI.begin();
}

/****************************************************************************************************
 * loop() function
 ****************************************************************************************************/
void loop() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Scan your Card ");
  
  mfrc522.PCD_Init();

  if (!mfrc522.PICC_IsNewCardPresent()) { return; }
  if (!mfrc522.PICC_ReadCardSerial()) { return; }

  Serial.println();
  Serial.println(F("Reading data from RFID..."));
  readAndFormatRFIDData();

  Serial.println("Formatted Value: " + formattedValue);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hello!");
  lcd.setCursor(0, 1);
  lcd.print(formattedValue);

  digitalWrite(BUZZER, HIGH);
  delay(200);
  digitalWrite(BUZZER, LOW);
  delay(200);

  if (WiFi.status() == WL_CONNECTED) {
    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    client->setInsecure();
    
    String encoded_fullra = urlEncode(formattedValue);  
    String sheet_url_with_data = sheet_url + encoded_fullra;
    sheet_url_with_data.trim();
    Serial.println("Final URL: " + sheet_url_with_data);  

    HTTPClient https;
    if (https.begin(*client, sheet_url_with_data)) {
      int httpCode = https.GET();
      
      if (httpCode > 0) {
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        String payload = https.getString();
        Serial.println("Response: " + payload);

        if (httpCode == 200) {
          lcd.setCursor(0, 1);  // Only update the second row with the status
          lcd.print("Data Recorded ");
        } 
        delay(500);
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();
    } else {
      Serial.printf("[HTTPS] Unable to connect\n");
    }
  } else {
    Serial.println("WiFi not connected!");
    lcd.setCursor(0, 1);  // Only update the second row with the WiFi failure message
    lcd.print("WiFi Failed   ");
    delay(1000);
  }
  delay(1000);
}
