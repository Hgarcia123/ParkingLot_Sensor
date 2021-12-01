#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>

#define onBoardLed 25
#define sensorSignal 17
#define led 2

//WiFiMulti wifiMulti;

using namespace websockets;

WebsocketsClient client;

int spotId = 2;
bool spotStatus;

bool control = false;
String token;

const char* ssid = ""; 
const char* pass = "";
const char* webSocketServer = "";

void setup() {
  Serial.begin(115200);
  
  //PINS SETUP
  pinMode(onBoardLed, OUTPUT);
  pinMode(sensorSignal, INPUT);
  //attachInterrupt(sensorSignal, movement, RISING);
  pinMode(led, OUTPUT);
  
  digitalWrite(onBoardLed, LOW);
  digitalWrite(led, LOW);

/*  if(!WiFi.config(local_IP, gateway, subnet)){
    Serial.println("Failed to configure static IP");
  }*/

  //WiFi SCAN
  WiFiScan();
  //WiFi SETUP
  WiFiSetup();

  //LoginAPI
  LoginAPI();

  //serverConnect();

  spotStatus = checkStatus(1);
  //Serial.println(spotStatus);
  Serial.println("Scanning for movement");
}

void loop() {
  bool sensor = digitalRead(sensorSignal);
  
  if(sensor == HIGH){
    if(control == false){
      digitalWrite(onBoardLed, HIGH);
      Serial.println("Movement detected!");
      
      toggleEntry();
      
      control = true; 
    }
  }else{
    digitalWrite(onBoardLed, LOW);
    //Serial.println("No movement detected");
    control = false;
  }
  delay(2000);
}

void WiFiScan(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.persistent(false);

  int n = WiFi.scanNetworks();
  Serial.println("Networks found: ");
  for(int i = 0; i<n; ++i){
    char currentSSID[64];
    Serial.print(WiFi.SSID(i)); Serial.print(" "); Serial.print(WiFi.BSSIDstr(i)); Serial.print(" "); Serial.print(" ("); Serial.print(WiFi.RSSI(i)); Serial.print(")");
    Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
  }
  Serial.println("Network scan finished\n");
  //waitForSerial();
}

void WiFiSetup(){
  int count = 0;
  
  Serial.print("Connecting to WiFi"); //Serial.println(ssid);
  
  while(WiFi.status() != WL_CONNECTED && count < 20){
    WiFi.begin(ssid, pass);
    Serial.print(".");
    ++count;
    delay(7000);
  }
  
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("Connection Timedout");
    Serial.println("Restarting...");
    delay(2000);
    ESP.restart();
  }
  Serial.println("");
  digitalWrite(led, HIGH);
  Serial.print("Connection established with IP Address: ");
  Serial.println(WiFi.localIP());
}

void LoginAPI(){
  
  if(WiFi.status() == WL_CONNECTED){
    StaticJsonDocument<JSON_OBJECT_SIZE(2)> doc;
    doc["userName"] = "SuperUser";
    doc["password"] = "Manager_2019";
    
    String payload;
    serializeJsonPretty(doc, payload);
    Serial.println("Logging in...");
    
    int responseCode = -1;
    HTTPClient http;
    while(responseCode != 200){  
      http.begin("http://*IP ADDRESS*/api/Authorization/LoginUser");
      http.addHeader("Content-Type", "application/json");
      
      responseCode = http.POST(payload);
      Serial.println(responseCode);
      delay(2000);
      token = "Bearer " + http.getString();
      token.replace("\"",""); //Remove quotation marks
      Serial.println(token);

      http.end();
    }
    Serial.println("Login successful!");
  }
}

void serverConnect (){
  Serial.println("Creating WebSocket");
  bool connected = client.connect(webSocketServer);
  if(connected){
    Serial.println("WebSocket established");
    client.send("Esp32 connected");
  }else{
    Serial.println("Connection failed");
  }
}

void toggleEntry(){
  if(WiFi.status() == WL_CONNECTED){
    int responseCode = -1;
    HTTPClient http;
    String payload;
    if(spotStatus == false){
      spotStatus = true;
      StaticJsonDocument<JSON_OBJECT_SIZE(6)> doc;
      doc["id"] = 0;
      doc["spotId"] = spotId;
      doc["plate"] = "AA-BB-11";
      doc["arrival"] = "2021-09-22T15:20:25.559Z";
      doc["departure"] = "2021-09-22T15:20:25.559Z";
      doc["pricePerHour"] = 10;
      
      serializeJsonPretty(doc, payload);
      Serial.println(payload);
      
      while(responseCode != 200){  
        http.begin("http://*IP ADDRESS*/api/Entry");
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Authorization", token);
      
        responseCode = http.POST(payload);
        Serial.println(responseCode);
        Serial.println(http.getString());
        
        http.end();
        delay(10);
      }
      serverConnect();
      client.send("Entry Created");
      client.close();
      
      responseCode = -1;
    }else{
      spotStatus = false;
      while(responseCode != 200){
        http.begin("http://*IP ADDRESS*/api/Entry");
        http.addHeader("Authorization", token);
        http.addHeader("id", String(spotId));

        responseCode = http.PUT("Sending PUT");
        Serial.println(responseCode);
        Serial.println(http.getString());

        http.end();
        delay(10);
      }
      
      serverConnect();
      client.send("Entry Closed");
      client.close();
      
      responseCode = -1;
    }
  }
}

bool checkStatus(int parkId){
  HTTPClient http;
  int responseCode = -1;
  String list;
  
  while(responseCode != 200){
    http.begin("http://*IP ADDRESS*/api/Search/FreeSpotsInPark");
    http.addHeader("Authorization", token);
    http.addHeader("parkId", String(parkId));

    responseCode = http.GET();

    //Serial.println(responseCode);
    list = http.getString();
    //Serial.println(list);

    http.end();
    delay(10);
  }
  DynamicJsonDocument doc(1024);
  auto error = deserializeJson(doc, list);
  if(error){
    Serial.println("Deserialization Error");
  }
  for(int i = 0; i < measureJson(doc); i++){
    int id = doc[i]["spot"]["id"];
    //Serial.println(id);
    if(id == spotId){
      return false; //Free Spot
    }
  }
  return true; //Ocuppied Spot
}

void waitForSerial(){
  Serial.println("Press any key to continue...");
  while(!Serial.available()){}
}
