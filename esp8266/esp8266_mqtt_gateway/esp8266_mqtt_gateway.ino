#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
// ==================== WiFi 配置 ====================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASS     = "YOUR_WIFI_PASSWORD";
// ==================== MQTT 配置 ====================
const char* MQTT_SERVER   = "YOUR_MQTT_BROKER_IP";
const int   MQTT_PORT     = 1883;
// ==================== MQTT 主题 ====================
const char* TOPIC_FALL    = "device/elder_2/fall";
const char* TOPIC_PERSON  = "device/elder_2/presence";
const char* TOPIC_GAZE_CMD = "device/elder_2/gaze_cmd";
const char* TOPIC_MPU6050 = "device/elder_2/mpu6050_flag";
const char* TOPIC_GPS     = "device/elder_2/gps";
const char* TOPIC_MEDICINE= "device/elder_2/medicine_box";
// =====新增：云台自动搜索控制主题=====
const char* TOPIC_SEARCH_CTRL = "device/elder_2/camera_follow/control";
// =====新增：呼叫主题=====
const char* TOPIC_CALL = "device/elder_2/call";
// ==================== 状态变量 ====================
String lastGazeDirection = "center";
String lastMcuCommand = "";
unsigned long lastGazeTime = 0;
unsigned long lastMcuTime = 0;
const unsigned long DATA_TIMEOUT = 10000;
// ==================== 对象 ====================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
SoftwareSerial serialK230(D2, D1);
SoftwareSerial serial_mcu(D3, D4);   // 接 GD32H759I-EVAL 主控;
//=====新增天问模块软串口  RX:D5, TX:D6=====
SoftwareSerial serialTianwen(D5, D6);
// =====新增 MQTT 回调函数=====
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();
  Serial.print("MQTT订阅收到控制指令：");
  Serial.println(msg);
  // 只处理 start / stop，转发串口给GD32主控
  if (msg == "start" || msg == "stop") {
    serial_mcu.println(msg);
    Serial.print("转发给GD32主控：");
    Serial.println(msg);
  }
}
// ==================== 嵌套判断并发送 ====================
void processNestedLogic() {
  if (lastGazeDirection.length() == 0 || lastMcuCommand.length() == 0) {
    return;
  }
  unsigned long now = millis();
  if (now - lastGazeTime > DATA_TIMEOUT || now - lastMcuTime > DATA_TIMEOUT) {
    return;
  }
  String result = "";
  String description = "";
  if (lastGazeDirection == "left") {
    if (lastMcuCommand == "1") {
      result = "happy";
      serialTianwen.print("g");
      Serial.println("已发送至天问");
      description = "人眼朝左 + 命令1 = happy";
    } else if (lastMcuCommand == "2") {
      result = "hungry";
      serialTianwen.print("i");
      Serial.println("已发送至天问");
      description = "人眼朝左 + 命令2 = hungry";
    } else if (lastMcuCommand == "3") {
      result = "TV";
      serialTianwen.print("k");
      Serial.println("已发送至天问");
      description = "人眼朝左 + 命令3 = TV";
    } else if (lastMcuCommand == "4") {
      result = "fear";
      serialTianwen.print("m");
      Serial.println("已发送至天问");
      description = "人眼朝左 + 命令4 = fear";
    } else if (lastMcuCommand == "5") {
      result = "thirsty";
      serialTianwen.print("o");
      Serial.println("已发送至天问");
      description = "人眼朝左 + 命令5 = thirsty";
    } else if (lastMcuCommand == "6") {
      result = "walk";
      serialTianwen.print("q");
      Serial.println("已发送至天问");
      description = "人眼朝左 + 命令6 = walk";
    }
  } else if (lastGazeDirection == "right") {
    if (lastMcuCommand == "1") {
      result = "said";
      serialTianwen.print("h");
      Serial.println("已发送至天问");
      description = "人眼朝右 + 命令1 = said";
    } else if (lastMcuCommand == "2") {
      result = "wc";
      serialTianwen.print("j");
      Serial.println("已发送至天问");
      description = "人眼朝右 + 命令2 = wc";
    } else if (lastMcuCommand == "3") {
      result = "up";
      serialTianwen.print("l");
      Serial.println("已发送至天问");
      description = "人眼朝右 + 命令3 = up";
    } else if (lastMcuCommand == "4") {
      result = "angry";
      serialTianwen.print("n");
      Serial.println("已发送至天问");
      description = "人眼朝右 + 命令4 = angry";
    } else if (lastMcuCommand == "5") {
      result = "uncomfortable";
      serialTianwen.print("p");
      Serial.println("已发送至天问");
      description = "人眼朝右 + 命令5 = uncomfortable";
    } else if (lastMcuCommand == "6") {
      result = "accompany";
      serialTianwen.print("r");
      Serial.println("已发送至天问");
      description = "人眼朝右 + 命令6 = accompany";
    }
  }
  if (result.length() > 0) {
    mqttClient.publish(TOPIC_GAZE_CMD, result.c_str());
    Serial.println("=========================================");
    Serial.println("【嵌套判断】");
    Serial.println("  注视方向: " + lastGazeDirection);
    Serial.println("  GD32主控命令: " + lastMcuCommand);
    Serial.println("  判断结果: " + description);
    Serial.println("  发送到主题: " + String(TOPIC_GAZE_CMD));
    Serial.println("  发送内容: " + result);
    Serial.println("=========================================");
    lastMcuCommand = "";
  }
}
// ==================== 处理K230数据 ====================
void handleK230Data(String data) {
  if (data == "left" || data == "right") {
    if (data != lastGazeDirection) {
      lastGazeDirection = data;
      lastGazeTime = millis();
      Serial.println("更新注视方向: " + data);
      processNestedLogic();
    }
  }
  else if (data == "center") {
    lastGazeDirection = "center";
    lastGazeTime = millis();
    Serial.println("注视方向: center（不触发判断）");
  }
  else if (data == "fall") {
    mqttClient.publish(TOPIC_FALL, "fall");
    Serial.println("  → 发送到 sensor/fall");
  }
  else if (data == "person") {
    mqttClient.publish(TOPIC_PERSON, "person");
    Serial.println("  → 发送到 sensor/person");
  }
  else if (data == "FACE_LOST") {
    Serial.println("  → 人脸丢失，重置注视方向");
    lastGazeDirection = "center";
    lastMcuCommand = "";
  }
}
// ==================== 处理 GD32 主控数据 ====================
void handleMcuData(String data) {
  //=====新增：g~r字符转发给天问模块=====
  if(data == "g" || data == "h" || data == "i" || data == "j" || data == "k" || data == "l" ||
     data == "m" || data == "n" || data == "o" || data == "p" || data == "q" || data == "r")
  {
    serialTianwen.print(data);
    Serial.print("【天问转发】收到GD32主控指令：");
    Serial.print(data);
    Serial.println(" → 发送至天问模块串口");
    return;
  }
  // =====呼叫信号=====
  if (data == "f")
  {
    mqttClient.publish(TOPIC_CALL, "Call");
    serialTianwen.print(data);
    Serial.println("=========================================");
    Serial.println("【呼叫信号】收到GD32主控呼叫信号");
    Serial.println("发送主题：" + String(TOPIC_CALL));
    Serial.println("发送内容：Call");
    Serial.println("=========================================");
    return;
  }
  if (data == "medicine")
  {
    mqttClient.publish(TOPIC_MEDICINE, "medicine");
    Serial.println("=========================================");
    Serial.println("【药品检测】收到medicine信号");
    Serial.println("发送主题：" + String(TOPIC_MEDICINE));
    Serial.println("发送内容：medicine");
    Serial.println("=========================================");
    return;
  }
  if (data == "DEVICE_FALL_DETECTED") {
    mqttClient.publish(TOPIC_MPU6050, "overturn");
    Serial.println("=========================================");
    Serial.println("【设备摔倒】");
    Serial.println("  收到GD32主控上报的设备摔倒信息");
    Serial.println("  发送内容: overturn");
    Serial.println("=========================================");
    return;
  }
  if (data.startsWith("{\"type\":\"gps\"")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, data);
    if (!error) {
      const char* type = doc["type"];
      const char* time = doc["time"];
      float lat = doc["lat"];
      float lon = doc["lon"];
      StaticJsonDocument<200> mqttDoc;
      mqttDoc["type"] = type;
      mqttDoc["time"] = time;
      mqttDoc["lat"] = lat;
      mqttDoc["lon"] = lon;
      char jsonBuffer[200];
      serializeJson(mqttDoc, jsonBuffer);
      mqttClient.publish(TOPIC_GPS, jsonBuffer);
      Serial.println("=========================================");
      Serial.println("【GPS数据】");
      Serial.println("  收到GPS JSON数据");
      Serial.print("  时间: ");
      Serial.println(time);
      Serial.print("  纬度: ");
      Serial.println(lat, 6);
      Serial.print("  经度: ");
      Serial.println(lon, 6);
      Serial.println("  发送到主题: " + String(TOPIC_GPS));
      Serial.print("  发送内容: ");
      Serial.println(jsonBuffer);
      Serial.println("=========================================");
    } else {
      Serial.println("【GPS数据】JSON解析失败");
    }
    return;
  }
  if (data == "1" || data == "2" || data == "3" ||
      data == "4" || data == "5" || data == "6") {
    lastMcuCommand = data;
    lastMcuTime = millis();
    Serial.println("更新GD32主控命令: " + data);
    processNestedLogic();
  }
}
// ==================== 初始化 ====================
void setup() {
  Serial.begin(115200);
  serialK230.begin(115200);
  serial_mcu.begin(115200);
  serialTianwen.begin(115200); //天问波特率，如不是115200自行修改
  connectWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback); //注册MQTT回调
  connectMQTT();
  //上电测试天问串口输出，用于硬件通路验证
  serialTianwen.println("esp ready");
  Serial.println("=========================================");
  Serial.println("ESP8266启动完成");
  Serial.println("映射规则：");
  Serial.println("  Left  1-6: happy, hungry, TV, fear, thirsty, walk");
  Serial.println("  Right 1-6: said, wc, up, angry, uncomfortable, accompany");
  Serial.println("  Center不发送");
  Serial.println("设备摔倒检测：已启用");
  Serial.println("  接收: DEVICE_FALL_DETECTED");
  Serial.println("  发送: " + String(TOPIC_MPU6050));
  Serial.println("GPS定位：已启用");
  Serial.println("  接收: JSON格式GPS数据");
  Serial.println("  发送: " + String(TOPIC_GPS));
  Serial.println("药品检测：已启用（GD32主控上报 medicine）");
  Serial.println("  MQTT主题: " + String(TOPIC_MEDICINE));
  Serial.println("云台搜索控制主题：" + String(TOPIC_SEARCH_CTRL));
  Serial.println("呼叫报警主题：" + String(TOPIC_CALL));
  Serial.println("天问模块串口：D5(RX),D6(TX)，波特率115200");
  Serial.println("等待 K230 与 GD32 主控数据...");
  Serial.println("=========================================");
}
// ==================== 连接WiFi ====================
void connectWiFi() {
  Serial.print("连接WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" OK");
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());
}
// ==================== 连接MQTT ====================
void connectMQTT() {
  while (!mqttClient.connected()) {
    String clientId = "esp8266-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("MQTT连接成功");
      // 订阅搜索控制主题
      mqttClient.subscribe(TOPIC_SEARCH_CTRL);
    } else {
      Serial.print("MQTT连接失败，重试...");
      delay(2000);
    }
  }
}
// ==================== 主循环 ====================
void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();
  if (serialK230.available()) {
    String data = serialK230.readStringUntil('\n');
    data.replace("\r", "");
    data.trim();
    if (data.length() > 0) {
      Serial.print("收到K230: [");
      Serial.print(data);
      Serial.println("]");
      handleK230Data(data);
    }
  }
  if (serial_mcu.available()) {
    String data = serial_mcu.readStringUntil('\n');
    data.replace("\r", ""); //清除\r回车，修复脏字符匹配失败
    data.trim();
    if (data.length() > 0) {
      Serial.print("收到GD32主控: [");
      Serial.print(data);
      Serial.println("]");
      handleMcuData(data);
    }
  }
  unsigned long now = millis();
  if (lastGazeDirection != "center" && now - lastGazeTime > DATA_TIMEOUT) {
    Serial.println("注视方向数据超时,重置为center");
    lastGazeDirection = "center";
  }
  delay(10);
}
