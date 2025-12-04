#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <time.h>

// ====== إعدادات WiFi Manager ======
WiFiManager wifiManager;

// ====== إعدادات n8n ======
String n8nWebhookURL = ""; // سيتم تعيينه من WiFi Manager
#define EEPROM_SIZE 512
#define WEBHOOK_ADDRESS 0

// ====== إعدادات زر Reset WiFi ======
#define RESET_BUTTON D3  // GPIO0 (زر Flash في NodeMCU)

// ====== إعدادات RFID ======
#define SS_PIN D8      // SDA pin (GPIO15)
#define RST_PIN D0     // RST pin (GPIO16)
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ====== إعدادات LED و Buzzer (مبسطة) ======
#define LED_GREEN D1    // GPIO5 - لمبة خضراء (نجاح)
#define LED_RED D2      // GPIO4 - لمبة حمراء (رفض/خطأ)
#define BUZZER_PIN D4   // GPIO2 - بازر

// ====== متغيرات عامة ======
String lastScannedUID = "";
unsigned long lastScanTime = 0;
const unsigned long SCAN_DELAY = 3000; // 3 ثواني بين كل مسح

// ====== إعدادات الوقت ======
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;  // +2 ساعة (مصر)
const int daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== نظام الحضور RFID - ESP8266 ===");
  
  // تهيئة EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // تهيئة المخارج
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RESET_BUTTON, INPUT_PULLUP);
  
  // إطفاء جميع الأضواء
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);
  
  // تهيئة SPI و RFID
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("✅ RFID Reader تم تهيئة");
  
  // صوت بداية التشغيل
  beep(100);
  delay(100);
  beep(100);
  
  // قراءة webhook من EEPROM
  n8nWebhookURL = readWebhookFromEEPROM();
  if (n8nWebhookURL.length() > 0) {
    Serial.println("📋 تم تحميل webhook من الذاكرة");
    Serial.println("🔗 Webhook: " + n8nWebhookURL);
  }
  
  // الاتصال بالواي فاي
  connectWiFi();
  
  // مزامنة الوقت من الإنترنت
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("⏰ جاري مزامنة الوقت...");
  
  // انتظار حتى يتم الحصول على الوقت
  int retries = 0;
  while (time(nullptr) < 1000000000 && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  Serial.println();
  
  if (time(nullptr) > 1000000000) {
    Serial.println("✅ تم مزامنة الوقت بنجاح");
    printLocalTime();
  } else {
    Serial.println("⚠️ فشلت مزامنة الوقت - سيتم استخدام millis()");
  }
  
  // صوت الجاهزية
  beep(100);
  delay(100);
  beep(100);
  delay(100);
  beep(100);
  
  Serial.println("✅ النظام جاهز للمسح");
  blinkLED(LED_GREEN, 500);
}

void loop() {
  // التحقق من الضغط على زر Reset (ضغط طويل 3 ثواني)
  if (digitalRead(RESET_BUTTON) == LOW) {
    unsigned long pressTime = millis();
    while (digitalRead(RESET_BUTTON) == LOW) {
      if (millis() - pressTime > 3000) {
        Serial.println("🔄 إعادة تعيين إعدادات WiFi...");
        blinkLED(LED_RED, 100);
        blinkLED(LED_RED, 100);
        blinkLED(LED_RED, 100);
        
        wifiManager.resetSettings();
        Serial.println("✅ تم إعادة التعيين - سيتم إعادة التشغيل...");
        
        beep(1000);
        delay(1000);
        ESP.restart();
      }
    }
  }
  
  // التحقق من وجود بطاقة جديدة
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  
  // قراءة البطاقة
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  
  // الحصول على UID
  String uid = getUID();
  
  // منع المسح المتكرر لنفس البطاقة
  if (uid == lastScannedUID && (millis() - lastScanTime) < SCAN_DELAY) {
    Serial.println("⚠️ نفس البطاقة - انتظر قليلاً");
    mfrc522.PICC_HaltA();
    return;
  }
  
  // تحديث آخر بطاقة ممسوحة
  lastScannedUID = uid;
  lastScanTime = millis();
  
  Serial.println("📱 بطاقة ممسوحة: " + uid);
  beep(50);
  
  // إرسال البيانات إلى n8n
  sendToN8N(uid);
  
  // إيقاف البطاقة
  mfrc522.PICC_HaltA();
  
  delay(500);
}

void connectWiFi() {
  Serial.println("🔌 بدء إعداد WiFi Manager...");
  
  // تعيين timeout للبوابة (3 دقائق)
  wifiManager.setConfigPortalTimeout(180);
  
  // إضافة حقل مخصص لإدخال رابط webhook
  WiFiManagerParameter custom_webhook("webhook", "n8n Webhook URL", n8nWebhookURL.c_str(), 200);
  wifiManager.addParameter(&custom_webhook);
  
  // إضافة callback لحفظ المعلومات
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  // محاولة الاتصال التلقائي، إذا فشل يفتح بوابة الإعداد
  Serial.println("🔍 محاولة الاتصال بآخر شبكة محفوظة...");
  
  if (!wifiManager.autoConnect("ESP8266-Attendance")) {
    Serial.println("❌ فشل الاتصال وانتهى الوقت");
    blinkLED(LED_RED, 500);
    beep(1000);
    delay(3000);
    // إعادة التشغيل والمحاولة مرة أخرى
    ESP.restart();
    delay(5000);
  }
  
  // حفظ webhook URL من الحقل المخصص
  String newWebhook = custom_webhook.getValue();
  if (newWebhook.length() > 0 && newWebhook != n8nWebhookURL) {
    n8nWebhookURL = newWebhook;
    saveWebhookToEEPROM(n8nWebhookURL);
    Serial.println("💾 تم حفظ webhook جديد");
  }
  
  if (n8nWebhookURL.length() == 0) {
    Serial.println("⚠️ تحذير: لم يتم إدخال webhook URL");
    Serial.println("يمكنك إعادة التعيين بالضغط على زر Flash لمدة 3 ثواني");
  }
  
  Serial.println("✅ متصل بالواي فاي بنجاح!");
  Serial.print("📡 IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("🔗 Webhook URL: ");
  Serial.println(n8nWebhookURL);
  
  // إشارة نجاح الاتصال
  blinkLED(LED_GREEN, 200);
  beep(100);
  delay(100);
  blinkLED(LED_GREEN, 200);
  beep(100);
}

String getUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      uid += "0";
    }
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

String getTimestamp() {
  time_t now = time(nullptr);
  
  // التحقق من صحة الوقت
  if (now < 1000000000) {
    // إذا لم يتم مزامنة الوقت، استخدم millis
    Serial.println("⚠️ استخدام millis بدلاً من الوقت الحقيقي");
    return String(millis());
  }
  
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char timestamp[25];
  // تنسيق ISO 8601: YYYY-MM-DDTHH:MM:SS+02:00
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  
  // إضافة timezone
  String result = String(timestamp) + "+02:00";
  return result;
}

void printLocalTime() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  Serial.print("📅 التاريخ والوقت: ");
  Serial.println(getTimestamp());
}

void sendToN8N(String rfid_uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ غير متصل بالواي فاي");
    // لمبة حمراء + بازر طويل للخطأ
    digitalWrite(LED_RED, HIGH);
    beep(500);
    delay(1000);
    digitalWrite(LED_RED, LOW);
    return;
  }
  
  if (n8nWebhookURL.length() == 0) {
    Serial.println("❌ webhook URL غير معرّف");
    digitalWrite(LED_RED, HIGH);
    beep(500);
    delay(1000);
    digitalWrite(LED_RED, LOW);
    return;
  }
  
  HTTPClient http;
  
  // التحقق من نوع البروتوكول
  if (n8nWebhookURL.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    http.begin(client, n8nWebhookURL);
    Serial.println("🔒 استخدام HTTPS (SSL)");
  } else {
    WiFiClient client;
    http.begin(client, n8nWebhookURL);
    Serial.println("🔓 استخدام HTTP");
  }
  
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);
  
  // إنشاء JSON للإرسال مع timestamp صحيح
  StaticJsonDocument<256> doc;
  doc["rfid_uid"] = rfid_uid;
  doc["timestamp"] = getTimestamp();
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("📤 إرسال البيانات إلى n8n...");
  Serial.println(jsonString);
  
  // إرسال POST request
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("📥 الرد من n8n:");
    Serial.println(response);
    
    // معالجة الرد
    processResponse(response);
  } else {
    Serial.print("❌ خطأ في الإرسال: ");
    Serial.println(httpResponseCode);
    
    if (httpResponseCode == -1) {
      Serial.println("💡 خطأ في الاتصال - تحقق من:");
      Serial.println("   - صحة رابط webhook");
      Serial.println("   - اتصال الإنترنت");
    } else if (httpResponseCode == -11) {
      Serial.println("💡 Timeout - السيرفر لم يرد في الوقت المحدد");
    }
    
    // لمبة حمراء + بازر للخطأ
    digitalWrite(LED_RED, HIGH);
    beep(500);
    delay(1000);
    digitalWrite(LED_RED, LOW);
  }
  
  http.end();
}

void processResponse(String jsonResponse) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonResponse);
  
  if (error) {
    Serial.println("❌ خطأ في قراءة JSON");
    // لمبة حمراء + بازر
    digitalWrite(LED_RED, HIGH);
    beep(500);
    delay(1000);
    digitalWrite(LED_RED, LOW);
    return;
  }
  
  bool success = doc["success"];
  String action = doc["action"];
  String message = doc["message"];
  
  Serial.println("✅ Action: " + action);
  Serial.println("📝 Message: " + message);
  
  // معالجة مبسطة: نجاح = أخضر، رفض/خطأ = أحمر
  if (success) {
    // نجاح - لمبة خضراء + بيب واحد
    Serial.println("✅ تم بنجاح");
    digitalWrite(LED_GREEN, HIGH);
    beep(200);
    delay(2000);
    digitalWrite(LED_GREEN, LOW);
    
  } else {
    // رفض أو خطأ - لمبة حمراء + بيبين
    Serial.println("❌ تم الرفض");
    digitalWrite(LED_RED, HIGH);
    beep(150);
    delay(100);
    beep(150);
    delay(2000);
    digitalWrite(LED_RED, LOW);
  }
  
  // طباعة تفاصيل إضافية إذا وجدت
  if (doc.containsKey("data")) {
    JsonObject data = doc["data"];
    if (data.containsKey("student_name")) {
      Serial.println("👨‍🎓 الطالب: " + String((const char*)data["student_name"]));
    }
    if (data.containsKey("teacher_name")) {
      Serial.println("👨‍🏫 المعلم: " + String((const char*)data["teacher_name"]));
    }
  }
  
  Serial.println("✅ جاهز للمسح التالي");
}

void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

void blinkLED(int pin, int duration) {
  digitalWrite(pin, HIGH);
  delay(duration);
  digitalWrite(pin, LOW);
  delay(duration);
}

// ====== دوال حفظ وقراءة EEPROM ======
void saveWebhookToEEPROM(String url) {
  for (int i = 0; i < 200; i++) {
    EEPROM.write(WEBHOOK_ADDRESS + i, 0);
  }
  
  int len = url.length();
  EEPROM.write(WEBHOOK_ADDRESS, len);
  
  for (int i = 0; i < len; i++) {
    EEPROM.write(WEBHOOK_ADDRESS + 1 + i, url[i]);
  }
  
  EEPROM.commit();
  Serial.println("💾 تم حفظ webhook في EEPROM");
}

String readWebhookFromEEPROM() {
  int len = EEPROM.read(WEBHOOK_ADDRESS);
  
  if (len == 0 || len > 200 || len == 255) {
    return "";
  }
  
  String url = "";
  for (int i = 0; i < len; i++) {
    char c = EEPROM.read(WEBHOOK_ADDRESS + 1 + i);
    if (c == 0 || c == 255) break;
    url += c;
  }
  
  return url;
}

void saveConfigCallback() {
  Serial.println("📝 تم طلب حفظ الإعدادات");
}