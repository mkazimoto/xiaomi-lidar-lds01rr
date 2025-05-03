// library: https://github.com/kaiaai/LDS

#ifndef ESP32
  #error This example runs on ESP32
#endif

// 1. CHANGE these to match your wiring
// IGNORE pins absent from your Lidar model (often EN, PWM)
const uint8_t LIDAR_GPIO_EN = 19; // ESP32 GPIO connected to Lidar EN pin
const uint8_t LIDAR_GPIO_RX = 16; // ESP32 GPIO connected to Lidar RX pin
const uint8_t LIDAR_GPIO_TX = 17; // ESP32 GPIO connected to Lidar TX pin
const uint8_t LIDAR_GPIO_PWM = 15;// ESP32 GPIO connected to Lidar PWM pin

// 2. UNCOMMENT if using PWM pin and PWM LOW enables the motor
//#define INVERT_PWM_PIN

#define XIAOMI_LDS02RR

// 4. UNCOMMENT debug option(s)
// and increase SERIAL_MONITOR_BAUD to MAX possible
//#define DEBUG_GPIO
//#define DEBUG_PACKETS
//#define DEBUG_SERIAL_IN
//#define DEBUG_SERIAL_OUT

const uint32_t SERIAL_MONITOR_BAUD = 115200;
const uint32_t LIDAR_PWM_FREQ = 10000;
const uint8_t LIDAR_PWM_BITS = 11;
const uint8_t LIDAR_PWM_CHANNEL = 2;
const uint16_t LIDAR_SERIAL_RX_BUF_LEN = 1048;
const uint16_t PRINT_EVERY_NTH_POINT = 10;
const uint16_t HEX_DUMP_WIDTH = 16;

#include "lds_all_models.h"

LDS *lidar;
uint16_t hex_dump_pos = 0;

void setupLidar() {

  lidar = new LDS_LDS02RR();

  lidar->setScanPointCallback(lidar_scan_point_callback);
  lidar->setPacketCallback(lidar_packet_callback);
  lidar->setSerialWriteCallback(lidar_serial_write_callback);
  lidar->setSerialReadCallback(lidar_serial_read_callback);
  lidar->setMotorPinCallback(lidar_motor_pin_callback);
  lidar->setInfoCallback(lidar_info_callback);
  lidar->setErrorCallback(lidar_error_callback);

  Serial2.begin(lidar->getSerialBaudRate(), SERIAL_8N1, LIDAR_GPIO_TX, LIDAR_GPIO_RX);
  
  lidar->init();
  //lidar->stop();
}

void setup() {
  Serial.begin(SERIAL_MONITOR_BAUD);

  Serial.println();
  Serial.print("ESP IDF version ");
  Serial.println(ESP_IDF_VERSION_MAJOR);

  setupLidar();

  Serial.print("LiDAR model ");
  Serial.print(lidar->getModelName());
  Serial.print(", baud rate ");
  Serial.print(lidar->getSerialBaudRate());
  Serial.print(", TX GPIO ");
  Serial.print(LIDAR_GPIO_TX);
  Serial.print(", RX GPIO ");
  Serial.println(LIDAR_GPIO_RX);

  LDS::result_t result = lidar->start();
  Serial.print("startLidar() result: ");
  Serial.println(lidar->resultCodeToString(result));

  // Set desired rotations-per-second for some LiDAR models
  lidar->setScanTargetFreqHz(4.00f);
}

void printByteAsHex(uint8_t b) {
  if (b < 16)
    Serial.print('0');
  Serial.print(b, HEX);
  Serial.print(' ');
}

void printBytesAsHex(const uint8_t * buffer, uint16_t length) {
  if (length == 0)
    return;

  for (uint16_t i = 0; i < length; i++) {
    printByteAsHex(buffer[i]);
  }
}

int lidar_serial_read_callback() {
  #ifdef DEBUG_SERIAL_IN
  int ch = Serial2.read();
  if (ch != -1) {
    if (hex_dump_pos++ % HEX_DUMP_WIDTH == 0)
      Serial.println();
    printByteAsHex(ch);
  }
  return ch;
  #else
  return Serial2.read();
  #endif
}

size_t lidar_serial_write_callback(const uint8_t * buffer, size_t length) {
  #ifdef DEBUG_SERIAL_OUT
  Serial.println();
  Serial.print('>');
  printBytesAsHex(buffer, length);
  Serial.println();
  #endif
  
  return Serial2.write(buffer, length);
}

void lidar_scan_point_callback(float angle_deg, float distance_mm, float quality,
  bool scan_completed) {

  if (((int)angle_deg) % 1 == 0) {
    Serial.print((int)angle_deg);
    Serial.print(':');
    Serial.println((int)distance_mm);
  }
  
}

void lidar_motor_pin_callback(float value, LDS::lds_pin_t lidar_pin) {

  int pin = (lidar_pin == LDS::LDS_MOTOR_EN_PIN) ? LIDAR_GPIO_EN : LIDAR_GPIO_PWM;

  #ifdef DEBUG_GPIO
  Serial.print("GPIO ");
  Serial.print(pin);
  Serial.print(' ');
  Serial.print(lidar->pinIDToString(lidar_pin));
  Serial.print(" mode set to ");
  Serial.println(lidar->pinStateToString((LDS::lds_pin_state_t) int(value)));
  #endif

  if (value <= (float)LDS::DIR_INPUT) {

    // Configure pin direction
    if (value == (float)LDS::DIR_OUTPUT_PWM) {

      #if ESP_IDF_VERSION_MAJOR < 5
       ledcSetup(LIDAR_PWM_CHANNEL, LIDAR_PWM_FREQ, LIDAR_PWM_BITS);
       ledcAttachPin(pin, LIDAR_PWM_CHANNEL);
      #else
      if (!ledcAttachChannel(pin, LIDAR_PWM_FREQ, LIDAR_PWM_BITS, LIDAR_PWM_CHANNEL))
        Serial.println("lidar_motor_pin_callback() ledcAttachChannel() error");
      #endif
    } else
      pinMode(pin, (value == (float)LDS::DIR_INPUT) ? INPUT : OUTPUT);

    return;
  }

  if (value < (float)LDS::VALUE_PWM) {
    // Set constant output
    // TODO invert PWM as needed
    digitalWrite(pin, (value == (float)LDS::VALUE_HIGH) ? HIGH : LOW);

  } else {
    #ifdef DEBUG_GPIO
    Serial.print("PWM value set to ");
    Serial.print(value);
    #endif

    // set PWM duty cycle
    #ifdef INVERT_PWM_PIN
    value = 1 - value;
    #endif

    int pwm_value = ((1<<LIDAR_PWM_BITS)-1)*value;

    #if ESP_IDF_VERSION_MAJOR < 5
      ledcWrite(LIDAR_PWM_CHANNEL, pwm_value);
    #else
      ledcWriteChannel(LIDAR_PWM_CHANNEL, pwm_value);
    #endif

    #ifdef DEBUG_GPIO
    Serial.print(' ');
    Serial.println(pwm_value);
    #endif
  }
}

void lidar_info_callback(LDS::info_t code, String info) {
  Serial.print("LiDAR info ");
  Serial.print(lidar->infoCodeToString(code));
  Serial.print(": ");
  Serial.println(info);
}

void lidar_error_callback(LDS::result_t code, String aux_info) {
  // Serial.print("LiDAR error ");
  // Serial.print(lidar->resultCodeToString(code));
  // Serial.print(": ");
  // Serial.println(aux_info);
}

void lidar_packet_callback(uint8_t * packet, uint16_t length, bool scan_completed) {
  #ifdef DEBUG_PACKETS
  Serial.println();
  Serial.print("Packet callback, length=");
  Serial.print(length);
  Serial.print(", scan_completed=");
  Serial.println(scan_completed);
  #endif
  
  return;
}

void loop() {
  lidar->loop();
}
