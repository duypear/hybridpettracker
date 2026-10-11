// =========================================================
// SUCA BOARD - ESP32-C3 Gateway với WiFi + P2P LoRa
// Pet Tracker Gateway - FIXED VERSION với Timeout Detection
// =========================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ==================== WIFI CONFIG ====================
const char* ssid = "DANANG SOFTWARE PARK";
const char* password = "";

// ==================== API CONFIG ====================
const char* API_URL = "https://api.lpwanmapper.com/webhook/thanhduy1332004";
const char* API_TOKEN = "f6f90d4e-0af7-448b-a65d-e3b09e20cbda";

// ==================== P2P LORA CONFIG ====================
HardwareSerial mySerial1(1);
int rxPin = 20;
int txPin = 21;

// Buffer RSSI - lưu 5 mẫu để tính trung bình
int rssiBuffer[5];
int sampleCount = 0;

// Ngưỡng RSSI để xác định pet ở nhà hay không
// Thay bằng:
int RSSI_THRESHOLD_HOME = -80;      // Pet ở nhà
int RSSI_THRESHOLD_NEAR = -90;     // Pet lân cận
// RSSI < -100 → Pet ở xa (sẽ chuyển LoRaWAN)

// ==================== PET CONFIG ====================
String petName = "Mèo Miu";
String petDeviceId = "node-001";

// ==================== HOME LOCATION ====================
double homeLat = 16.070749;
double homeLon = 108.168314;

// ==================== STATE VARIABLES ====================
bool lastPetStatus = true;  // true = at home, false = away
unsigned long lastSendTime = 0;
unsigned long SEND_INTERVAL = 3000;
bool wifiConnected = false;

// ✅ TIMEOUT DETECTION - KEY FIX
unsigned long lastPacketTime = 0;        // Timestamp của packet cuối cùng
unsigned long PACKET_TIMEOUT = 15000;    // 15 giây không nhận packet = offline
bool isNodeOnline = false;               // Trạng thái node online/offline

unsigned long lastHeartbeat = 0;
unsigned long HEARTBEAT_INTERVAL = 5000;
unsigned long lastAPICall = 0;

// ==================== SETUP ====================
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("  SUCA BOARD - PET TRACKER GATEWAY");
  Serial.println("  API: LPWAN Mapper (FIXED VERSION)");
  Serial.println("========================================\n");

  // Setup LED pins
  pinMode(10, OUTPUT); 
  pinMode(4, OUTPUT);  

  // LED blink khi khởi động
  digitalWrite(4, HIGH);
  delay(500);
  digitalWrite(4, LOW);
  delay(500);
  digitalWrite(10, HIGH);

  // ==================== CONNECT WIFI ====================
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n✓ WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    digitalWrite(4, HIGH);
  } else {
    Serial.println("\n✗ WiFi Failed!");
    wifiConnected = false;
  }

  // ==================== SETUP P2P LORA ====================
  pinMode(txPin, OUTPUT);
  pinMode(rxPin, INPUT);

  mySerial1.begin(115200, SERIAL_8N1, rxPin, txPin);
  delay(1000);

  Serial.println("\nInitializing LoRa P2P...");
  
  mySerial1.println("AT");
  delay(300);
  while (mySerial1.available()) Serial.write(mySerial1.read());

  // Chuyển sang P2P mode
  mySerial1.println("AT+NWM=0");
  delay(300);
  while (mySerial1.available()) Serial.write(mySerial1.read());

  // Cấu hình P2P: Freq=923MHz, SF=10, BW=125kHz, CR=4/5, Preamble=8, Power=14dBm
  mySerial1.println("AT+P2P=923000000:10:0:1:8:14");
  delay(300);
  while (mySerial1.available()) Serial.write(mySerial1.read());

  // Bật chế độ nhận liên tục
  mySerial1.println("AT+PRECV=65533");
  delay(300);
  while (mySerial1.available()) Serial.write(mySerial1.read());

  Serial.println("✓ P2P Receiver Ready!");
  Serial.println("\n========================================");
  Serial.println("System Started - Waiting for Pet Node...");
  Serial.println("Timeout: 15 seconds without packet = OFFLINE");
  Serial.println("========================================\n");
}

// ==================== SEND DATA TO LPWAN MAPPER API ====================
void sendToAPI(String petName, String deviceId, int rssi, String status, double lat, double lon, bool isOnline)
{
  // ✅ CHECK WIFI TRƯỚC KHI GỬI
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi disconnected, reconnecting...");
    WiFi.reconnect();
    delay(2000);
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("✗ WiFi reconnect failed, skipping API call");
      wifiConnected = false;
      digitalWrite(4, LOW);
      return;
    } else {
      wifiConnected = true;
      digitalWrite(4, HIGH);
    }
  }

  HTTPClient http;
  http.setTimeout(15000);  // 15 giây timeout
  
  if (!http.begin(API_URL)) {
    Serial.println("✗ HTTP begin failed!");
    return;
  }
  
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + API_TOKEN);

  // ✅ TẠO JSON VỚI TRẠNG THÁI CHÍNH XÁC
  StaticJsonDocument<512> doc;
  
  doc["pet_name"] = petName;
  doc["device_id"] = deviceId;
  doc["devEUI"] = deviceId;
  doc["rssi"] = isOnline ? rssi : -999;  // ✅ -999 nếu offline
doc["pet_status"] = status;  // "home", "near", "away", "offline"
doc["is_home"] = (status == "home");  // Giữ tương thích ngược// ✅ false nếu offline
  doc["is_p2p"] = true;
  doc["data_source"] = "P2P";
  doc["is_online"] = isOnline;           // ✅ Thêm flag online/offline          // ✅ Thêm flag online/offline
  
  JsonObject location = doc.createNestedObject("location");
  location["latitude"] = lat;
  location["longitude"] = lon;
  
  doc["timestamp"] = millis();

  String jsonString;
  serializeJson(doc, jsonString);

  Serial.println("\n>>> Sending to LPWAN Mapper API:");
  Serial.println(jsonString);

  // GỬI REQUEST
  int httpCode = http.POST(jsonString);

  if (httpCode > 0) {
    Serial.printf("✓ HTTP Response: %d\n", httpCode);
    
    if (httpCode == 200 || httpCode == 201) {
      String response = http.getString();
      Serial.println("API response: " + response);
      
      // Blink LED success
      digitalWrite(4, LOW);
      delay(100);
      digitalWrite(4, HIGH);
    } else {
      Serial.printf("⚠️ Unexpected status: %d\n", httpCode);
      Serial.println(http.getString());
    }
  } else {
    Serial.printf("✗ HTTP Error: %s\n", http.errorToString(httpCode).c_str());
    
    // Debug error types
    if (httpCode == HTTPC_ERROR_CONNECTION_REFUSED) {
      Serial.println("→ Connection refused (check server/firewall)");
    } else if (httpCode == HTTPC_ERROR_CONNECTION_LOST) {
      Serial.println("→ Connection lost (check WiFi)");
    } else if (httpCode == HTTPC_ERROR_READ_TIMEOUT) {
      Serial.println("→ Server timeout");
    }
  }
 lastAPICall = millis();   // Cập nhật timestamp mỗi khi gửi API thành công
  
  http.end();
  http.end();
}

// ==================== ✅ CHECK TIMEOUT ====================
// ==================== ✅ CHECK TIMEOUT - FIXED ====================
void checkNodeTimeout() {
  unsigned long now = millis();
  
  if (lastPacketTime == 0) {
    isNodeOnline = false;
    return;
  }
  
  unsigned long timeSinceLastPacket = now - lastPacketTime;
  
  if (timeSinceLastPacket > PACKET_TIMEOUT) {
    if (isNodeOnline) {
      Serial.println("\n⚠️⚠️⚠️ P2P TIMEOUT ⚠️⚠️⚠️");
      Serial.printf("No P2P packet for %lu ms (> %lu ms)\n", timeSinceLastPacket, PACKET_TIMEOUT);
      Serial.println("→ Node may have switched to LoRaWAN or is offline");
      Serial.println("→ SUCA will STOP sending to API\n");
      
      isNodeOnline = false;
      sampleCount = 0;
      
      // ✅ KHÔNG GỬI API - Để Web tự phát hiện offline qua timestamp
    }
  } else {
    if (!isNodeOnline) {
      Serial.println("\n✅✅✅ P2P ONLINE ✅✅✅\n");
    }
    isNodeOnline = true;
  }
}

// ==================== MAIN LOOP ====================
void loop() 
{
  // ✅ KIỂM TRA NODE TIMEOUT MỖI VÒNG LẶP
  checkNodeTimeout();
  
  // Kiểm tra WiFi
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    digitalWrite(4, LOW);
    Serial.println("⚠️ WiFi disconnected, reconnecting...");
    WiFi.reconnect();
    delay(5000);
    return;
  } else {
    if (!wifiConnected) {
      Serial.println("✓ WiFi reconnected!");
      wifiConnected = true;
      digitalWrite(4, HIGH);
    }
  }

  // Đọc dữ liệu từ LoRa module
  if (mySerial1.available()) {
    String line = mySerial1.readStringUntil('\n');
    line.trim();

    // Nhận được gói tin P2P
    if (line.startsWith("+EVT:RXP2P")) {
      Serial.println("\n==== PACKET RECEIVED ====");
      Serial.println(line);

      // ✅ CẬP NHẬT TIMESTAMP PACKET
      lastPacketTime = millis();
      isNodeOnline = true;

      // Parse RSSI từ response
      // Format: +EVT:RXP2P:RSSI:-XX:SNR:YY
      int c1 = line.indexOf(':');
      int c2 = line.indexOf(':', c1 + 1);
      int c3 = line.indexOf(':', c2 + 1);

      if (c1 < 0 || c2 < 0 || c3 < 0) {
        Serial.println("⚠️ Invalid packet format");
        return;
      }

      String rssiStr = line.substring(c2 + 1, c3);
      int rssi = rssiStr.toInt();

      Serial.print("RSSI sample = ");
      Serial.println(rssi);

      // Lưu RSSI vào buffer
      rssiBuffer[sampleCount] = rssi;
      sampleCount++;
   
if (sampleCount == 1) {
  if (rssi > RSSI_THRESHOLD_HOME) {
    Serial.println("✅ Sample 1: RSSI good → Sending STAY_HOME (0x30)");
    mySerial1.println("AT+PSEND=30");
  } else 
  if (rssi > RSSI_THRESHOLD_NEAR){
    Serial.println("⚠️ Sample 2: RSSI weak → Sending LEAVE_HOME (0x31)");
    mySerial1.println("AT+PSEND=32");
  }
  else {
    Serial.println("⚠️ Sample 2: RSSI weak → Sending LEAVE_HOME (0x31)");
    mySerial1.println("AT+PSEND=31");
  }
}

      // Nếu chưa đủ 5 mẫu thì chờ tiếp
      if (sampleCount < 5) {
        Serial.print("Collected samples: ");
        Serial.print(sampleCount);
        Serial.println("/5\n");
        return;
      }

      // ================================
      // ĐỦ 5 MẪU → TÍNH TRUNG BÌNH
      // ================================
  // ================================
// ĐỦ 5 MẪU → TÍNH TRUNG BÌNH
// ================================
int sum = 0;
for (int i = 0; i < 5; i++) {
  sum += rssiBuffer[i];
}
int rssiAvg = sum / 5;

Serial.print("\n>>> RSSI AVG (5 samples) = ");
Serial.println(rssiAvg);

// ================================
// ✅ QUYẾT ĐỊNH TRẠNG THÁI PET
// ================================
String petStatus;
uint8_t commandToSend = 0x00;  // Mặc định không gửi

if (rssiAvg > RSSI_THRESHOLD_HOME) {
    // ✅ Case 1: Pet ở nhà
    petStatus = "home";
    Serial.println("✅ Pet AT HOME");
    commandToSend = 0x30;  // STAY_HOME
    
} else if (rssiAvg > RSSI_THRESHOLD_NEAR) {
    // ⚠️ Case 2: Pet lân cận (vùng cảnh báo)
    petStatus = "near";
    Serial.println("⚠️ Pet NEAR HOME (Warning Zone)");
    commandToSend = 0x32;  // ✅ Command mới: NEAR_HOME
    
} else {
    // 🚪 Case 3: Pet đã ra xa
    petStatus = "away";
    Serial.println("🚪 Pet AWAY FROM HOME");
    commandToSend = 0x31;  // LEAVE_HOME
}

// ✅ GỬI COMMAND CHO NODE
static String lastStatus = "";
static unsigned long lastCommandTime = 0;

if (petStatus != lastStatus || (millis() - lastCommandTime > 15000)) {
    Serial.printf("→ Sending command 0x%02X to node\n", commandToSend);
    mySerial1.printf("AT+PSEND=%02X\n", commandToSend);
    lastCommandTime = millis();
    lastStatus = petStatus;
}

// ✅ GỬI LÊN API
sendToAPI(petName, petDeviceId, rssiAvg, petStatus, homeLat, homeLon, true);

// RESET chu kỳ
sampleCount = 0;
lastSendTime = millis();
    }
  }

  // ✅ HEARTBEAT ĐỊNH KỲ - LOGIC MỚI
 // ✅ HEARTBEAT ĐỊNH KỲ - FIXED VERSION
// ✅ UNIFIED HEARTBEAT - NEW LOGIC
if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
  Serial.println("\n[Heartbeat Check]");
  
  // ✅ KIỂM TRA: Đã gửi API trong 5s qua chưa?
  unsigned long timeSinceLastAPI = millis() - lastAPICall;
  
  if (timeSinceLastAPI < HEARTBEAT_INTERVAL) {
    // ✅ ĐÃ GỬI API gần đây (qua P2P) → SKIP heartbeat
    Serial.printf("→ Skip heartbeat (API sent %lu ms ago)\n", timeSinceLastAPI);
  } 
else if (isNodeOnline && sampleCount > 0) {
    // Node online NHƯNG chưa gửi API → gửi trạng thái hiện tại
    Serial.println("→ Node online - sending current state");
    int sum = 0;
    for (int i = 0; i < sampleCount; i++) {
      sum += rssiBuffer[i];
    }
    int rssiAvg = sum / sampleCount;
    
    // ✅ XÁC ĐỊNH TRẠNG THÁI
    String petStatus;
    if (rssiAvg > RSSI_THRESHOLD_HOME) {
        petStatus = "home";
    } else if (rssiAvg > RSSI_THRESHOLD_NEAR) {
        petStatus = "near";
    } else {
        petStatus = "away";
    }
    
    sendToAPI(petName, petDeviceId, rssiAvg, petStatus, homeLat, homeLon, true);
}
  else {
    // ✅ Node offline → gửi ALIVE PING
    Serial.println("→ Node offline - sending SUCA alive ping");
    
    HTTPClient http;
    http.setTimeout(15000);
    
    if (!http.begin(API_URL)) {
      Serial.println("✗ HTTP begin failed!");
      lastHeartbeat = millis();  // Reset để retry sau
      return;
    }
    
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + API_TOKEN);

    StaticJsonDocument<512> doc;
    
    doc["pet_name"] = petName;
    doc["device_id"] = petDeviceId;
    doc["devEUI"] = petDeviceId;  
    doc["rssi"] = -999;
    doc["is_home"] = false;
    doc["is_p2p"] = true;
    doc["data_source"] = "P2P_NO_CONTACT";
    doc["is_online"] = false;
    doc["suca_status"] = "online";
    doc["node_status"] = "offline_or_lorawan";
    
    JsonObject location = doc.createNestedObject("location");
    location["latitude"] = homeLat;
    location["longitude"] = homeLon;
    
    doc["timestamp"] = millis();

    String jsonString;
    serializeJson(doc, jsonString);

    int httpCode = http.POST(jsonString);
    
    if (httpCode > 0) {
      Serial.printf("✓ Sent SUCA alive ping: %d\n", httpCode);
      lastAPICall = millis();  // ✅ Cập nhật timestamp
    }
    
    http.end();
  }
  
  lastHeartbeat = millis();
}

  delay(100);
}