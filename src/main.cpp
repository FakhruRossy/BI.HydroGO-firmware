#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "AnalogSensorService.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- KONFIGURASI JARINGAN ---
const char* DEFAULT_WIFI_SSID = "hydrogoo";
const char* DEFAULT_WIFI_PASSWORD = "hydrogoo";
const char* SUPABASE_URL_SENSORS = "https://ntudiforfsotyqdufhxu.supabase.co/rest/v1/sensor_logs";
const char* SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im50dWRpZm9yZnNvdHlxZHVmaHh1Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkxMjUzMTksImV4cCI6MjA3NDcwMTMxOX0.nSlLo-F6fUs-5hnqq2lt3zk8OU1wRnIjjCvBEMsqe1Y";
// --- SARAN PERBAIKAN ---
// Perpanjang interval untuk mengurangi seberapa sering kode 'blocking' akibat koneksi internet.
// 10000ms = 10 detik. Ini akan membuat sistem jauh lebih responsif.
const unsigned long SUPABASE_SEND_INTERVAL = 10000; 
unsigned long lastSupabaseSend = 0;

// --- KONFIGURASI UMUM ---
const int LCD_ADDRESS = 0x27;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
const int SENSOR_5V_RELAY_PIN  = 16;
const int SENSOR_GND_RELAY_PIN = 19;
const int relayPins[] = {13, 27, 26, 25, 33};
const int NUM_RELAYS = 5;
bool relayStates[NUM_RELAYS] = {false};
const int buttonPins[] = {32, 35, 34, 39, 36};
const int NUM_BUTTONS = 5;
const unsigned long DEBOUNCE_DELAY = 100;
const int PH_MINUS_RELAY_INDEX = 3;
const int PH_PLUS_RELAY_INDEX = 4;
const float phTargetMin = 5.8;
const float phTargetMax = 6.2;
const unsigned long AUTOMATION_START_DELAY = 180000; // 3 menit
const unsigned long PUMP_ON_DURATION = 200;
const unsigned long PUMP_COOLDOWN_DURATION = 30000;
unsigned long lastDoseTime = 0;
unsigned int autoDoseCount = 0;

// --- OBJEK & VARIABEL GLOBAL ---
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
AnalogSensorService sensorService(SENSOR_5V_RELAY_PIN, SENSOR_GND_RELAY_PIN);
unsigned long lastLcdUpdate = 0;
const unsigned long LCD_UPDATE_INTERVAL = 1000;
float lastKnownPh = -1.0;
float lastKnownTds = -1.0;
float lastKnownPhVoltage = -1.0;
float lastKnownTdsVoltage = -1.0;

// --- PERBAIKAN: Variabel untuk non-blocking pump ---
bool isDosing = false;
unsigned long doseStartTime = 0;
int dosingRelayIndex = -1;

// --- FUNGSI PROTOTIPE ---
void setupWifi();
bool connectToWifi(const char* ssid, const char* password, const char* message);
void sendToSupabase();
void handleButtons();
void handleAutomation();
void startDose(int relayIndex); // Diubah dari dosePhSolution
void handleDosing(); // Fungsi baru untuk mematikan pompa
void updateLcdDisplay();


void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Sistem Monitoring & Kontrol v5.7 (Non-Blocking) ---");
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Inisialisasi...");

  setupWifi();

  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT);
  }
  
  sensorService.setCompensationPH(-8.491, 35.597); 
  Serial.println("Kalibrasi pH kustom diterapkan.");

  sensorService.begin();
  Serial.println("Sistem berjalan. Mode Auto aktif setelah 3 menit.");
  delay(1000);
  lcd.clear();
}

void loop() {
  sensorService.update();
  handleButtons();
  handleDosing(); // Cek apakah sudah waktunya mematikan pompa
  handleAutomation();

  if (millis() - lastLcdUpdate >= LCD_UPDATE_INTERVAL) {
    lastLcdUpdate = millis();
    updateLcdDisplay();
  }

  if (WiFi.status() == WL_CONNECTED && millis() - lastSupabaseSend >= SUPABASE_SEND_INTERVAL) {
    lastSupabaseSend = millis();
    sendToSupabase();
  }
}

// --- Fungsi WiFi tidak berubah ---
void setupWifi() {
  if (!connectToWifi(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD, "WiFi")) {
    Serial.println("\nTidak dapat terhubung ke WiFi.");
    lcd.clear(); 
    lcd.print("WiFi Gagal!");
  }
}

bool connectToWifi(const char* ssid, const char* password, const char* message) {
  Serial.printf("\nMencoba koneksi ke %s...", message);
  lcd.clear(); lcd.setCursor(0,0); lcd.print("Connecting to"); lcd.setCursor(0,1); lcd.print(ssid);
  WiFi.begin(ssid, password);
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nTerhubung!");
    Serial.print("Alamat IP: "); Serial.println(WiFi.localIP());
    lcd.clear(); lcd.print("WiFi Terhubung!"); lcd.setCursor(0,1); lcd.print(WiFi.localIP());
    delay(2000);
    return true;
  }
  return false;
}

// --- Fungsi Supabase tidak berubah, tapi akan lebih jarang dipanggil ---
void sendToSupabase() {
  // if (lastKnownPh < 0 || lastKnownTds < 0) {
  //   return;
  // }
  JsonDocument doc;
  doc["ph"] = lastKnownPh;
  doc["tds"] = (int)lastKnownTds;
  doc["ph_auto"] = autoDoseCount;
  doc["ph_v"] = lastKnownPhVoltage;
  doc["tds_v"] = lastKnownTdsVoltage;
  
  String jsonPayload;
  serializeJson(doc, jsonPayload);
  HTTPClient http;
  http.begin(SUPABASE_URL_SENSORS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  int httpResponseCode = http.POST(jsonPayload);
  if (httpResponseCode == 201) {
    Serial.println("Data Supabase (dgn V) Terkirim!");
  } else {
    Serial.print("Gagal kirim. Kode: ");
    Serial.println(httpResponseCode);
    Serial.println(http.getString());
  }
  http.end();
}

void handleButtons() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool isPressed = (digitalRead(buttonPins[i]) == LOW);
        digitalWrite(relayPins[i], isPressed ? LOW : HIGH);
        relayStates[i] = isPressed;
    }
}

// --- PERBAIKAN: Fungsi ini sekarang HANYA MENGECEK dan MEMULAI dosis ---
void handleAutomation() {
    if (isDosing) return; // Jangan lakukan apapun jika pompa sedang aktif
    if (millis() < AUTOMATION_START_DELAY) return;
    if (digitalRead(buttonPins[PH_MINUS_RELAY_INDEX]) == LOW || digitalRead(buttonPins[PH_PLUS_RELAY_INDEX]) == LOW) return; 
    if (millis() - lastDoseTime < PUMP_COOLDOWN_DURATION) return;
    if (!sensorService.isPhActiveNow()) return;
    
    float currentPh = sensorService.getCalibratedPHValue();

    if (currentPh <= 0) return;
    if (currentPh < phTargetMin) {
        startDose(PH_PLUS_RELAY_INDEX);
    } else if (currentPh > phTargetMax) {
        startDose(PH_MINUS_RELAY_INDEX);
    }
}

// --- PERBAIKAN: Fungsi ini HANYA untuk MENYALAKAN pompa ---
void startDose(int relayIndex) {
    digitalWrite(relayPins[relayIndex], LOW);
    relayStates[relayIndex] = true;
    isDosing = true;
    doseStartTime = millis();
    dosingRelayIndex = relayIndex;
    autoDoseCount++;
    // updateLcdDisplay(); // Tidak perlu, karena loop utama sudah mengurusnya
}

// --- FUNGSI BARU: Untuk mematikan pompa tanpa delay ---
void handleDosing() {
  if (isDosing && (millis() - doseStartTime >= PUMP_ON_DURATION)) {
    digitalWrite(relayPins[dosingRelayIndex], HIGH);
    relayStates[dosingRelayIndex] = false;
    isDosing = false;
    lastDoseTime = millis();
    dosingRelayIndex = -1;
  }
}


void updateLcdDisplay() {
    char line1[17];
    char line2[17];
    if (sensorService.isPhActiveNow()) {
        lastKnownPh = sensorService.getCalibratedPHValue();
        lastKnownPhVoltage = sensorService.getFilteredPHVoltage();
    } else {
        lastKnownTds = sensorService.getCalibratedTDSValue(25.0);
        lastKnownTdsVoltage = sensorService.getFilteredTDSVoltage();
    }
    snprintf(line1, sizeof(line1), "pH:%.2f TDS:%-4d", 
             lastKnownPh > 0 ? lastKnownPh : 0.0, 
             lastKnownTds > 0 ? (int)lastKnownTds : 0);
    char relayStatusStr[7] = "";
    for (int i = 0; i < NUM_RELAYS; i++) {
        relayStatusStr[i] = relayStates[i] ? (char)('1' + i) : '-';
    }
    relayStatusStr[NUM_RELAYS] = '\0';
    snprintf(line2, sizeof(line2), "R:%s A:%s/%-2u", 
             relayStatusStr, 
             (millis() > AUTOMATION_START_DELAY) ? "ON " : "OFF",
             autoDoseCount);
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
}

