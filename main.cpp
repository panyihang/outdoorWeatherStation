#include "wifi.h"
#include <Wire.h>
#include <EEPROM.h>

int time_cnt = 0, cnt = 0, init_flag = 0;
int TEM, HUM, TEM_Past, HUM_Past;
union data
{
  int a;
  byte b[4];
};
data HUMUPPR;
data HUMLOWER;


void setup() {


  pinMode(2, INPUT_PULLUP);     // 重置Wi-Fi按键初始化

  Serial.begin(9600);     // 软件串口初始化

  wifi_protocol_init();

  for (int n = 0; n < 4; n++) 
    HUMUPPR.b[n] = EEPROM.read(n);
  for (int n = 0; n < 4; n++)
    HUMLOWER.b[n] = EEPROM.read(n + 4);

}

void loop() {
  if (init_flag == 0) {
    time_cnt++;
    if (time_cnt % 6000 == 0) {
      time_cnt = 0;
      cnt ++;
    }
    wifi_stat_led(&cnt);   // Wi-Fi状态处理
  }
  wifi_uart_service();
  myserialEvent();      // 串口接收处理
  key_scan();           // 重置配网按键检测
  if (init_flag == 1) //联网后开始自动控制
  {
  aotocontrol();   //自动控制
  }     
}


void aotocontrol() {
  HUM = (int)sht31.readHumidity();
  TEM = (int)(sht31.readTemperature() * 10);
  for (int n = 0; n < 4; n++) //湿度上限读取EEPROM
    HUMUPPR.b[n] = EEPROM.read(n);
  for (int n = 0; n < 4; n++)//湿度下限读取EEPROM,下限起始地址在4
    HUMLOWER.b[n] = EEPROM.read(n + 4);
  if (HUM > HUMUPPR.a)
  { if (digitalRead(relay) == 0)
    { digitalWrite(relay, HIGH);
      mcu_dp_bool_update(DPID_SWITCH, digitalRead(relay)); //BOOL型数据上报;
      mcu_dp_value_update(DPID_HUMIDITY_VALUE, HUM); //VALUE型数据上报;
      Serial.print(HUM);
      Serial.print(">");
      Serial.println(HUMUPPR.a);
      Serial.println("Relay on");
    }
  }
  else if (HUM < (HUMUPPR.a - 3))
  {
    if (digitalRead(relay) == 1)
    { digitalWrite(relay, LOW);
      mcu_dp_bool_update(DPID_SWITCH, digitalRead(relay)); //BOOL型数据上报;
      mcu_dp_value_update(DPID_HUMIDITY_VALUE, HUM); //VALUE型数据上报;
    }
  }
  if (abs(HUM - HUM_Past) >= 1) //湿度有变化时上报
  {
    mcu_dp_value_update(DPID_HUMIDITY_VALUE, HUM); //VALUE型数据上报;
  }
  if (abs(TEM - TEM_Past) > 1) ////温度有变化时上报
  {
    mcu_dp_value_update(DPID_TEMP_CURRENT, TEM); //VALUE型数据上报;
  }
  TEM_Past = TEM;
  HUM_Past = HUM;
}

void myserialEvent() {
  if (mySerial.available()) {
    unsigned char ch = (unsigned char)Serial.read();
    uart_receive_input(ch);
  }
}

void key_scan(void)
{
  int timecount;
  static char ap_ez_change = 0;
  unsigned char buttonState  = HIGH;
  buttonState = digitalRead(3);
  if (buttonState == LOW) {
    delay(100);
    buttonState = digitalRead(3);
    while (buttonState == LOW)
    { buttonState = digitalRead(3);
      timecount++;
      Serial.print("KEY down ");
      Serial.print(timecount * 100);
      Serial.println(" ms");
      delay(100);
    }
    if (timecount >= 30) {
      Serial.println("Wifi init successful!");
      init_flag = 0;
      switch (ap_ez_change) {
        case 0 :
          mcu_set_wifi_mode(SMART_CONFIG);
          break;
        case 1 :
          mcu_set_wifi_mode(AP_CONFIG);
          break;
        default:
          break;
      }
      ap_ez_change = !ap_ez_change;
    }
    else { 
      digitalWrite(relay, !digitalRead(relay));
      Serial.println("relay control successful!");
      mcu_dp_bool_update(DPID_SWITCH, digitalRead(relay));
    }
  }
}

void wifi_stat_led(int *cnt)
{
#define wifi_stat_led 2
  switch (mcu_get_wifi_work_state())
  {
    case SMART_CONFIG_STATE:  //0x00
      init_flag = 0;
      if (*cnt == 2) {
        *cnt = 0;
      }
      if (*cnt % 2 == 0)  //LED快闪
      {
        digitalWrite(wifi_stat_led, LOW);
      }
      else
      {
        digitalWrite(wifi_stat_led, HIGH);
      }
      break;
    case AP_STATE:  //0x01
      init_flag = 0;
      if (*cnt >= 30) {
        *cnt = 0;
      }
      if (*cnt  == 0)      // LED 慢闪
      {
        digitalWrite(wifi_stat_led, LOW);
      }
      else if (*cnt == 15)
      {
        digitalWrite(wifi_stat_led, HIGH);
      }
      break;

    case WIFI_NOT_CONNECTED:  // 0x02
      digitalWrite(wifi_stat_led, HIGH); // LED 熄灭
      break;
    case WIFI_CONNECTED:  // 0x03
      break;
    case WIFI_CONN_CLOUD:  // 0x04
      if ( 0 == init_flag )
      {
        digitalWrite(wifi_stat_led, LOW);// LED 常亮
        init_flag = 1;                  // Wi-Fi 连接上后该灯可控
        *cnt = 0;
      }

      break;

    default:
      digitalWrite(wifi_stat_led, HIGH);
      break;
  }
}
