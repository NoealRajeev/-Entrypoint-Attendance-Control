#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <MFRC522.h>
#include <map>
#include <Servo.h>

const char* ssid = "NOEALRAJEEV";
const char* password = "6282501392";
int port = 8080;
const char* endpoint = "/api/v1/auth/nfc";  // Your Spring Boot endpoint
String serverUrl;

const char* payloadTemplate = "{\"time\": \"%04d-%02d-%02dT%02d:%02d:%02d\",\"UUID\": \"%s\",\"cls\": \"%s\"}";  // Adjusted payload format

#define RST_PIN D3    // Configurable, see typical pin layout above
#define SS_PIN D8     // Configurable, see typical pin layout above
#define SERVO_PIN D4  // Define servo motor pin
#define IR_PIN D0     // Define IR sensor pin

Servo servo1;
bool doorState = false;

RTC_DS3231 rtc;
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
// Define a map to store statuses for different NFC IDs
std::map<String, String> statusMap;

void setup() {
  Serial.begin(115200);
  delay(10);

  WiFi.begin(ssid, password);  // Connect to the network
  SPI.begin();                 // Init SPI bus
  servo1.attach(SERVO_PIN);    // Attach servo to the pin
  servo1.write(0);
  pinMode(IR_PIN, INPUT);

  // Perform a self-test to check the status of the RFID sensor
  if (!mfrc522.PCD_PerformSelfTest()) {
    Serial.println("MFRC522 Self Test Error");
  }

  mfrc522.PCD_Init();  // Init MFRC522

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected :- ");
  Serial.println(WiFi.localIP());

  // Obtain and print the router's IP address
  Serial.print("Router's IP address: ");
  Serial.println(WiFi.gatewayIP());

  // Generate server URL
  serverUrl = "http://" + WiFi.gatewayIP().toString() + ":" + String(port) + endpoint;

  Serial.print("Servers's IP address: ");
  Serial.println(serverUrl);

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  } else {
    DateTime now = rtc.now();
    char currentTime[20];
    snprintf(currentTime, 20, "%04d-%02d-%02dT%02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    Serial.println("RTC Initialized...");
    Serial.println(currentTime);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}
void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  // Get the UID and check its corresponding status in the map
  String uidString = getUIDString();
  if (statusMap.find(uidString) == statusMap.end()) {
    statusMap[uidString] = "entry";
  } else if (statusMap[uidString] == "entry") {
    statusMap[uidString] = "exit";
  } else if (statusMap[uidString] == "exit") {
    statusMap[uidString] = "entry";
  }
  sendUIDToServer(uidString, statusMap[uidString]);

  if (digitalRead(IR_PIN == LOW)&& doorState = true)
    closeDoor();  // Close the door if motion is detected and the door is open
}

void sendUIDToServer(String uid, String status) {
  Serial.print(status);
  Serial.println("\n");
  Serial.print(uid);
  Serial.println("\n");
  DateTime now = rtc.now();
  char currentTime[20];
  char uidString[20];  // Changed variable name to uidString
  snprintf(currentTime, 20, "%04d-%02d-%02dT%02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  snprintf(uidString, 20, "%02X%02X%02X%02X", mfrc522.uid.uidByte[0], mfrc522.uid.uidByte[1], mfrc522.uid.uidByte[2], mfrc522.uid.uidByte[3]);

  HTTPClient http;

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    if (http.begin(client, serverUrl)) {
      Serial.println("Connected to server");

      // Create JSON payload
      char jsonBuffer[200];
      snprintf(jsonBuffer, 200, payloadTemplate, now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), uidString, status);  // Changed variable name to uidString

      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(jsonBuffer);

      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println(httpResponseCode);
        Serial.println(response);
        if (httpResponseCode == 200) {
          openDoor();
        }
      } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
      }

      http.end();
    } else {
      Serial.println("Connection to server failed");
    }
  } else {
    Serial.println("WiFi connection lost");
  }

  delay(5000);  // Adjust the delay as per your requirement
}

// Helper function to convert UID bytes to a string
String getUIDString() {
  char uidString[20];
  snprintf(uidString, 20, "%02X%02X%02X%02X", mfrc522.uid.uidByte[0], mfrc522.uid.uidByte[1], mfrc522.uid.uidByte[2], mfrc522.uid.uidByte[3]);
  return String(uidString);
}
// Function to open the door
void openDoor() {
  servo1.write(90);
  doorState = true;
  Serial.println("Door is Opened");
}

// Function to close the door
void closeDoor() {
  servo1.write(0);
  doorState = false;
  Serial.println("Door is Closed");
}
