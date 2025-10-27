#include <HID-Project.h>
#include <HID-Settings.h>

// Matrix and encoder pin definitions
const byte ROWS = 4;
const byte COLS = 4;
const byte rowPins[ROWS] = { 9, 8, 7, 6 };
const byte colPins[COLS] = { 2, 3, 4, 5 };

const byte encoderPinA = 10;
const byte encoderPinB = 16;
const byte encoderButtonPin = 14;
const byte ledPin = 15;

// LED effect modes: "press", "constant", "blink"
String ledMode = "press";

// Key type enumeration
enum KeyType {
  KEYBOARD_KEY,
  CONSUMER_KEY
};

// Key structure
struct KeyDefinition {
  KeyType type;
  uint16_t code;
};

// Keymap definition
const KeyDefinition keymap[ROWS][COLS] = {
  { { CONSUMER_KEY, MEDIA_VOLUME_UP },      { KEYBOARD_KEY, '3' }, { KEYBOARD_KEY, '2' }, { KEYBOARD_KEY, '1' } },
  { { CONSUMER_KEY, MEDIA_VOLUME_DOWN },    { KEYBOARD_KEY, '6' }, { KEYBOARD_KEY, '5' }, { KEYBOARD_KEY, '4' } },
  { { CONSUMER_KEY, HID_CONSUMER_MUTE },    { KEYBOARD_KEY, '9' }, { KEYBOARD_KEY, '8' }, { KEYBOARD_KEY, '7' } },
  { { CONSUMER_KEY, MEDIA_PLAY_PAUSE },     { CONSUMER_KEY, CONSUMER_BRIGHTNESS_UP }, { KEYBOARD_KEY, '0' }, { CONSUMER_KEY, CONSUMER_BRIGHTNESS_DOWN } }
};

// Key state and timing
bool keyState[ROWS][COLS] = { false };
unsigned long lastDebounceTime[ROWS][COLS] = { 0 };
unsigned long lastRepeatTime[ROWS][COLS] = { 0 };
const unsigned long debounceDelay = 30;
const unsigned long repeatDelay = 150;

// Encoder state
int lastEncoderAState = HIGH;
bool lastButtonState = HIGH;
bool encoderButtonPressed = false;
unsigned long lastClickTime = 0;
int clickCount = 0;
const unsigned long clickTimeout = 400;

// LED blinking state
unsigned long lastBlinkTime = 0;
bool ledBlinkState = false;

void setup() {
  Serial.begin(9600);

  setupMatrixPins();
  setupEncoderPins();

  pinMode(ledPin, OUTPUT);

  BootKeyboard.begin();
  Consumer.begin();

  Serial.println("Macropad + Encoder ready");
}

void setupMatrixPins() {
  for (byte r = 0; r < ROWS; r++)
    pinMode(rowPins[r], INPUT_PULLUP);

  for (byte c = 0; c < COLS; c++) {
    pinMode(colPins[c], OUTPUT);
    digitalWrite(colPins[c], HIGH);
  }
}

void setupEncoderPins() {
  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);
  pinMode(encoderButtonPin, INPUT_PULLUP);
}

void loop() {
  scanMatrix();
  handleEncoderRotation();
  handleEncoderButton();
  updateLED();
  delay(5);
}

void scanMatrix() {
  unsigned long now = millis();

  for (byte c = 0; c < COLS; c++) {
    digitalWrite(colPins[c], LOW);
    delayMicroseconds(100);

    for (byte r = 0; r < ROWS; r++) {
      bool pressed = (digitalRead(rowPins[r]) == LOW);

      if (pressed != keyState[r][c] && (now - lastDebounceTime[r][c] > debounceDelay)) {
        lastDebounceTime[r][c] = now;
        keyState[r][c] = pressed;
        handleKeyEvent(r, c, pressed);
      }

      if (keyState[r][c]) {
        repeatConsumerKey(r, c, now);
      }
    }

    digitalWrite(colPins[c], HIGH);
  }
}

void handleKeyEvent(byte r, byte c, bool pressed) {
  KeyDefinition key = keymap[r][c];

  if (pressed) {
    if (key.type == KEYBOARD_KEY) {
      BootKeyboard.press(key.code);
    } else {
      Consumer.write(key.code);
      lastRepeatTime[r][c] = millis();
    }
  } else {
    if (key.type == KEYBOARD_KEY) {
      BootKeyboard.release(key.code);
    }
  }
}

void repeatConsumerKey(byte r, byte c, unsigned long now) {
  KeyDefinition key = keymap[r][c];

  if (key.type == CONSUMER_KEY && (now - lastRepeatTime[r][c] > repeatDelay)) {
    Consumer.write(key.code);
    lastRepeatTime[r][c] = now;
  }
}

void handleEncoderRotation() {
  int encoderAState = digitalRead(encoderPinA);
  int encoderBState = digitalRead(encoderPinB);

  if (encoderAState != lastEncoderAState) {
    if (encoderAState == LOW) {
      if (encoderBState == HIGH) {
        Consumer.write(MEDIA_VOLUME_UP);
        Serial.println("Encoder → Volume Up");
      } else {
        Consumer.write(MEDIA_VOLUME_DOWN);
        Serial.println("Encoder → Volume Down");
      }
    }
  }

  lastEncoderAState = encoderAState;
}

void handleEncoderButton() {
  bool buttonPressed = (digitalRead(encoderButtonPin) == LOW);
  unsigned long now = millis();

  if (buttonPressed != lastButtonState) {
    lastButtonState = buttonPressed;

    if (buttonPressed) {
      encoderButtonPressed = true;

      if (now - lastClickTime > clickTimeout) {
        clickCount = 0;
      }

      clickCount++;
      lastClickTime = now;
    } else {
      encoderButtonPressed = false;
    }
  }

  if (clickCount > 0 && (now - lastClickTime > clickTimeout)) {
    handleClickAction(clickCount);
    clickCount = 0;
  }
}

void handleClickAction(int clicks) {
  switch (clicks) {
    case 1:
      Consumer.write(MEDIA_PLAY_PAUSE);
      Serial.println("Single Press → Play/Pause");
      break;
    case 2:
      Consumer.write(MEDIA_NEXT);
      Serial.println("Double Press → Next Track");
      break;
    case 3:
      Consumer.write(MEDIA_PREVIOUS);
      Serial.println("Triple Press → Previous Track");
      break;
  }
}

void updateLED() {
  if (ledMode == "press") {
    digitalWrite(ledPin, isAnyButtonPressed() ? HIGH : LOW);
  } else if (ledMode == "constant") {
    digitalWrite(ledPin, HIGH);
  } else if (ledMode == "blink") {
    unsigned long now = millis();
    if (now - lastBlinkTime >= 500) {
      ledBlinkState = !ledBlinkState;
      digitalWrite(ledPin, ledBlinkState ? HIGH : LOW);
      lastBlinkTime = now;
    }
  }
}

bool isAnyButtonPressed() {
  for (byte r = 0; r < ROWS; r++) {
    for (byte c = 0; c < COLS; c++) {
      if (keyState[r][c]) return true;
    }
  }

  return encoderButtonPressed;
}