#include <Wire.h>
#include <Adafruit_BMP280.h>

// Pin Definitions
#define TRIG_PIN    12  // HC-SR04 Trigger pin
#define ECHO_PIN    13  // HC-SR04 Echo pin (via 1k/2k voltage divider)
#define MOSFET_PIN  14  // IRLZ44N N-Channel MOSFET Gate pin
#define I2C_SDA     21  // BMP280 I2C Data (SDA)
#define I2C_SCL     19  // BMP280 I2C Clock (SCL)

// Distance and PWM Dimming Thresholds
const float MIN_DISTANCE_CM = 5.0;   // Full brightness (100% duty cycle) threshold
const float MAX_DISTANCE_CM = 40.0;  // Off (0% duty cycle) threshold

Adafruit_BMP280 bmp;
bool isBmpReady = false;
float smoothedDistance = MAX_DISTANCE_CM;

unsigned long lastReportTime = 0;
const unsigned long REPORT_INTERVAL_MS = 500; // Telemetry interval (ms)

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW); // Keep GPIO12 (strapping pin) LOW at startup
  pinMode(ECHO_PIN, INPUT);

  pinMode(MOSFET_PIN, OUTPUT);
  analogWrite(MOSFET_PIN, 0); // Ensure MOSFET is OFF at startup

  Wire.begin(I2C_SDA, I2C_SCL);

  if (bmp.begin(0x76) || bmp.begin(0x77)) {
    isBmpReady = true;
    Serial.println("System Ready: BMP280, HC-SR04, and MOSFET active.");
  } else {
    Serial.println("WARNING: BMP280 not detected. Continuing distance-based LED control...");
  }
}

void loop() {
  // 1. Trigger HC-SR04 ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // 2. Measure echo pulse duration (30 ms timeout)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration > 0) {
    float rawDistance = (duration * 0.0343) / 2.0;
    // Exponential moving average filter to prevent LED flickering
    smoothedDistance = (smoothedDistance * 0.7) + (rawDistance * 0.3);
  } else {
    // Smoothly fade out LED when object moves out of range
    smoothedDistance = (smoothedDistance * 0.85) + (MAX_DISTANCE_CM * 0.15);
  }

  // 3. Map distance to 8-bit PWM duty cycle (0 - 255)
  int pwmValue = 0;
  if (smoothedDistance < MAX_DISTANCE_CM) {
    float constrainedDist = constrain(smoothedDistance, MIN_DISTANCE_CM, MAX_DISTANCE_CM);
    pwmValue = (int)((MAX_DISTANCE_CM - constrainedDist) / (MAX_DISTANCE_CM - MIN_DISTANCE_CM) * 255.0);
  }

  analogWrite(MOSFET_PIN, pwmValue);

  // 4. Read BMP280 and print telemetry every 500 ms (non-blocking)
  unsigned long currentTime = millis();
  if (currentTime - lastReportTime >= REPORT_INTERVAL_MS) {
    lastReportTime = currentTime;

    // Attempt reconnection if BMP280 was disconnected at startup
    if (!isBmpReady && (bmp.begin(0x76) || bmp.begin(0x77))) {
      isBmpReady = true;
    }

    int brightnessPercent = map(pwmValue, 0, 255, 0, 100);

    Serial.print("Distance: ");
    Serial.print(smoothedDistance, 1);
    Serial.print(" cm | LED Brightness: ");
    Serial.print(brightnessPercent);
    Serial.print("%");

    if (isBmpReady) {
      float temperature = bmp.readTemperature();
      float pressureHpa = bmp.readPressure() / 100.0F;
      Serial.print(" | Temp: ");
      Serial.print(temperature, 1);
      Serial.print(" C | Pressure: ");
      Serial.print(pressureHpa, 2);
      Serial.println(" hPa");
    } else {
      Serial.println(" | BMP280: Waiting for sensor...");
    }
  }

  delay(30); // Short delay for smooth PWM transitions
}