#include <Wire.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

const uint8_t MOTOR_PINS[] = {2, 3, 4, 5};
const uint8_t POT_PIN = A0;
const uint8_t RESET_BUTTON_PIN = 6;

const uint8_t LCD_ADDRESS = 0x27;
const uint8_t LCD_BACKLIGHT = 0x08;
const uint8_t LCD_ENABLE = 0x04;
const uint8_t LCD_REGISTER_SELECT = 0x01;

const uint16_t STEPS_PER_REVOLUTION = 48;
const uint16_t POT_MAX_ADC = 675;
const uint16_t MIN_RPM = 1;
const uint16_t MAX_RPM = 400;
const uint8_t STOP_THRESHOLD = 5;
const uint16_t DISPLAY_INTERVAL_MS = 250;
const uint16_t BUTTON_DEBOUNCE_MS = 30;
const uint16_t POT_SAMPLE_INTERVAL_MS = 10;
const uint8_t POT_AVERAGE_SAMPLES = 8;
const uint8_t RPM_UPDATE_DEADBAND = 4;

const uint8_t STEP_PORTD_MASKS[4] = {
  0x14, 0x18, 0x28, 0x24
};

volatile uint8_t stepIndex = 0;
volatile uint16_t stepsInRevolution = 0;
volatile uint32_t completedRevolutions = 0;
uint32_t lastDisplayMillis = 0;
uint32_t lastButtonChangeMillis = 0;
uint32_t lastPotSampleMillis = 0;
uint16_t filteredPotValueQ3 = 0;
uint16_t setRpm = 0;
uint16_t activeRpm = 0;
uint8_t animationFrame = 0;
bool motorEnergized = false;
bool buttonReading = HIGH;
bool buttonState = HIGH;
char displayCache[2][16];

ISR(TIMER1_COMPA_vect) {
  stepIndex = (stepIndex + 1) & 0x03;
  PORTD = (PORTD & 0xC3) | STEP_PORTD_MASKS[stepIndex];

  stepsInRevolution++;
  if (stepsInRevolution >= STEPS_PER_REVOLUTION) {
    stepsInRevolution = 0;
    completedRevolutions++;
  }
}

void configureStepTimer(uint32_t intervalMicros) {
  uint16_t prescaler;
  uint8_t clockSelect;

  if (intervalMicros <= 32767UL) {
    prescaler = 8;
    clockSelect = _BV(CS11);
  } else if (intervalMicros <= 262140UL) {
    prescaler = 64;
    clockSelect = _BV(CS11) | _BV(CS10);
  } else if (intervalMicros <= 1048560UL) {
    prescaler = 256;
    clockSelect = _BV(CS12);
  } else {
    prescaler = 1024;
    clockSelect = _BV(CS12) | _BV(CS10);
  }

  uint32_t timerTicks = intervalMicros * (F_CPU / 1000000UL) / prescaler;
  if (timerTicks < 1) {
    timerTicks = 1;
  }

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    OCR1A = static_cast<uint16_t>(timerTicks - 1);
    TIFR1 = _BV(OCF1A);
    TIMSK1 |= _BV(OCIE1A);
    TCCR1B = _BV(WGM12) | clockSelect;
  }
}

void stopStepTimer() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    TIMSK1 &= ~_BV(OCIE1A);
    TCCR1B = 0;
    PORTD &= 0xC3;
  }
}

void writeExpander(uint8_t value) {
  Wire.beginTransmission(LCD_ADDRESS);
  Wire.write(value | LCD_BACKLIGHT);
  Wire.endTransmission();
}

void pulseEnable(uint8_t value) {
  writeExpander(value | LCD_ENABLE);
  delayMicroseconds(1);
  writeExpander(value & ~LCD_ENABLE);
  delayMicroseconds(50);
}

void writeLcdNibble(uint8_t nibble, bool dataMode) {
  uint8_t value = nibble & 0xF0;
  if (dataMode) {
    value |= LCD_REGISTER_SELECT;
  }
  pulseEnable(value);
}

void sendLcd(uint8_t value, bool dataMode) {
  writeLcdNibble(value, dataMode);
  writeLcdNibble(value << 4, dataMode);
}

void lcdCommand(uint8_t command) {
  sendLcd(command, false);
  if (command == 0x01 || command == 0x02) {
    delay(2);
  }
}

void lcdWriteChar(char character) {
  sendLcd(static_cast<uint8_t>(character), true);
}

void initLcd() {
  Wire.begin();
  delay(50);
  writeExpander(0);
  delay(5);
  writeLcdNibble(0x30, false);
  delay(5);
  writeLcdNibble(0x30, false);
  delayMicroseconds(150);
  writeLcdNibble(0x30, false);
  writeLcdNibble(0x20, false);
  lcdCommand(0x28);
  lcdCommand(0x08);
  lcdCommand(0x01);
  lcdCommand(0x06);
  lcdCommand(0x0C);
}

void printLcdLine(uint8_t row, uint8_t address, const char *text) {
  for (uint8_t i = 0; i < 16; i++) {
    char character = text[i];
    if (character == '\0') {
      character = ' ';
    }
    if (displayCache[row][i] != character) {
      lcdCommand(address + i);
      lcdWriteChar(character);
      displayCache[row][i] = character;
    }
  }
}

void updateDisplay() {
  char line[17];
  uint32_t revolutionsSnapshot;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    revolutionsSnapshot = completedRevolutions;
  }
  snprintf(line, sizeof(line), "Zwoje:     %05lu",
           static_cast<unsigned long>(revolutionsSnapshot % 100000UL));
  printLcdLine(0, 0x80, line);

  char bar[5] = {' ', ' ', ' ', ' ', '\0'};
  if (setRpm > 0) {
    bar[animationFrame] = '_';
  }
  snprintf(line, sizeof(line), "RPM/min %s %3u", bar, setRpm);
  printLcdLine(1, 0xC0, line);
  if (setRpm > 0) {
    animationFrame = (animationFrame + 1) % 4;
  }
}

void setup() {
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(MOTOR_PINS[i], OUTPUT);
    digitalWrite(MOTOR_PINS[i], LOW);
  }

  initLcd();
  filteredPotValueQ3 = analogRead(POT_PIN) << 3;
  lastPotSampleMillis = millis();
  updateDisplay();
}

void loop() {
  bool newButtonReading = digitalRead(RESET_BUTTON_PIN);
  uint32_t nowMillis = millis();

  if (newButtonReading != buttonReading) {
    buttonReading = newButtonReading;
    lastButtonChangeMillis = nowMillis;
  }

  if (static_cast<uint32_t>(nowMillis - lastButtonChangeMillis) >= BUTTON_DEBOUNCE_MS &&
      buttonState != buttonReading) {
    buttonState = buttonReading;
    if (buttonState == LOW) {
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        completedRevolutions = 0;
        stepsInRevolution = 0;
      }
    }
  }

  if (static_cast<uint32_t>(nowMillis - lastPotSampleMillis) >= POT_SAMPLE_INTERVAL_MS) {
    uint32_t sampleTotal = 0;
    for (uint8_t i = 0; i < POT_AVERAGE_SAMPLES; i++) {
      sampleTotal += analogRead(POT_PIN);
    }
    int16_t averagedSample = sampleTotal / POT_AVERAGE_SAMPLES;
    int16_t filteredValue = filteredPotValueQ3;
    filteredValue += ((averagedSample << 3) - filteredValue) / 8;
    filteredPotValueQ3 = filteredValue;
    lastPotSampleMillis = nowMillis;
  }

  uint16_t potValue = (filteredPotValueQ3 + 4) >> 3;
  if (potValue > POT_MAX_ADC) {
    potValue = POT_MAX_ADC;
  }

  if (potValue < STOP_THRESHOLD) {
    setRpm = 0;
    if (motorEnergized) {
      stopStepTimer();
      motorEnergized = false;
      activeRpm = 0;
    }
  } else {
    setRpm = map(potValue, 0, POT_MAX_ADC, MIN_RPM, MAX_RPM);
    uint16_t rpmDifference =
        setRpm > activeRpm ? setRpm - activeRpm : activeRpm - setRpm;
    uint32_t stepIntervalMicros =
        60000000UL / STEPS_PER_REVOLUTION / setRpm;

    if (!motorEnergized || rpmDifference >= RPM_UPDATE_DEADBAND) {
      motorEnergized = true;
      activeRpm = setRpm;
      configureStepTimer(stepIntervalMicros);
    }
  }

  nowMillis = millis();
  if (static_cast<uint32_t>(nowMillis - lastDisplayMillis) >= DISPLAY_INTERVAL_MS) {
    updateDisplay();
    lastDisplayMillis = nowMillis;
  }
}
