#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Encoder.h>
#include <Fonts/FreeSans9pt7b.h>
#include <SPI.h>
#include <Wire.h>

// Rotary Encoder
#define ENCODER_OPTIMIZE_INTERRUPTS
#define ENCODER_USE_INTERRUPTS
#define ENCODER_SUBSTEPS                                                       \
  4 // this is because the encoder takes 4 steps for every full click of the
    // wheel
#define ENCODER_PUSH_BUTTON_PIN 4
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3

// Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1

// Stepper Motor Driver
#define DIR_PIN 7
#define STEP_PIN 8
#define MS_1 11
#define MS_2 10
#define MS_3 9
#define STEP_ENABLE 12

// Stepper Motor Mechanics
#define STEP_TIME 1000  // us
#define VERTICAL_STEP 0.1  // mm
#define SCREW_LEAD 8       // mm per rotation
#define STEPS_PER_REVOLUTION 200
#define GEAR_RATIO 4
#define MOTOR_STEPS_PER_MM ( ( STEPS_PER_REVOLUTION / SCREW_LEAD ) * GEAR_RATIO )

// Swithces
#define GO_UP_PIN 5
#define GO_DOWN_PIN 6
#define TOP_PIN A1
#define BOTTOM_PIN A0
#define CONTACT_STOP A2

#define MIN_DISPLAY_TIME 250 // ms

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Encoder positionKnob(ENCODER_PIN_A, ENCODER_PIN_B);

long encoderPosition = 0;    // measured in clicks
long stepperCount = 0;       // signed steps the stepper motor has take
double verticalPosition = 0; // measured in mm

double lastDisplayUpdateTime = millis();

char displayString[40];

void updateDisplay() {
  Serial.println("-----");
  Serial.println(millis());
  double now = millis();
  if (now < lastDisplayUpdateTime + MIN_DISPLAY_TIME ) {
    return;
  }
  lastDisplayUpdateTime = now;
  char newDisplayString[40];
  dtostrf(verticalPosition, 6, 1, newDisplayString);

  if (strcmp(displayString, newDisplayString) != 0) {
    dtostrf(verticalPosition, 6, 1, displayString);
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(2.5);
    display.setCursor(0, 50);

    display.println(displayString);
    display.display();
  }
  Serial.println(millis());
}

bool encoderButtonPushed() {
  return digitalRead(ENCODER_PUSH_BUTTON_PIN) == LOW;
}

double verticalPositionFromStepper(int steps) {
  return double(steps) / MOTOR_STEPS_PER_MM;
}

bool stepDown() {
  if (digitalRead(BOTTOM_PIN) == HIGH && !encoderButtonPushed()) {
    digitalWrite(DIR_PIN, HIGH);
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(STEP_TIME);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(STEP_TIME);
    stepperCount -= 1;
    verticalPosition = verticalPositionFromStepper(stepperCount);
    // updateDisplay();
    return true;
  }
  return false;
}

bool stepUp() {
  if (digitalRead(TOP_PIN) == HIGH && !encoderButtonPushed()) {
    digitalWrite(DIR_PIN, LOW);
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(STEP_TIME);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(STEP_TIME);
    stepperCount += 1;
    verticalPosition = verticalPositionFromStepper(stepperCount);
    // updateDisplay();
    return true;
  }
  return false;
}


int encoderPositionFromVertical(double vertical) {
  return - ENCODER_SUBSTEPS * vertical / VERTICAL_STEP;
}

void turn(int steps) {
  if (steps > 0) {
    for (int i = 0; i < steps; i++) {
      if (!stepUp()) {
        updateDisplay();
        break;
      }
    }
  } else {
    for (int i = 0; i < -steps; i++) {
      if (!stepDown()) {
        updateDisplay();
        break;
      }
    }
  }
  updateDisplay();
}

void turnTo(int newStepperCount) {
  int steps = newStepperCount - stepperCount;
  turn(steps);
}

/*
 * converts a number of clicks on the rotary encoder wheel to
 * a vertical distance in mm.
 */
double verticalPositionFromEncoder(int clicks) {
  return -VERTICAL_STEP * clicks / ENCODER_SUBSTEPS;
}

long stepperCountFromEncoder(int clicks) {
  return MOTOR_STEPS_PER_MM * verticalPositionFromEncoder(clicks);
}


void zeroVerticalPosition() {
  verticalPosition = 0;
  stepperCount = 0;
  encoderPosition = 0;
  positionKnob.write(0);
  updateDisplay();
}

void goToZeroPosition() {
  turnTo(0);
}

void homeToBottom() {
  if (verticalPosition <= 0) {
    while (stepDown()) {}
        updateDisplay();
    while (encoderButtonPushed()){} // wait until encoder button is released to avoid zeroing
    delay(5);
  } else {
    goToZeroPosition();
  }
}

void homeToTop() {
  if (verticalPosition >= 0) {
    while(stepUp()){}
        updateDisplay();
    while (encoderButtonPushed()){} // wait until encoder button is released to avoid zeroing
    delay(5);
  } else {
    goToZeroPosition();
  }
}

bool upButtonPushed() { return (digitalRead(GO_UP_PIN) == LOW); }
bool downButtonPushed() { return (digitalRead(GO_DOWN_PIN) == LOW); }

// Wait a small amount of time. If the button is still pressed, fast travel. Otherwise home to top.
void goUp() {
  delay(300);
  if (upButtonPushed()) {
    while(upButtonPushed()) {
      stepUp();
    }
    updateDisplay();
  } else {
    homeToTop();
  }
}

// Wait a small amount of time. If the button is still pressed, fast travel. Otherwise home to bottom.
void goDown() {
  delay(300);
  if (downButtonPushed()) {
    while(downButtonPushed()) {
      stepDown();
    }
        updateDisplay();
  } else {
    homeToBottom();
  }
}

void loop() {
  long newEncoderPosition = positionKnob.read();

  if (newEncoderPosition != encoderPosition) {
    encoderPosition = newEncoderPosition;
    int desiredStepperCount = stepperCountFromEncoder(newEncoderPosition);
    turnTo(desiredStepperCount);
  } else if (encoderButtonPushed()) {
    zeroVerticalPosition();
  } else if (upButtonPushed()) {
    goUp();
  } else if (downButtonPushed()) {
    goDown();
  }
}

void setup() {

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(MS_1, OUTPUT);
  pinMode(MS_2, OUTPUT);
  pinMode(MS_3, OUTPUT);
  pinMode(STEP_ENABLE, OUTPUT);
  digitalWrite(MS_1, LOW);
  digitalWrite(MS_2, LOW);
  digitalWrite(MS_3, LOW);
  digitalWrite(STEP_ENABLE, LOW);

  pinMode(GO_DOWN_PIN, INPUT_PULLUP);
  pinMode(GO_UP_PIN, INPUT_PULLUP);
  pinMode(ENCODER_PIN_A, INPUT);
  pinMode(ENCODER_PIN_B, INPUT);

  pinMode(TOP_PIN, INPUT_PULLUP);
  pinMode(BOTTOM_PIN, INPUT_PULLUP);
  pinMode(CONTACT_STOP, INPUT_PULLUP);

  pinMode(ENCODER_PUSH_BUTTON_PIN, INPUT_PULLUP);

  positionKnob.write(0);
  Serial.begin(9600);
  display.setFont(&FreeSans9pt7b);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  updateDisplay();
}
