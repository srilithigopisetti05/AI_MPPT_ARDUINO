#include <TimerOne.h>

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define PIN_PV_VOLT    A0
#define PIN_PV_CURR    A1
#define PIN_BAT_VOLT   A2
#define PIN_PWM_OUT    9   // Timer1 PWM pin (50kHz capable)
#define PIN_LED_AI     13  // Onboard LED indicates AI active mode

// ==========================================
// CALIBRATION & CONSTANTS
// ==========================================
const float V_REF          = 5.0;
const float ADC_RES        = 1023.0;
const float VOLTAGE_DIVIDER = 5.0;  // Adjust based on physical resistor network ratio
const float CURRENT_SENS    = 0.185; // Sensitivity for ACS712-05B (185mV/A)

const uint16_t PWM_PERIOD_US = 20;   // 20us = 50 kHz switching frequency
const int PWM_MIN            = 10;   // 10% min duty cycle limit
const int PWM_MAX            = 90;   // 90% max duty cycle limit
const int PWM_STEP           = 1;    // Perturb step size

const uint32_t AI_TIMEOUT_MS = 30000; // 30s watchdog threshold

// ==========================================
// STATE VARIABLES
// ==========================================
float pv_voltage  = 0.0;
float pv_current  = 0.0;
float pv_power    = 0.0;
float prev_power  = 0.0;
float prev_volts  = 0.0;
float bat_voltage = 0.0;

int pwm_duty = 50;  // Start at 50% duty cycle

float ai_target_vmpp = 0.0;
uint32_t last_ai_message = 0;
bool ai_mode_active = false;

void readSensors();
void processSerialCommands();
void runMPPTControl();
void sendTelemetry();

void setup() {
  Serial.begin(9600); // Hardware UART link to ESP32
  pinMode(PIN_LED_AI, OUTPUT);
  digitalWrite(PIN_LED_AI, LOW);

  // Configure 50kHz PWM on Pin 9 using TimerOne
  Timer1.initialize(PWM_PERIOD_US);
  Timer1.pwm(PIN_PWM_OUT, map(pwm_duty, 0, 100, 0, 1023));
}

void loop() {
  readSensors();
  processSerialCommands();
  
  // Check AI Watchdog Status
  if (millis() - last_ai_message > AI_TIMEOUT_MS) {
    ai_mode_active = false;
    digitalWrite(PIN_LED_AI, LOW); // LED OFF = Standalone Fallback
  } else {
    ai_mode_active = true;
    digitalWrite(PIN_LED_AI, HIGH); // LED ON = AI Guided
  }

  runMPPTControl();
  sendTelemetry();
  delay(100); // 10Hz control loop
}

void readSensors() {
  // Read Analog Sensors
  float raw_pv_v = analogRead(PIN_PV_VOLT);
  float raw_pv_i = analogRead(PIN_PV_CURR);
  float raw_bat  = analogRead(PIN_BAT_VOLT);

  // Convert ADC values to real voltages/currents
  pv_voltage  = (raw_pv_v * V_REF / ADC_RES) * VOLTAGE_DIVIDER;
  bat_voltage = (raw_bat * V_REF / ADC_RES) * VOLTAGE_DIVIDER;
  
  // ACS712 Current sensor formula centered around VREF/2 (2.5V)
  float raw_curr_volts = (raw_pv_i * V_REF / ADC_RES);
  pv_current  = abs(raw_curr_volts - 2.5) / CURRENT_SENS;
  
  pv_power = pv_voltage * pv_current;
}

void processSerialCommands() {
  while (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.startsWith("VMPP:")) {
      float val = command.substring(5).toFloat();
      if (val > 5.0 && val < 30.0) { // Basic safety bounds check
        ai_target_vmpp = val;
        last_ai_message = millis();
      }
    }
  }
}

void runMPPTControl() {
  if (ai_mode_active && ai_target_vmpp > 0.0) {
    // ----------------------------------------------------
    // AI-Guided Adaptive Tracking: Adjust PWM toward Target VMPP
    // ----------------------------------------------------
    if (pv_voltage < ai_target_vmpp - 0.2) {
      pwm_duty -= PWM_STEP; // Decrease duty cycle to raise panel voltage
    } else if (pv_voltage > ai_target_vmpp + 0.2) {
      pwm_duty += PWM_STEP; // Increase duty cycle to draw down panel voltage
    }
  } else {
    // ----------------------------------------------------
    // Fallback Mode: Classic Perturb & Observe (P&O)
    // ----------------------------------------------------
    float delta_p = pv_power - prev_power;
    float delta_v = pv_voltage - prev_volts;

    if (delta_p != 0.0) {
      if (delta_p > 0.0) {
        if (delta_v > 0.0) pwm_duty -= PWM_STEP;
        else pwm_duty += PWM_STEP;
      } else {
        if (delta_v > 0.0) pwm_duty += PWM_STEP;
        else pwm_duty -= PWM_STEP;
      }
    }
  }

  // Constrain duty cycle to safe operational limits
  pwm_duty = constrain(pwm_duty, PWM_MIN, PWM_MAX);

  // Update PWM Hardware
  Timer1.setPwmDuty(PIN_PWM_OUT, map(pwm_duty, 0, 100, 0, 1023));

  // Store values for next iteration
  prev_power = pv_power;
  prev_volts = pv_voltage;
}

void sendTelemetry() {
  static uint32_t last_tx = 0;
  if (millis() - last_tx >= 1000) { // Broadcast 1Hz telemetry
    last_tx = millis();
    Serial.print("DATA,PVV,");
    Serial.print(pv_voltage, 2);
    Serial.print(",PVI,");
    Serial.print(pv_current, 2);
    Serial.print(",PVP,");
    Serial.print(pv_power, 2);
    Serial.print(",BAT,");
    Serial.print(bat_voltage, 2);
    Serial.print(",PWM,");
    Serial.print(pwm_duty);
    Serial.print(",AI,");
    Serial.println(ai_mode_active ? 1 : 0);
  }
}