#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

/* ======== WIFI CONFIG (Change these) ======== */
const char* ssid    = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

/* ======== TELEGRAM CONFIG (Change these) ======== */
const char* botToken = "YOUR_TELEGRAM_BOT_TOKEN";
const String ownerChatId = "YOUR_TELEGRAM_CHAT_ID";
const String openSecret = "CHANGE_ME"; 

WiFiClientSecure secured_client;
UniversalTelegramBot bot(botToken, secured_client);

/* ======== RFID PINS ======== */
#define SS_PIN    5
#define RST_PIN   4

MFRC522 rfid(SS_PIN, RST_PIN);

/* ======== SERVO ======== */
#define SERVO_PIN  15
Servo doorLock;

const int LOCK_ANGLE   = 180;
const int UNLOCK_ANGLE = 0;

bool doorLocked = true;
unsigned long lastRfidActionTime = 0;
const unsigned long rfidDebounceTime = 1000; // 1 second between RFID reads

/* ======== LEDs & BUZZER ======== */
#define LED_RED_PIN    25
#define LED_GREEN_PIN  26
#define BUZZER_PIN     27

/* ======== AUTHORIZED RFID UID ======== */
byte authorizedUID[] = {0x00, 0x00, 0x00, 0x00};   // replace with your card UID (printed on the Serial Monitor)
const byte authorizedSize = 4;

/* ======== TELEGRAM POLL ======== */
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 1000;

// Keyboard buttons
const String doorKeyboard =
  "[[\"🔓 Open\"],[\"🔒 Lock\"]]";

/* =================== FUNCTIONS PROTOTYPES =================== */
void notifyDoorStatus(const String& method, bool isLocked);
void setDoorStatus(bool locked);
void lockDoor(const String& method);
void unlockDoor(const String& method);

/* =================== BEEP & STATUS =================== */

void setDoorStatus(bool locked) {
  doorLocked = locked;
  if (locked) {
    digitalWrite(LED_RED_PIN, HIGH);
    digitalWrite(LED_GREEN_PIN, LOW);
  } else {
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_GREEN_PIN, HIGH);
  }
}

void beepPattern(int onMs, int offMs, int repeat) {
  for (int i = 0; i < repeat; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(onMs);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < repeat - 1) delay(offMs);
  }
}

void playUnlockSound() {
  beepPattern(80, 60, 2);
}

void playLockSound() {
  beepPattern(200, 0, 1);
}

/* =================== TELEGRAM NOTIFICATION =================== */

void notifyDoorStatus(const String& method, bool isLocked) {
  String status = isLocked ? F("🔒 Locked") : F("🔓 Unlocked");
  String msg = String(F("🚨 Door Status: ")) + status;
  msg += String(F("\nControlled via: ")) + method;
  bot.sendMessage(ownerChatId, msg, "");
}

/* =================== DOOR CONTROL =================== */

void lockDoor(const String& method) {
  if (doorLocked) return;
  
  doorLock.write(LOCK_ANGLE);
  setDoorStatus(true);
  playLockSound();
  Serial.println(String(F("Door locked by ")) + method + F("."));
  notifyDoorStatus(method, true);
}

void unlockDoor(const String& method) {
  if (!doorLocked) return;
  
  doorLock.write(UNLOCK_ANGLE);
  setDoorStatus(false);
  playUnlockSound();
  Serial.println(String(F("Door unlocked by ")) + method + F("."));
  notifyDoorStatus(method, false);
}

/* RFID Check and Toggle */
bool checkRFID() {
  if (millis() - lastRfidActionTime < rfidDebounceTime) {
    rfid.PICC_HaltA();
    return false;
  }

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return false;

  Serial.print(F("RFID UID: "));
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  if (rfid.uid.size != authorizedSize) {
    rfid.PICC_HaltA();
    return false;
  }

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] != authorizedUID[i]) {
      rfid.PICC_HaltA();
      return false;
    }
  }

  rfid.PICC_HaltA();
  lastRfidActionTime = millis();

  if (doorLocked) {
    Serial.println(F("[RFID] Access granted. Unlocking door..."));
    unlockDoor(F("RFID"));
  } else {
    Serial.println(F("[RFID] Locking door..."));
    lockDoor(F("RFID"));
  }

  return true;
}

/* =================== TELEGRAM HANDLER =================== */

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text    = bot.messages[i].text;
    String from    = bot.messages[i].from_name;

    Serial.println(String(F("Message from: ")) + from + String(F(" (")) + chat_id + F(")"));
    Serial.println(String(F("Text: ")) + text);

    if (chat_id != ownerChatId) {
      bot.sendMessage(chat_id, F("Access denied: You are not the owner."), "");
      continue;
    }

    if (text == "/start") {
      String msg = String(F("Welcome, ")) + from + F(" \n");
      msg += F("Use the buttons below or command /open <code>.\n");
      msg += String(F("Example: /open ")) + openSecret;
      bot.sendMessageWithReplyKeyboard(chat_id, msg, "", doorKeyboard, true);
    }
    else if (text == "🔓 Open") {
      if (!doorLocked) {
        bot.sendMessage(chat_id, F("Door is already unlocked."), "");
      } else {
        bot.sendMessage(chat_id, F("Unlocking door..."), "");
        Serial.println(F("[TG] Button Open pressed."));
        unlockDoor(F("Telegram Button"));
      }
    }
    else if (text == "🔒 Lock") {
      if (doorLocked) {
        bot.sendMessage(chat_id, F("Door is already locked."), "");
      } else {
        lockDoor(F("Telegram Button"));
        bot.sendMessage(chat_id, F("Door locked."), "");
        Serial.println(F("[TG] Button Lock pressed."));
      }
    }
    else if (text.startsWith("/open")) {
      int spaceIndex = text.indexOf(' ');
      if (spaceIndex == -1) {
        bot.sendMessage(chat_id, String(F("Please send: /open ")) + openSecret, "");
        continue;
      }
      String code = text.substring(spaceIndex + 1);
      code.trim();

      if (code == openSecret) {
        bot.sendMessage(chat_id, F("Access granted. Unlocking door..."), "");
        Serial.println(F("[TG] /open command OK."));
        unlockDoor(F("Telegram Code"));
      } else {
        bot.sendMessage(chat_id, F("Wrong code!"), "");
      }
    }
    else {
      bot.sendMessage(chat_id, F("Unknown command. Use the buttons or /open <code>."), "");
    }
  }
}

/* =================== SETUP =================== */

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  setDoorStatus(true); 

  doorLock.attach(SERVO_PIN);
  lockDoor(F("Initial State"));
  Serial.println(F("Door locked (initial)."));

  SPI.begin();
  rfid.PCD_Init();
  Serial.println(F("RFID init done."));

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print(F("Connecting to WiFi"));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F("\nWiFi connected!"));
  Serial.print(F("IP Address: "));
  Serial.println(WiFi.localIP());

  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  Serial.println(F("Telegram bot ready."));
  
  bot.sendMessage(ownerChatId, F("System Booted."), "");
}

/* =================== LOOP =================== */

void loop() {
  if (millis() - lastCheckTime > checkInterval) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastCheckTime = millis();
  }

  checkRFID();
}