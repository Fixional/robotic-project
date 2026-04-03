#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

// Bypass Brownout
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Inisialisasi Pin
#define SERVO_PIN 13
#define LED_HIJAU 25
#define LED_MERAH 26
#define IR_SENSOR 18

#define TRIG_ORG 14
#define ECHO_ORG 27
#define TRIG_ANO 32
#define ECHO_ANO 33

// Objek
Servo binServo;
WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Variabel
const char* ssid = "Rhome_Multi";
const char* password = "mmtari9999";
const int DISTANCE_EMPTY = 12; 

int pctOrg = 0;
int pctAno = 0;
bool isObjectDetected = false;

// Fungsi Baca Ultrasonic
int getPercentage(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH);
  int distance = duration * 0.034 / 2;
  if (distance > DISTANCE_EMPTY) distance = DISTANCE_EMPTY;
  if (distance < 2) distance = 2;
  return ((float)(DISTANCE_EMPTY - distance) / (DISTANCE_EMPTY - 2)) * 100;
}

// Update LCD
void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("ORG:"); lcd.print(pctOrg); lcd.print("%   ");
  lcd.setCursor(0, 1);
  lcd.print("ANO:"); lcd.print(pctAno); lcd.print("%   ");
  
  // Status IR di pojok kanan bawah
  lcd.setCursor(10, 1);
  if(isObjectDetected) lcd.print("[OBJ]");
  else lcd.print("[---]");
}

void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:sans-serif; text-align:center;} .btn{padding:15px; margin:10px; color:white; border-radius:10px; border:none; font-size:18px;}";
  html += ".org{background:#4CAF50;} .ano{background:#f44336;} .disabled{background:#888; cursor:not-allowed;}</style>";
  
  html += "<script>setInterval(function(){ fetch('/status').then(r => r.json()).then(data => {";
  html += "document.getElementById('st-ir').innerHTML = data.ir ? 'ADA SAMPAH' : 'KOSONG';";
  html += "document.getElementById('st-ir').style.color = data.ir ? 'green' : 'red';";
  html += "const btns = document.querySelectorAll('.btn');";
  html += "btns.forEach(b => { if(!data.ir) b.classList.add('disabled'); else b.classList.remove('disabled'); });";
  html += "});}, 1000);</script></head><body>";
  
  html += "<h1>Smart Bin Control</h1>";
  html += "<h3>Status IR: <span id='st-ir'>...</span></h3>";
  html += "<p>Letakkan sampah di sensor IR untuk mengaktifkan tombol</p>";
  
  html += "<button class='btn org' onclick=\"fetch('/move?type=org')\">Organik</button>";
  html += "<button class='btn ano' onclick=\"fetch('/move?type=ano')\">Anorganik</button>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{\"org\":" + String(pctOrg) + ",\"ano\":" + String(pctAno) + ",\"ir\":" + String(isObjectDetected) + "}";
  server.send(200, "application/json", json);
}

void handleMove() {
  // Proteksi: Jika IR tidak mendeteksi benda, abaikan perintah web
  if (!isObjectDetected) {
    server.send(403, "text/plain", "Gagal: Sampah tidak terdeteksi di sensor IR!");
    return;
  }

  String type = server.arg("type");
  if (type == "org") {
    digitalWrite(LED_HIJAU, HIGH);
    binServo.write(0);
    delay(3000);
    digitalWrite(LED_HIJAU, LOW);
  } else if (type == "ano") {
    digitalWrite(LED_MERAH, HIGH);
    binServo.write(180);
    delay(3000);
    digitalWrite(LED_MERAH, LOW);
  }
  binServo.write(90);
  server.send(200, "text/plain", "Berhasil");
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);

  // GUNAKAN .begin() KARENA LIBRARY ANDA MENDUKUNG INI
  lcd.begin(); 
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  pinMode(LED_HIJAU, OUTPUT);
  pinMode(LED_MERAH, OUTPUT);
  pinMode(IR_SENSOR, INPUT);
  pinMode(TRIG_ORG, OUTPUT); pinMode(ECHO_ORG, INPUT);
  pinMode(TRIG_ANO, OUTPUT); pinMode(ECHO_ANO, INPUT);

  binServo.attach(SERVO_PIN);
  binServo.write(90);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) { 
    delay(500); 
    Serial.print("."); 
    retry++;
  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP()); 

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP Address:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP()); // Ini akan menampilkan angka IP di layar LCD
    delay(5000); // Tunggu 5 detik agar Anda sempat mencatat IP-nya
  } else {
    Serial.println("\nConnection Failed!");
    lcd.clear();
    lcd.print("WiFi Failed!");
    delay(2000);
  }
  
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/move", handleMove);
  server.begin();
  
  lcd.clear();
}

void loop() {
  server.handleClient();
  
  // Baca Sensor IR (Active LOW)
  isObjectDetected = (digitalRead(IR_SENSOR) == LOW);

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
    pctOrg = getPercentage(TRIG_ORG, ECHO_ORG);
    pctAno = getPercentage(TRIG_ANO, ECHO_ANO);
    updateLCD();
    lastUpdate = millis();
  }
}