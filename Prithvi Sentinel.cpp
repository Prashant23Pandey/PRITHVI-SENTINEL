// PRITHVI SENTINEL - Arduino Mega 2560
// Libraries: LiquidCrystal_I2C, DHT sensor library (Adafruit) + Adafruit Unified Sensor

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ---------- Pins ----------
const int TRIG = 8, ECHO = 9;
const int RAIN_PIN = A0, SOIL_PIN = A1;
const int DHT_PIN = 7;
const int LED_G = 23, LED_B = 24, LED_R = 25;

// ---------- Settings (yahan apne hisaab se badlo) ----------
const bool LED_ACTIVE_LOW = true;   // common + resistor se 5V ja raha hai -> true
                                    // LED ulta jale to false kar do
const uint8_t LCD_ADDR = 0x27;      // I2C scanner se check karo (0x27 ya 0x3F)

// Sensor se paani ki surface tak doori (cm). Doori kam = paani upar.
const float WATCH_CM    = 20.0;     // isse neeche aaye -> WATCH (blue)
const float CRITICAL_CM = 10.0;     // isse neeche aaye -> CRITICAL (red blink)

// Rain / soil calibration (dry = jab sensor sukha, wet = jab bhiga)
const int RAIN_DRY = 1023, RAIN_WET = 300;
const int SOIL_DRY = 1023, SOIL_WET = 300;
const int RAIN_HIGH_PCT = 60;       // itne % se upar = tez baarish
const int SOIL_HIGH_PCT = 60;       // itne % se upar = zameen saturated

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
DHT dht(DHT_PIN, DHT11);

const char* LEVEL_NAME[] = {"NORMAL", "WATCH", "CRITICAL"};
int level = 0;                      // 0 normal, 1 watch, 2 critical
unsigned long lastRead = 0, lastBlink = 0;
bool blinkState = false;

void led(int pin, bool on) {
  digitalWrite(pin, (on ^ LED_ACTIVE_LOW) ? HIGH : LOW);
}

float readDistanceOnce() {
  digitalWrite(TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long d = pulseIn(ECHO, HIGH, 30000);
  if (d == 0) return -1;            // echo nahi aaya
  return d * 0.0343 / 2.0;
}

// 5 readings ka median, galat readings hata deta hai. -1 matlab sensor error.
float readDistance() {
  float v[5];
  int n = 0;
  for (int i = 0; i < 5; i++) {
    float x = readDistanceOnce();
    if (x > 2 && x < 400) v[n++] = x;
    delay(30);
  }
  if (n < 3) return -1;
  for (int i = 1; i < n; i++) {
    float k = v[i]; int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[n / 2];
}

int pct(int raw, int dry, int wet) {
  return constrain(map(raw, dry, wet, 0, 100), 0, 100);
}

void updateLeds() {
  led(LED_G, level == 0);
  led(LED_B, level == 1);
  if (level == 2) {
    if (millis() - lastBlink >= 250) {
      lastBlink = millis();
      blinkState = !blinkState;
    }
    led(LED_R, blinkState);         // red blink
  } else {
    led(LED_R, false);
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(LED_R, OUTPUT);
  dht.begin();

  // LED wiring test: green, blue, red ek ek karke jalenge
  led(LED_G, false); led(LED_B, false); led(LED_R, false);
  led(LED_G, true); delay(400); led(LED_G, false);
  led(LED_B, true); delay(400); led(LED_B, false);
  led(LED_R, true); delay(400); led(LED_R, false);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("PRITHVI SENTINEL");
  lcd.setCursor(0, 1); lcd.print("Starting...     ");
  Serial.println(F("=== PRITHVI SENTINEL started ==="));
  delay(1500);
}

void loop() {
  updateLeds();
  if (millis() - lastRead < 1000) return;
  lastRead = millis();

  float dist = readDistance();
  int rainRaw = analogRead(RAIN_PIN);
  int soilRaw = analogRead(SOIL_PIN);
  int rainPct = pct(rainRaw, RAIN_DRY, RAIN_WET);
  int soilPct = pct(soilRaw, SOIL_DRY, SOIL_WET);
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // ---- Risk logic ----
  int waterLvl = 0;
  if (dist > 0) {
    if (dist <= CRITICAL_CM) waterLvl = 2;
    else if (dist <= WATCH_CM) waterLvl = 1;
  }
  int extra = (rainPct >= RAIN_HIGH_PCT) + (soilPct >= SOIL_HIGH_PCT);
  level = waterLvl;
  const char* reason = "All readings safe";
  if (waterLvl == 2) reason = "Water level very high";
  else if (waterLvl == 1) reason = "Water level rising";
  if (waterLvl == 0 && extra >= 1) { level = 1; reason = "Rain/soil high"; }
  if (waterLvl == 1 && extra == 2) { level = 2; reason = "Water rising + rain + soil saturated"; }
  if (dist < 0) reason = "Ultrasonic ERROR (no echo) - check wiring";

  // ---- Serial Monitor ----
  Serial.println(F("--------------------------------"));
  Serial.print(F("Water distance : "));
  if (dist < 0) Serial.println(F("ERROR"));
  else { Serial.print(dist, 1); Serial.println(F(" cm")); }
  Serial.print(F("Rain           : ")); Serial.print(rainPct);
  Serial.print(F("% (raw ")); Serial.print(rainRaw); Serial.println(F(")"));
  Serial.print(F("Soil moisture  : ")); Serial.print(soilPct);
  Serial.print(F("% (raw ")); Serial.print(soilRaw); Serial.println(F(")"));
  Serial.print(F("Temp / Humidity: "));
  if (isnan(temp) || isnan(hum)) Serial.println(F("NA"));
  else { Serial.print(temp, 0); Serial.print(F(" C / ")); Serial.print(hum, 0); Serial.println(F(" %")); }
  Serial.print(F("RISK LEVEL     : ")); Serial.println(LEVEL_NAME[level]);
  Serial.print(F("Reason         : ")); Serial.println(reason);

  // ---- LCD ----
  char buf[17];
  if (dist < 0) snprintf(buf, 17, "D: ERR  %-8s", LEVEL_NAME[level]);
  else snprintf(buf, 17, "D:%3dcm %-8s", (int)dist, LEVEL_NAME[level]);
  lcd.setCursor(0, 0); lcd.print(buf);
  snprintf(buf, 17, "R:%3d%% S:%3d%%   ", rainPct, soilPct);
  lcd.setCursor(0, 1); lcd.print(buf);
}
