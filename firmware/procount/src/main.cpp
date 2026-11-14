#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include <TM1637Display.h>
#include "config.h"
#include "secrets.h"

#if MQTT_USE_TLS
WiFiClientSecure networkClient;
#else
WiFiClient networkClient;
#endif
PubSubClient mqtt(networkClient);

MFRC522 rfid(RFID_SS, RFID_RST);
class Max7219Matrix {
 public:
  explicit Max7219Matrix(uint8_t cs):cs_(cs){}
  void begin(){pinMode(cs_,OUTPUT);digitalWrite(cs_,HIGH);send(0x0F,0);send(0x0C,1);send(0x0B,7);send(0x09,0);clear();}
  void setRow(uint8_t row,uint8_t value){if(row<8)send(row+1,value);}
  void shutdown(bool enabled){send(0x0C,enabled?0:1);}
  void setIntensity(uint8_t value){send(0x0A,value&0x0F);}
  void clear(){for(uint8_t r=0;r<8;r++)send(r+1,0);}
 private:
  uint8_t cs_;
  void send(uint8_t reg,uint8_t value){SPI.beginTransaction(SPISettings(1000000,MSBFIRST,SPI_MODE0));digitalWrite(cs_,LOW);SPI.transfer(reg);SPI.transfer(value);digitalWrite(cs_,HIGH);SPI.endTransaction();}
};
Max7219Matrix matrix(MATRIX_CS);
TM1637Display digits(SEG_CLK, SEG_DIO);
byte rowPins[4] = {KEYPAD_ROWS[0],KEYPAD_ROWS[1],KEYPAD_ROWS[2],KEYPAD_ROWS[3]};
byte colPins[4] = {KEYPAD_COLS[0],KEYPAD_COLS[1],KEYPAD_COLS[2],KEYPAD_COLS[3]};
char keymap[4][4]={{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
Keypad keypad=Keypad(makeKeymap(keymap),rowPins,colPins,4,4);
String topicCommand, topicStatus, topicEvent;
String selectedUser="A", entered="", activeRequest="";
String enrolledTags[4];
uint8_t enrolledCount=0, animationFrame=0;
uint32_t remainingSteps=0, lastStepAt=0, lastStatusAt=0, lastRFIDAt=0, lastAnimationAt=0;
uint8_t stepPhase=0; bool dispensing=false;
const uint8_t phases[8]={0b0001,0b0011,0b0010,0b0110,0b0100,0b1100,0b1000,0b1001};

void setLights(bool red,bool yellow,bool green){
  if(LED_RED>=0) digitalWrite(LED_RED,red?HIGH:LOW);
  if(LED_YELLOW>=0) digitalWrite(LED_YELLOW,yellow?HIGH:LOW);
  if(LED_GREEN>=0) digitalWrite(LED_GREEN,green?HIGH:LOW);
}
void face(const uint8_t rows[8]){for(int i=0;i<8;i++) matrix.setRow(i,rows[i]);}
void showMood(const char* mood){
  static const uint8_t ready[8]={0x3C,0x42,0xA5,0x81,0xA5,0x99,0x42,0x3C};
  static const uint8_t work[8] ={0x3C,0x42,0xA5,0x81,0xBD,0x81,0x42,0x3C};
  static const uint8_t done[8] ={0x3C,0x42,0xA5,0x81,0xA5,0x81,0x5A,0x3C};
  static const uint8_t err[8]  ={0x3C,0x42,0xA5,0x81,0x99,0xA5,0x42,0x3C};
  if(!strcmp(mood,"work"))face(work);else if(!strcmp(mood,"done"))face(done);else if(!strcmp(mood,"error"))face(err);else face(ready);
}
void showProfileFace(uint8_t profile){
  static const uint8_t faces[4][8]={
    {0x3C,0x42,0xA5,0x81,0xA5,0x99,0x42,0x3C},
    {0x3C,0x42,0xA5,0x81,0xBD,0x81,0x42,0x3C},
    {0x3C,0x42,0x81,0xA5,0x81,0x99,0x42,0x3C},
    {0x3C,0x42,0xA5,0x81,0xA5,0x81,0x5A,0x3C}
  };
  face(faces[profile%4]);
}
void animateReady(){
  static const uint8_t frames[4][8]={
    {0x3C,0x42,0xA5,0x81,0xA5,0x99,0x42,0x3C},
    {0x3C,0x42,0x81,0xBD,0x81,0x99,0x42,0x3C},
    {0x3C,0x42,0x85,0x81,0xA5,0x99,0x42,0x3C},
    {0x3C,0x42,0xA5,0x81,0x81,0xBD,0x42,0x3C}
  };
  if(millis()-lastAnimationAt>=350){lastAnimationAt=millis();face(frames[animationFrame%4]);animationFrame++;}
}
void publishStatus(const char* state,const char* message=""){
  Serial.printf("[STATUS] %s | %s | user=%s | grams=%ld\n",state,message,selectedUser.c_str(),(long)entered.toInt());
  if(!mqtt.connected()){Serial.println("[MQTT] Status not published: broker is disconnected.");return;}
  JsonDocument d; d["deviceId"]=MQTT_DEVICE_ID;d["state"]=state;d["message"]=message;
  d["user"]=selectedUser;d["targetGrams"]=entered.toInt();d["estimatedGrams"]=dispensing?entered.toInt():0;
  if(activeRequest.length()) d["requestId"]=activeRequest;
  char out[300];size_t n=serializeJson(d,out,sizeof(out));mqtt.publish(topicStatus.c_str(),reinterpret_cast<const uint8_t*>(out),n,true);
}
void motorWrite(uint8_t p){for(int i=0;i<4;i++)digitalWrite(MOTOR_PINS[i],(p&(1<<i))?HIGH:LOW);}
void reject(const String& id,const char* reason){Serial.printf("[DISPENSE] Rejected request %s: %s\n",id.c_str(),reason);activeRequest=id;showMood("error");setLights(true,false,false);publishStatus("error",reason);}
void beginDispense(uint16_t grams,const String& who,const String& requestId){
  selectedUser=who;activeRequest=requestId;
  if(grams==0||grams>MAX_TARGET_GRAMS){reject(requestId,"Target outside allowed range");return;}
  if(STEPS_PER_GRAM<=0){reject(requestId,"Calibration required: set STEPS_PER_GRAM after external-scale calibration");return;}
  if(dispensing){reject(requestId,"Dispenser is busy");return;}
  entered=String(grams);remainingSteps=(uint32_t)(grams*STEPS_PER_GRAM+0.5f);
  if(remainingSteps==0){reject(requestId,"Invalid calibration");return;}
  Serial.printf("[DISPENSE] Starting request %s: target=%u g, estimated_steps=%lu, user=%s\n",requestId.c_str(),grams,(unsigned long)remainingSteps,who.c_str());
  dispensing=true;lastStepAt=millis();setLights(false,true,false);showMood("work");digits.showNumberDec(grams,false);publishStatus("dispensing","Step-count estimate; output is not weighed");
}
void onMqtt(char* topic,byte* payload,unsigned int length){
  if(String(topic)!=topicCommand)return;
  Serial.printf("[MQTT] Command received on %s\n",topic);
  JsonDocument d;auto e=deserializeJson(d,payload,length);if(e){Serial.printf("[MQTT] Invalid JSON: %s\n",e.c_str());publishStatus("error","Invalid JSON command");return;}
  String rid=d["requestId"]|"";String who=d["user"]|"A";uint16_t grams=d["grams"]|0;
  Serial.printf("[MQTT] Parsed request=%s user=%s target=%u g\n",rid.c_str(),who.c_str(),grams);
  if(rid.length()==0||rid==activeRequest){publishStatus("error","Missing or repeated requestId");return;}
  beginDispense(grams,who,rid);
}
void mqttReconnect(){
  static uint32_t retryAt=0;if(mqtt.connected()||millis()-retryAt<3000)return;retryAt=millis();
  String id=String(MQTT_DEVICE_ID)+"-"+String((uint32_t)ESP.getEfuseMac(),HEX);
  Serial.printf("[MQTT] Connecting to %s:%u as %s\n",MQTT_HOST,MQTT_PORT,id.c_str());
  bool connected;
  if(strlen(MQTT_USERNAME)>0){
    connected=mqtt.connect(id.c_str(),MQTT_USERNAME,MQTT_PASSWORD,topicStatus.c_str(),1,true,"{\"state\":\"offline\"}");
  }else{
    connected=mqtt.connect(id.c_str(),topicStatus.c_str(),1,true,"{\"state\":\"offline\"}");
  }
  if(connected){Serial.printf("[MQTT] Connected; subscribed to %s\n",topicCommand.c_str());mqtt.subscribe(topicCommand.c_str(),1);publishStatus("ready","Connected to HiveMQ; awaiting user");}
  else{Serial.printf("[MQTT] Connection failed, client state=%d; retrying in 3 seconds.\n",mqtt.state());}
}
void handleKey(char k){
  if(k>='A'&&k<='D'&&!dispensing){selectedUser=String(k);entered="";Serial.printf("[KEYPAD] Profile %s selected.\n",selectedUser.c_str());showMood("ready");setLights(true,false,false);publishStatus("ready","Profile selected on keypad");return;}
  if(k>='0'&&k<='9'&&!dispensing){if(entered.length()<3)entered+=k;digits.showNumberDec(entered.toInt(),false);return;}
  if(k=='*'&&!dispensing){entered="";digits.clear();showMood("ready");return;}
  if(k=='#'&&!dispensing){beginDispense(entered.toInt(),selectedUser,"local-"+String(millis()));}
}
void handleRFID(){
  if(!rfid.PICC_IsNewCardPresent()||!rfid.PICC_ReadCardSerial())return;
  String uid="";for(byte i=0;i<rfid.uid.size;i++){if(rfid.uid.uidByte[i]<16)uid+='0';uid+=String(rfid.uid.uidByte[i],HEX);}uid.toUpperCase();
  int profile=uid==RFID_UID_A?0:uid==RFID_UID_B?1:-1;
  if(profile>=0){selectedUser=String((char)('A'+profile));digits.showNumberDec(profile+1,false);showProfileFace(profile);Serial.printf("[RFID] Detected profile %c | UID %s\n",'A'+profile,uid.c_str());String message="Profile "+selectedUser+" selected";publishStatus("user_detected",message.c_str());}
  else{selectedUser="UNASSIGNED";showMood("error");Serial.printf("[RFID] Unknown tag UID %s; no profile assigned.\n",uid.c_str());publishStatus("error","Unknown RFID tag");}
  lastRFIDAt=millis();rfid.PICC_HaltA();rfid.PCD_StopCrypto1();
}
void tickMotor(){
  if(!dispensing)return;
  uint32_t now=millis();if(now-lastStepAt<STEP_INTERVAL_MS)return;lastStepAt=now;
  motorWrite(phases[stepPhase]);stepPhase=(stepPhase+1)&7;remainingSteps--;
  if(remainingSteps==0){motorWrite(0);dispensing=false;Serial.printf("[DISPENSE] Complete. Estimated target=%s g; mass was not measured.\n",entered.c_str());setLights(false,false,true);showMood("done");publishStatus("complete","Dispense finished; grams are estimated, not measured");}
}
void setup(){
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[ProCount] Booting firmware.");
  Serial.println("[TEST] RFID profile map loaded from local secrets. Idle matrix animates.");
  Serial.println("[TEST] Keypad scan disabled while D4/GPIO4 is used for RFID RST.");
  Serial.printf("[ProCount] Device ID: %s | HiveMQ: %s:%u | calibration: %.3f steps/g\n",MQTT_DEVICE_ID,MQTT_HOST,MQTT_PORT,STEPS_PER_GRAM);
  for(auto p:MOTOR_PINS)pinMode(p,OUTPUT);
  motorWrite(0);
  for(int p:{LED_RED,LED_YELLOW,LED_GREEN})if(p>=0)pinMode(p,OUTPUT);
  setLights(true,false,false);digits.setBrightness(4);digits.clear();
  SPI.begin(BUS_SCK,BUS_MISO,BUS_MOSI,RFID_SS);matrix.begin();matrix.shutdown(false);matrix.setIntensity(4);matrix.clear();showMood("ready");rfid.PCD_Init();
  String base=String(TOPIC_ROOT)+MQTT_DEVICE_ID;topicCommand=base+"/command";topicStatus=base+"/status";topicEvent=base+"/event";
  WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  Serial.printf("[WiFi] Connecting to SSID: %s\n",WIFI_SSID);
#if MQTT_USE_TLS
  networkClient.setCACert(MQTT_CA_CERT);
#endif
  mqtt.setServer(MQTT_HOST,MQTT_PORT);mqtt.setCallback(onMqtt);mqtt.setBufferSize(512);mqtt.setKeepAlive(30);
}
void loop(){
  static wl_status_t lastWiFiStatus=WL_IDLE_STATUS;
  wl_status_t currentWiFiStatus=WiFi.status();
  if(currentWiFiStatus!=lastWiFiStatus){
    lastWiFiStatus=currentWiFiStatus;
    if(currentWiFiStatus==WL_CONNECTED)Serial.printf("[WiFi] Connected. IP=%s\n",WiFi.localIP().toString().c_str());
    else Serial.printf("[WiFi] Status changed: %d\n",(int)currentWiFiStatus);
  }
  if(currentWiFiStatus==WL_CONNECTED){mqttReconnect();mqtt.loop();}
  if(KEYPAD_ENABLED){char k=keypad.getKey();if(k)handleKey(k);}handleRFID();tickMotor();
  if(!dispensing&&millis()-lastRFIDAt>1800)animateReady();
}
