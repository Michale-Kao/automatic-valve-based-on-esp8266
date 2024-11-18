#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <AliyunIoTSDK.h>


#define DHTPIN D5//温湿度
#define DHTTYPE DHT11
#define RELAY_PIN D6//继电器
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_SDA D2 
#define OLED_SCL D3
#define MQ2_AO A0
#define SERVO_PIN D4


// 阿里云 IoT 产品和设备信息 
#define PRODUCT_KEY "。。。"
#define DEVICE_NAME "。。。"
#define DEVICE_SECRET "。。。"
#define REGION_ID "cn-shanghai"//不用改

// WiFi信息，校园网连接比较麻烦，手机开热点即可
char WIFI_SSID[] = "wifi名称"; 
char WIFI_PASSWD[] = "WiFi密码";   

// 创建传感器和设备对象
DHT dht(DHTPIN, DHTTYPE);
Servo myservo;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
static WiFiClient espClient;
bool manualControl = false; // false表示未手动控制，true表示手动控制

unsigned long previousMillis = 0;
const long interval = 2000;
unsigned int sensorValue = 0;

void setup() {
  Serial.begin(115200);
  dht.begin();
  myservo.attach(SERVO_PIN);
  pinMode(RELAY_PIN, OUTPUT);

  // 初始化 OLED 显示屏
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();

  // 初始化 WiFi
  wifiInit(WIFI_SSID, WIFI_PASSWD);

  // 初始化阿里云 IoT SDK
  AliyunIoTSDK::begin(espClient, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET, REGION_ID);
  AliyunIoTSDK::bindData("valve", powerCallback);
  AliyunIoTSDK::bindData("pump", pumpCallback);

}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    float humi = dht.readHumidity();
    float temp = dht.readTemperature();
    int sensorValue = analogRead(MQ2_AO); 

    if (!isnan(humi) && !isnan(temp)) {
      Serial.print("Humidity: "); Serial.print(humi); Serial.print(" %\t");
      Serial.print("Temperature: "); Serial.print(temp); Serial.println(" *C");
      Serial.print("Gas level: "); Serial.println(sensorValue);

      // 显示数据到 OLED 屏幕
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.print("Humidity: "); display.print(humi); display.print(" %");
      display.setCursor(0, 20);
      display.print("Temperature: "); display.print(temp); display.print(" C");
      display.setCursor(0, 40);
      display.print("Gas Level: "); display.print(sensorValue);
      display.display();

      // 发送数据到阿里云
      AliyunIoTSDK::send("temp", float(temp));
      AliyunIoTSDK::send("humi", float(humi));
      AliyunIoTSDK::send("gas", int(sensorValue));
     
     // 如果没有手动控制，阈值控制舵机 
      if (!manualControl) { 
        if (humi >= 65) { 
          myservo.write(90); //mgs90可以控制角度
          digitalWrite(RELAY_PIN, LOW);
          Serial.println("Humidity >= 65, closing valve halfway and starting pump."); } 
          else { 
            myservo.write(0); 
            digitalWrite(RELAY_PIN, HIGH);
            Serial.println("Humidity < 65, stopping injection."); }
        }
      
    } 
  }
  //烟雾检测
  if (sensorValue > 300) {
    Serial.println("Gas detected, closing valve!");
    myservo.write(180);
    digitalWrite(RELAY_PIN, HIGH);
  }

  // 阿里云 IoT SDK 主循环
  AliyunIoTSDK::loop();
}

void wifiInit(const char* ssid, const char* passphrase) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, passphrase);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(1000);
    Serial.println("WiFi not Connect");
  }
  Serial.println("Connected to AP");
}

void powerCallback(JsonVariant p) { 
  int PowerSwitch = p["valve"];
  if (PowerSwitch == 1) { manualControl = true; // 设置为手动控制模式 
  myservo.write(180); // 将舵机
  Serial.println("Servo manually turned ON via Aliyun."); 
  } 
  else { manualControl = false; // 退出手动控制模式 
  myservo.write(0); // 舵机回到0度 
  Serial.println("Servo manually turned OFF via Aliyun.");
   }
}

void pumpCallback(JsonVariant p) {
  int pump = p["pump"];
  if (pump == 1) {manualControl = true;
    digitalWrite(RELAY_PIN, LOW); // 启动水泵
  } 
  else {
    digitalWrite(RELAY_PIN, HIGH); // 关闭水泵
  }
}
