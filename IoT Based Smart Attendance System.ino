#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WebServer.h> // Include the WebServer library

// RFID Configuration
#define RFID_RX 4 // RX pin for RFID
#define RFID_TX 5 // TX pin for RFID
HardwareSerial RFID(1); // Use UART1 for RFID

// OLED Configuration
#define I2C_SDA 21 // GPIO21 (D21)
#define I2C_SCL 22 // GPIO22 (D22)
#define i2c_Address 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// RTC Configuration
RTC_DS3231 rtc;

// Timer Configuration
unsigned long displayTimeout = 5000; // 5 seconds timeout for black display
unsigned long lastRFIDScanTime = 0;
bool displayOn = true;

// Wi-Fi Configuration
const char* ap_ssid = "Signature";           // AP SSID
const char* ap_password = "0987654321";      // AP password (minimum 8 characters)
const char* ssid = "Joy's Phone";            // Replace with your Wi-Fi network SSID
const char* password = "Joy12345";           // Replace with your Wi-Fi network password

// Web Server
WebServer server(80);

// Data Storage
String rfidData = "";
int serialNumber = 1; // Initialize serial number

// Function Prototypes
void handleRoot();
void handleNotFound();

void setup() {
  // Initialize serial monitor
  Serial.begin(115200);
  delay(250);

  // Initialize RFID module
  RFID.begin(9600, SERIAL_8N1, RFID_RX, RFID_TX);
  Serial.println("Bring your RFID card closer...");

  // Initialize OLED display
  Wire.begin(I2C_SDA, I2C_SCL); // Specify I2C pins
  if (!display.begin(i2c_Address, true)) {
    Serial.println("OLED initialization failed!");
    while (1); // Halt if OLED fails
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("RFID Reader Ready");
  display.display();

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC initialization failed!");
    display.clearDisplay();
    display.println("RTC Error!");
    display.display();
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Set to compile time
  }

  // Initialize Wi-Fi in both STA and AP modes
  setupWiFi();

  // Set up Web Server routes
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  static String text = "";

  // Turn off display after timeout
  if (displayOn && millis() - lastRFIDScanTime >= displayTimeout) {
    display.clearDisplay();
    display.display(); // Turn off OLED (black display)
    displayOn = false;
  }

  // Read data from RFID module
  while (RFID.available() > 0) {
    delay(5);
    char c = RFID.read();
    text += c;
  }

  if (text.length() > 0) {
    lastRFIDScanTime = millis(); // Reset timer
    if (!displayOn) {
      displayOn = true; // Turn on display
    }

    String cardID = text.substring(1, 11); // Extract RFID card ID
    unsigned int decValue = hexToDec(cardID); // Convert hex to decimal

    // Get current time from RTC
    DateTime now = rtc.now();

    // Format time in 12-hour format
    int hour = now.hour();
    String am_pm = "AM";

    if (hour == 0) {
      hour = 12; // Midnight hour
    } else if (hour == 12) {
      am_pm = "PM"; // Noon hour
    } else if (hour > 12) {
      hour -= 12; // Convert to 12-hour format
      am_pm = "PM";
    }

    String currentTime = String(hour) + ":" +
                         (now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" +
                         (now.second() < 10 ? "0" : "") + String(now.second()) + " " + am_pm;

    String currentDate = String(now.day()) + "/" + String(now.month()) + "/" + String(now.year());

    // Store RFID data for web display with serial number
    rfidData += "<tr><td>" + String(serialNumber++) + "</td><td>" + cardID + "</td><td>" +
                String(decValue) + "</td><td>" + currentTime + "</td><td>" + currentDate + "</td></tr>";

    // Print RFID and time data to Serial Monitor
    Serial.print("Card ID (Hex): ");
    Serial.println(cardID);
    Serial.print("Card ID (Dec): ");
    Serial.println(decValue);
    Serial.print("Time: ");
    Serial.println(currentTime);
    Serial.print("Date: ");
    Serial.println(currentDate);

    // Display RFID and time data on OLED
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("RFID Reader:");
    display.println("----------------");
    display.print("Card ID: ");
    display.println(cardID);
    display.print("Dec ID: ");
    display.println(decValue);
    display.println();
    display.print("Time: ");
    display.println(currentTime);
    display.print("Date: ");
    display.println(currentDate);
    display.display();

    // Clear text for the next read
    text = "";
    Serial.println("Bring your RFID card closer...");
  }

  // Handle web server
  server.handleClient();
}

void setupWiFi() {
  // Start AP mode
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP Mode: ");
  Serial.println(apIP);

  // Start STA mode
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi in STA mode");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to Wi-Fi. IP address: ");
  Serial.println(WiFi.localIP());
}

void handleRoot() {
  String html = "<html><head><title>RFID Data</title></head><body>";
  html += "<h1>RFID Data Log</h1>";
  html += "<table border='1'><tr><th>S.No</th><th>Card ID</th><th>Decimal ID</th><th>Time</th><th>Date</th></tr>";
  html += rfidData;
  html += "</table></body></html>";
  server.send(200, "text/html", html);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

unsigned int hexToDec(String hexString) {
  unsigned int decValue = 0;
  for (int i = 0; i < hexString.length(); i++) {
    char c = hexString.charAt(i);
    if (isDigit(c)) {
      decValue = (decValue * 16) + (c - '0');
    } else if (isAlpha(c)) {
      c = toupper(c);
      decValue = (decValue * 16) + (c - 'A' + 10);
    }
  }
  return decValue;
}
