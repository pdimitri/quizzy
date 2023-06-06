/*********
  Peter Dimitri
  @inovex..de

  Explanations of AccessPoint Demo Projekt at https://randomnerdtutorials.com/esp32-access-point-ap-web-server/
  Example with Asynch Webserver at https://randomnerdtutorials.com/esp32-wi-fi-manager-asyncwebserver/
*********/
#include <Arduino.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <AsyncTCP.h>
#include "SPIFFS.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino_JSON.h>
#include "YX5300Sound.h"

//Connection for the OLED Display
// VCC = 3.3V
//GND = GND
//SDA = 21
//SDL= 22

// Set GPIOs for LED_1 and PIR Motion Sensor
const int LED_1 = 5;
const int LED_2 = 19;

String color = "black";

struct button {
  int pin;
  String name;
  bool blocked;
  uint8_t score;
};

button player_1 = {4,"Player 1", false,0};
button player_2 ={18, "Player 2", false,0};
button BTN_reset = {13, "Reset", false};
button score_1_incr={12,"Score 1 Increase",false};
button score_2_incr={14,"Score 2 Increase",false};
button score_1_decr={27,"Score 1 Decrease",false};
button score_2_decr={26,"Score 2 Decrease",false};
button score_reset={25,"Score Reset",false};

int GPIO_State = 0;
int GPIO_lastState = 0;
int score_1_lastState = 0;
int score_2_lastState = 0;

static unsigned long last_interrupt_time = 0;
static unsigned long last_cleanup_time = 0;
static int8_t Send_buf[8] = {0};
int soundToPlay = 0;

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire, -1);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

JSONVar gameState;

void notifyClients(String gameState) {
  ws.textAll(gameState);
}

//Used to send Serial Command to the YX5300 UART MP3-Player
void sendCommand(byte command, byte option1, byte option2){
  Send_buf[0] = 0x7e;
  Send_buf[1] = 0xff;
  Send_buf[2] = 0x06;
  Send_buf[3] = command;
  Send_buf[4] = 0x00;
  Send_buf[5] = option1;
  Send_buf[6] = option2;
  Send_buf[7] = 0xef;

  for(uint8_t i=0; i<8; i++){
    Serial2.write(Send_buf[i]);
  }
}

// Event Handler Interrupt Function for Buzzer 1
void IRAM_ATTR Btn_1_pressed() {
  
  if(!player_1.blocked){
    player_2.blocked = true;
    digitalWrite(LED_1, HIGH);
    GPIO_State=1;
    soundToPlay = 1;
  }
  
}

void IRAM_ATTR Btn_2_pressed() {
  if(!player_2.blocked){
    player_1.blocked = true;
    digitalWrite(LED_2, HIGH);
    GPIO_State=2;
    soundToPlay = 2;
  }
}

void IRAM_ATTR reset_round() {
  //Serial.println("Reset detected");
  player_1.blocked = false;
  player_2.blocked = false;
  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, LOW);
  GPIO_State = 0;
  
}

void IRAM_ATTR score_1_increase() {
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200){
    //Serial.println("Reset detected");
    player_1.blocked = false;
    player_2.blocked = false;
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    GPIO_State = 0;
    player_1.score++;
    
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR score_1_decrease() {
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200){
    //Serial.println("Reset detected");
    player_1.blocked = false;
    player_2.blocked = false;
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    GPIO_State = 0;
    if(player_1.score >0){
      player_1.score--;
    }
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR score_2_increase() {
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200){
    player_1.blocked = false;
    player_2.blocked = false;
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    GPIO_State = 0;
    player_2.score++;
    
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR score_2_decrease() {
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200){
    player_1.blocked = false;
    player_2.blocked = false;
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    GPIO_State = 0;
    if(player_2.score >0){
        player_2.score--;
    }
    
    
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR score_reset_all() {
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200){
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    GPIO_State = 0;
    player_1.score = 0;
    player_2.score = 0;
    
  }
  last_interrupt_time = interrupt_time;
}

String getGameState(){
  gameState["score_1"] = player_1.score;
  gameState["score_2"] = player_2.score;
  gameState["GPIO_State"] = GPIO_State;

  String jsonString = JSON.stringify(gameState);
  return jsonString;
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle") == 0) {
      GPIO_State = !GPIO_State;
      notifyClients(getGameState());
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String& var){
  Serial.println(var);
  if(var == "STATE"){
    if (GPIO_State){
      return "ON";
    }
    else{
      return "OFF";
    }
  }
}


void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  Serial.println("Setup started");

  //Serial Port for YX5300 Sound Module TS PIN 16, TX PIN 17
  Serial2.begin(9600, SERIAL_8N1,16,17);
  delay(500);

  //Initializing Sound Module and choosing SD-Card Reader
  sendCommand(CMD_SEL_DEV,0,DEV_TF);
  sendCommand(CMD_SET_VOLUME,0x00,0x1E);
  delay(200);

  
  // Configure Input GPIOs
  pinMode(player_1.pin, INPUT_PULLUP);
  pinMode(player_2.pin, INPUT_PULLUP);
  pinMode(BTN_reset.pin, INPUT_PULLUP);
  pinMode(score_1_incr.pin,INPUT_PULLUP);
  pinMode(score_2_incr.pin,INPUT_PULLUP);
  pinMode(score_1_decr.pin,INPUT_PULLUP);
  pinMode(score_2_decr.pin,INPUT_PULLUP);
  pinMode(score_reset.pin,INPUT_PULLUP);

  // Set up Interrupts
  attachInterrupt(digitalPinToInterrupt(player_1.pin), Btn_1_pressed, RISING);
  attachInterrupt(digitalPinToInterrupt(player_2.pin), Btn_2_pressed, RISING);
  attachInterrupt(digitalPinToInterrupt(BTN_reset.pin), reset_round, RISING);
  attachInterrupt(digitalPinToInterrupt(score_1_incr.pin), score_1_increase, RISING);
  attachInterrupt(digitalPinToInterrupt(score_1_decr.pin), score_1_decrease, RISING);
  attachInterrupt(digitalPinToInterrupt(score_2_incr.pin), score_2_increase, RISING);
  attachInterrupt(digitalPinToInterrupt(score_2_decr.pin), score_2_decrease, RISING);
  attachInterrupt(digitalPinToInterrupt(score_reset.pin), score_reset_all, RISING);

  // Configure LEDs
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, LOW);

  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }else{
   Serial.println("SPIFFS mounted successfully");
  }

    // Connect to Wi-Fi
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("Quizzy", NULL);

    IPAddress IP = WiFi.softAPIP(); 
    Serial.print("AP IP address: ");
    Serial.println(IP);

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/page.html","text/html", false, processor);
  });

  server.serveStatic("/", SPIFFS, "/");
  
  // Start server
  server.begin();

  display.begin(SSD1306_SWITCHCAPVCC,0x03C);
  display.setTextSize(1);
  display.setTextColor(1,0);
  display.clearDisplay();

  
}

void loop() {
  if(soundToPlay>0){
    sendCommand(CMD_PLAY_FOLDER_FILE,1,soundToPlay);
    soundToPlay=0;
  }

  if((GPIO_State != GPIO_lastState)|| (player_1.score != score_1_lastState) || (player_2.score != score_2_lastState)){
    notifyClients(getGameState());
    GPIO_lastState = GPIO_State;
    score_1_lastState = player_1.score;
    score_2_lastState = player_2.score;
  }
 
  display.clearDisplay();
  display.setCursor(15,10);
  display.setTextSize(1);
  display.println("Team 1     Team 2" );
  display.println("");
  display.setCursor(20,30);
  if(player_1.score < 10 && player_2.score <10 ){
      display.setTextSize(3);
  }else{
      display.setTextSize(2);
  }
  display.println(String(player_1.score) + " : " + String(player_2.score));
  display.display();
  
  /* unsigned long cleanup_time = millis();
  if (cleanup_time - last_cleanup_time > 4000){
    ws.cleanupClients(); 
    last_cleanup_time = cleanup_time;
  } */

}
