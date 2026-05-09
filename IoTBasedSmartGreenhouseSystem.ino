#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// ─────────────────────────────────────────────
// WIFI
// ─────────────────────────────────────────────
#define WIFI_SSID     "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// ─────────────────────────────────────────────
// FIREBASE
// ─────────────────────────────────────────────
#define FIREBASE_HOST "https://studio-4871676974-e3e3d-default-rtdb.firebaseio.com"
#define FIREBASE_API_KEY "YOUR_API_KEY"

// ─────────────────────────────────────────────
// DHT11
// ─────────────────────────────────────────────
#define DHT_PIN   23
#define DHT_TYPE  DHT11

DHT dht(DHT_PIN, DHT_TYPE);

// ─────────────────────────────────────────────
// LM35
// ─────────────────────────────────────────────
#define LM35_PIN 34

// ─────────────────────────────────────────────
// SOIL SENSOR
// ─────────────────────────────────────────────
#define SOIL_AO_PIN 35
#define SOIL_DO_PIN 32

// ─────────────────────────────────────────────
// OUTPUTS
// ─────────────────────────────────────────────
#define GREEN_LED  2
#define RED_LED    15
#define BUZZER_PIN 33

// ─────────────────────────────────────────────
// LCD
// ─────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─────────────────────────────────────────────
// KEYPAD
// ─────────────────────────────────────────────
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {19,18,5,4};
byte colPins[COLS] = {14,27,26,25};

Keypad keypad = Keypad(makeKeymap(keys),
                       rowPins,
                       colPins,
                       ROWS,
                       COLS);

// ─────────────────────────────────────────────
// THRESHOLDS
// ─────────────────────────────────────────────
float TEMP_THRESHOLD = 35.0;
float HUM_THRESHOLD  = 70.0;
int   SOIL_THRESHOLD = 3000;

// ─────────────────────────────────────────────
// VARIABLES
// ─────────────────────────────────────────────
int currentMode = 0;
bool wifiConnected = false;
int readingID = 0;

bool settingThreshold = false;
char thresholdTarget = ' ';
String inputBuffer = "";

unsigned long lastDisplay = 0;
unsigned long lastFirebase = 0;
unsigned long lastWifiRetry = 0;
unsigned long lastBuzzer = 0;

bool buzzerState = false;

// ─────────────────────────────────────────────
// LCD PRINT
// ─────────────────────────────────────────────
void lcdLine(int row, String text) {

  while(text.length() < 16)
    text += " ";

  lcd.setCursor(0,row);
  lcd.print(text.substring(0,16));
}

// ─────────────────────────────────────────────
// SENSOR FUNCTIONS
// ─────────────────────────────────────────────
float getDHTTemp() {

  float t = dht.readTemperature();

  if(isnan(t))
    return -1;

  return t;
}

float getHumidity() {

  float h = dht.readHumidity();

  if(isnan(h))
    return -1;

  return h;
}

float getLM35Temp() {

  analogSetAttenuation(ADC_11db);

  int raw = analogRead(LM35_PIN);

  float voltage = (raw / 4095.0) * 3300.0;

  return voltage / 10.0;
}

int getSoilRaw() {

  return analogRead(SOIL_AO_PIN);
}

// ─────────────────────────────────────────────
// WIFI CONNECT
// ─────────────────────────────────────────────
void connectWiFi() {

  lcd.clear();
  lcdLine(0,"Connecting WiFi");

  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);

  int count = 0;

  while(WiFi.status()!=WL_CONNECTED && count < 20) {

    delay(500);
    count++;
  }

  if(WiFi.status()==WL_CONNECTED) {

    wifiConnected = true;

    lcdLine(1,"Connected");

    Serial.println("WiFi Connected");
  }
  else {

    wifiConnected = false;

    lcdLine(1,"Failed");

    Serial.println("WiFi Failed");
  }

  delay(1500);
}

// ─────────────────────────────────────────────
// WELCOME SCREEN
// ─────────────────────────────────────────────
void showWelcome() {

  lcdLine(0,"GreenHouse Mon");
  lcdLine(1,"1T 2H 3S 4All");
}

// ─────────────────────────────────────────────
// SHOW TEMPERATURE
// ─────────────────────────────────────────────
void showTemp() {

  float dhtTemp = getDHTTemp();
  float lm35Temp = getLM35Temp();

  lcdLine(0,"DHT:"+String(dhtTemp,1)+"C");
  lcdLine(1,"LM35:"+String(lm35Temp,1)+"C");
}

// ─────────────────────────────────────────────
// SHOW HUMIDITY
// ─────────────────────────────────────────────
void showHumidityScreen() {

  float h = getHumidity();

  lcdLine(0,"Humidity");

  if(h > HUM_THRESHOLD)
    lcdLine(1,String(h,1)+"% ALERT");
  else
    lcdLine(1,String(h,1)+"% NORMAL");
}

// ─────────────────────────────────────────────
// SHOW SOIL
// ─────────────────────────────────────────────
void showSoil() {

  int raw = getSoilRaw();

  String status;

  if(raw > 3000)
    status = "DRY";

  else if(raw > 2000)
    status = "NORMAL";

  else
    status = "WET";

  lcdLine(0,"Soil Moisture");
  lcdLine(1,status+" "+String(raw));

  Serial.print("Soil Raw: ");
  Serial.print(raw);
  Serial.print(" Status: ");
  Serial.println(status);
}

// ─────────────────────────────────────────────
// SHOW ALL
// ─────────────────────────────────────────────
void showAll() {

  static int screen = 0;
  static unsigned long lastSwitch = 0;

  if(millis()-lastSwitch > 1500) {

    screen++;
    if(screen > 2)
      screen = 0;

    lastSwitch = millis();
    lcd.clear();
  }

  if(screen==0)
    showTemp();

  else if(screen==1)
    showHumidityScreen();

  else
    showSoil();
}

// ─────────────────────────────────────────────
// ALERTS
// ─────────────────────────────────────────────
void handleAlerts() {

  float temp = getDHTTemp();
  float hum  = getHumidity();
  int soil   = getSoilRaw();

  bool tempAlert = temp > TEMP_THRESHOLD;
  bool humAlert  = hum > HUM_THRESHOLD;
  bool soilAlert = soil > SOIL_THRESHOLD;

  int totalAlerts = tempAlert + humAlert + soilAlert;

  if(totalAlerts == 0) {

    digitalWrite(GREEN_LED,HIGH);
    digitalWrite(RED_LED,LOW);
    digitalWrite(BUZZER_PIN,LOW);

    buzzerState = false;
  }
  else {

    digitalWrite(GREEN_LED,LOW);
    digitalWrite(RED_LED,HIGH);

    unsigned long interval;

    if(totalAlerts > 1)
      interval = 300;
    else
      interval = 700;

    if(millis()-lastBuzzer > interval) {

      lastBuzzer = millis();

      buzzerState = !buzzerState;

      digitalWrite(BUZZER_PIN,buzzerState);
    }
  }
}

// ─────────────────────────────────────────────
// FIREBASE
// ─────────────────────────────────────────────
void sendToFirebase() {

  if(!wifiConnected || WiFi.status()!=WL_CONNECTED)
    return;

  float dhtTemp = getDHTTemp();
  float lm35Temp = getLM35Temp();
  float hum = getHumidity();
  int soil = getSoilRaw();

  bool tempAlert = dhtTemp > TEMP_THRESHOLD;
  bool humAlert = hum > HUM_THRESHOLD;
  bool soilAlert = soil > SOIL_THRESHOLD;

  HTTPClient http;

  String url = String(FIREBASE_HOST)
               + "/greenhouse/"
               + String(readingID)
               + ".json?auth="
               + FIREBASE_API_KEY;

  String json = "{";

  json += "\"dht_temp\":" + String(dhtTemp,1) + ",";
  json += "\"lm35_temp\":" + String(lm35Temp,1) + ",";
  json += "\"humidity\":" + String(hum,1) + ",";
  json += "\"soil_raw\":" + String(soil) + ",";

  json += String("\"temp_alert\":")
          + (tempAlert ? "true" : "false") + ",";

  json += String("\"hum_alert\":")
          + (humAlert ? "true" : "false") + ",";

  json += String("\"soil_alert\":")
          + (soilAlert ? "true" : "false");

  json += "}";

  http.begin(url);

  http.addHeader("Content-Type","application/json");

  int code = http.PUT(json);

  Serial.print("Firebase Code: ");
  Serial.println(code);

  http.end();

  readingID++;
}

// ─────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────
void setup() {

  Serial.begin(115200);

  pinMode(GREEN_LED,OUTPUT);
  pinMode(RED_LED,OUTPUT);
  pinMode(BUZZER_PIN,OUTPUT);

  pinMode(SOIL_DO_PIN,INPUT);

  dht.begin();

  Wire.begin(21,22);

  lcd.init();
  lcd.backlight();

  lcd.clear();

  lcdLine(0,"GreenHouse Mon");
  lcdLine(1,"Starting...");

  delay(2000);

  connectWiFi();

  showWelcome();
}

// ─────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────
void loop() {

  char key = keypad.getKey();

  // ─────────────────────────────────────────
  // THRESHOLD INPUT MODE
  // ─────────────────────────────────────────
  if(settingThreshold) {

    if(key >= '0' && key <= '9') {

      inputBuffer += key;

      lcd.clear();
      lcdLine(0,"Enter Value");
      lcdLine(1,inputBuffer);
    }

    else if(key == '#') {

      float value = inputBuffer.toFloat();

      if(thresholdTarget=='A')
        TEMP_THRESHOLD = value;

      else if(thresholdTarget=='B')
        HUM_THRESHOLD = value;

      else if(thresholdTarget=='C')
        SOIL_THRESHOLD = (int)value;

      Serial.println("Threshold Updated");

      lcd.clear();
      lcdLine(0,"Threshold Set");
      lcdLine(1,String(value));

      delay(1500);

      inputBuffer = "";
      settingThreshold = false;

      showWelcome();
    }

    else if(key == '*') {

      inputBuffer = "";
      settingThreshold = false;

      showWelcome();
    }

    return;
  }

  // ─────────────────────────────────────────
  // NORMAL KEYS
  // ─────────────────────────────────────────
  if(key) {

    switch(key) {

      case '1':
        currentMode = 1;
        lcd.clear();
        break;

      case '2':
        currentMode = 2;
        lcd.clear();
        break;

      case '3':
        currentMode = 3;
        lcd.clear();
        break;

      case '4':
        currentMode = 4;
        lcd.clear();
        break;

      case 'A':

        settingThreshold = true;
        thresholdTarget = 'A';
        inputBuffer = "";

        lcd.clear();
        lcdLine(0,"Set Temp");
        lcdLine(1,"Enter Value");

        break;

      case 'B':

        settingThreshold = true;
        thresholdTarget = 'B';
        inputBuffer = "";

        lcd.clear();
        lcdLine(0,"Set Humidity");
        lcdLine(1,"Enter Value");

        break;

      case 'C':

        settingThreshold = true;
        thresholdTarget = 'C';
        inputBuffer = "";

        lcd.clear();
        lcdLine(0,"Set Soil");
        lcdLine(1,"Enter Value");

        break;

      case '*':

        currentMode = 0;
        lcd.clear();
        showWelcome();

        break;
    }
  }

  // ─────────────────────────────────────────
  // DISPLAY
  // ─────────────────────────────────────────
  if(millis()-lastDisplay > 500) {

    lastDisplay = millis();

    switch(currentMode) {

      case 1:
        showTemp();
        break;

      case 2:
        showHumidityScreen();
        break;

      case 3:
        showSoil();
        break;

      case 4:
        showAll();
        break;
    }
  }

  // ─────────────────────────────────────────
  // ALERTS
  // ─────────────────────────────────────────
  handleAlerts();

  // ─────────────────────────────────────────
  // FIREBASE
  // ─────────────────────────────────────────
  if(millis()-lastFirebase > 3000) {

    lastFirebase = millis();

    sendToFirebase();
  }

  // ─────────────────────────────────────────
  // WIFI RETRY
  // ─────────────────────────────────────────
  if(!wifiConnected &&
     millis()-lastWifiRetry > 30000) {

    lastWifiRetry = millis();

    connectWiFi();
  }
}
