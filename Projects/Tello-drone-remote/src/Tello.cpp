#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WiFiUDP udp;
char packetBuffer[255];
unsigned int localPort = 9999;
IPAddress remoteIP(192, 168, 10, 1);
const char *ssid = "TELLO-FDB073";
const char *password = "shreyasd";


//global variables
int flying = 0;
int bat_time = 0;
int bat_check = 50;
int input = 0;
char bat_buf[50];
int warning = 0;
int warning_time = 0;
char rc_stream[50];

//pins
const uint8_t J1[2] = { 32, 33 }, J2[2] = { 34, 35 };  //Joysticks
const int flight_switch = 26;
const int speed_switch = 27;
int current_speed = 10;

void check_WiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    display.fillRect(0, 56, 6 * 12, 8, 0);
    display.display();
    display.setTextColor(WHITE);
    display.setCursor(0, 56);
    display.setTextSize(1);
    display.print("Connecting..");
    display.display();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      for (int i = 0; i <= 100; i += 2) {
        analogWrite(LED_BUILTIN, i);
        delay(5);
      }
      for (int i = 100; i >= 0; i -= 2) {
        analogWrite(LED_BUILTIN, i);
        delay(5);
      }
    }
    udp.begin(localPort);
    udp.beginPacket(remoteIP, 8889);
    udp.printf("command");
    udp.endPacket();
  }
  digitalWrite(LED_BUILTIN, 0);

  display.fillRect(0, 56, 6 * 12, 8, 0);
  display.display();
}

void battery_disp(const String &input) {
  int percent = input.toInt();
  int level = percent;
  display.fillRoundRect(0, 16, 15, 10, 2, 0);

  // Draw battery outline
  display.drawRoundRect(0, 16, 15, 10, 2, 1);
  display.fillRoundRect(15, 18, 2, 6, 1, 1);

  if (level > 0) {
    display.fillRoundRect(1, 16, 15 * level / 100, 10, 1, 1);
  }
  if ((level < 25)) {
    warning_time = millis();
    bat_check = 10;
    display.fillRoundRect(0, 16, 15, 10, 2, warning);

    // Draw battery outline
    display.drawRoundRect(0, 16, 15, 10, 2, 1);
    display.fillRoundRect(15, 18, 2, 6, 1, 1);


    display.setTextSize(1);
    display.setCursor(1, 17);
    display.setTextColor(!warning);
    display.print(level);

    display.fillRect(0, 0, 128, 16, warning);
    display.setTextColor(!warning);
    display.setTextSize(2);
    display.setCursor(5, 0);
    display.print("LowBattery");
    display.display();
    warning = !warning;
    delay(200);
  }
  if (level > 25) {
    warning_time = millis();
    bat_check = 5000;
    display.fillRect(0, 0, 128, 16, 0);
    display.setTextColor(1);
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("Ready");
  }

  display.display();
}

void speed_disp(int speed) {
  display.fillRect(110, 16, 18, 48, 0);
  display.drawRect(110, 16, 18, 48, 1);
  switch (speed) {
    case 10:
      display.fillRect(110, 16 + 32, 18, 48 - 32, 1);
      break;
    case 55:
      display.fillRect(110, 16 + 16, 18, 48 - 16, 1);
      break;
    case 100:
      display.fillRect(110, 16, 18, 48, 1);
      break;
  }
  display.setRotation(3);
  display.setTextColor(1);
  display.setCursor(10, 100);
  display.setTextSize(1);
  display.print("speed");
  display.display();
  display.setRotation(0);
}

void check_bat() {
  if (millis() >= bat_time + bat_check) {
    Serial.println("Battery");
    bat_time = millis();
    udp.beginPacket(remoteIP, 8889);
    udp.printf("battery?");
    udp.endPacket();

    while (udp.parsePacket()) delay(30);
    memset(bat_buf, 0, 50);

    if (udp.read(bat_buf, 50) > 0) {
      String commandResponse = String((char *)bat_buf);
      Serial.println(commandResponse);
      if (commandResponse[0] != 'o') battery_disp(commandResponse);
    }
  }
}

void toggle_flight(int input) {
  if (input == 1) return;

  udp.beginPacket(remoteIP, 8889);
  if (flying) {
    udp.printf("land");
    Serial.println("Land command issued");
  } else {
    udp.printf("takeoff");
    Serial.println("takeoff command issued");
  }
  udp.endPacket();
  flying = !flying;

  display.fillRect(0, 40, 6 * 8, 8 * 1, 0);
  display.setTextColor(WHITE);
  display.setCursor(0, 40);
  display.setTextSize(1);
  display.print((flying) ? "active" : "inactive");
  display.display();

  delay(600);
}

void toggle_speed() {
  int input = digitalRead(speed_switch);
  if (input == 1) return;

  switch (current_speed) {
    case 10:
      current_speed = 55;
      break;
    case 55:
      current_speed = 100;
      break;
    case 100:
      current_speed = 10;
      break;
  }
  speed_disp(current_speed);
  Serial.printf("speed: %d", current_speed);
  udp.beginPacket(remoteIP, 8889);
  udp.printf("speed %d", current_speed);
  udp.endPacket();
  delay(500);
}

void rc_command() {
  if (1 || flying) {
    sprintf(rc_stream, "rc %d %d %d %d",
              constrain((int)((double)analogRead(J2[0]) / 20.48 - 95), -100, 100),
              -constrain((int)((double)analogRead(J2[1]) / 20.48 - 85), -100, 100),
              constrain((int)((double)analogRead(J1[0]) / 20.48 - 95), -100, 100),
              -constrain((int)((double)analogRead(J1[1]) / 20.48 - 94), -100, 100)
            );

    Serial.println(rc_stream);

    udp.beginPacket(remoteIP, 8889);
    udp.printf(rc_stream);
    udp.endPacket();
  }
}

void testsetup() {
  Serial.begin(115200);
  pinMode(J1[0], INPUT);
  pinMode(J1[1], INPUT);
  pinMode(J2[0], INPUT);
  pinMode(J2[1], INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(flight_switch, INPUT_PULLUP);
  pinMode(speed_switch, INPUT_PULLUP);
}

void setup() {
  delay(4000);
  Serial.begin(115200);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  delay(1000);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(20, 0);
  display.setTextSize(2);
  display.print("Boot");

  //battery
  display.drawRoundRect(0, 16, 15, 10, 2, 1);
  display.fillRoundRect(15, 18, 2, 6, 1, 1);

  //speed
  display.fillRect(110, 16, 18, 48, 0);
  display.drawRect(110, 16, 18, 48, 1);

  display.display();

  pinMode(J1[0], INPUT);
  pinMode(J1[1], INPUT);
  pinMode(J2[0], INPUT);
  pinMode(J2[1], INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(flight_switch, INPUT_PULLUP);
  pinMode(speed_switch, INPUT_PULLUP);

  check_WiFi();

  display.fillRect(0, 0, 128, 64, 0);
  display.setTextColor(WHITE);
  display.setCursor(15, 0);
  display.setTextSize(2);
  display.print("Connected");
  display.setCursor(25, 20);
  display.display();


  WiFi.setAutoReconnect(1);
  udp.begin(localPort);

  udp.beginPacket(remoteIP, 8889);
  udp.printf("command");
  udp.endPacket();
  delay(3000);
  bat_time = millis();
}

void wifi_graphics() {
}

void loop() {
  toggle_flight(digitalRead(flight_switch));

  toggle_speed();

  check_bat();

  check_WiFi();

  rc_command();
}