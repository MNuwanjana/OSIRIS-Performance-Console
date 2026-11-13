#include <FastLED.h>
#include <arduinoFFT.h>

// --- LED CONFIGURATION ---
#define LED_PIN       5       // ESP32 pin connected to Data In of WS2812B
#define NUM_LEDS      570     // Total number of LEDs
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB
CRGB leds[NUM_LEDS];

// --- MICROPHONE CONFIGURATION (MAX9814) ---
#define MIC_PIN       34      // Analog pin for MAX9814 Audio Out (ADC1 on ESP32)

// --- BUTTON PINS ---
#define BTN_MODE        12
#define BTN_BRIGHTNESS  13
#define BTN_INTENSITY   14

// --- FFT CONFIGURATION ---
#define SAMPLES       256             // Must be a power of 2
#define SAMPLING_FREQ 10000           // Hz, must be at least 2x highest expected frequency
unsigned int sampling_period_us;
unsigned long microseconds;
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT FFT = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// --- SYSTEM VARIABLES ---
int currentMode = 0;                  // 8 Modes (0 to 7)
int brightnessLevels[] = {50, 100, 150, 200, 255};
int currentBrightnessIdx = 2;         // Default medium brightness (150)
float intensitySpread = 1.0;          // Multiplier for effect spread
int intensityState = 0;

// Button debounce variables
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 200;

void setup() {
  Serial.begin(115200);
  
  // Initialize FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightnessLevels[currentBrightnessIdx]);
  
  // Initialize Buttons
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_BRIGHTNESS, INPUT_PULLUP);
  pinMode(BTN_INTENSITY, INPUT_PULLUP);
  
  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));
  
  // Startup Animation (Wipe clear)
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void loop() {
  handleButtons();
  
  // --- AUDIO SAMPLING (MAX9814) ---
  for (int i = 0; i < SAMPLES; i++) {
    microseconds = micros();
    vReal[i] = analogRead(MIC_PIN); // Read analog value from AGC mic
    vImag[i] = 0;
    while (micros() - microseconds < sampling_period_us) {
      // empty loop to maintain sampling frequency
    }
  }
  
  // --- FFT PROCESSING ---
  FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(FFT_FORWARD);
  FFT.ComplexToMagnitude();
  
  // Calculate Bass, Mid, Treble averages from FFT bins
  int bass = 0;
  for (int i = 2; i < 10; i++) bass += vReal[i];
  bass = bass / 8; // Average bass magnitude
  
  // Apply Intensity Spread multiplier
  int reactiveValue = constrain((bass * intensitySpread) / 10, 0, NUM_LEDS / 2);

  // --- RENDER MODES ---
  switch(currentMode) {
    case 0: modeCenterOut(reactiveValue); break;         // Mode 1: Classic VU
    case 1: modeHeartbeat(reactiveValue); break;         // Mode 2: Signature Heartbeat (Your Hero effect!)
    case 2: modeRainbowPulse(reactiveValue); break;      // Mode 3: Rainbow Bass
    case 3: modeShootingStars(reactiveValue); break;     // Mode 4: Shooting Stars
    case 4: modeBassFlash(reactiveValue); break;         // Mode 5: Full Bass Flash
    case 5: modeColorWipe(reactiveValue); break;         // Mode 6: Audio Wipe
    case 6: modeMirrorPlasma(reactiveValue); break;      // Mode 7: Plasma Mirror
    case 7: modeFireReactive(reactiveValue); break;      // Mode 8: Audio Fire
  }
  
  FastLED.show();
}

// ==========================================
// BUTTON HANDLING (Mode, Brightness, Intensity)
// ==========================================
void handleButtons() {
  if (millis() - lastDebounceTime > debounceDelay) {
    // Mode Change
    if (digitalRead(BTN_MODE) == LOW) {
      currentMode++;
      if (currentMode > 7) currentMode = 0;
      fill_solid(leds, NUM_LEDS, CRGB::Black); // Clear strip on change
      lastDebounceTime = millis();
    }
    // Brightness Change
    if (digitalRead(BTN_BRIGHTNESS) == LOW) {
      currentBrightnessIdx++;
      if (currentBrightnessIdx > 4) currentBrightnessIdx = 0;
      FastLED.setBrightness(brightnessLevels[currentBrightnessIdx]);
      lastDebounceTime = millis();
    }
    // Intensity/Spread Change
    if (digitalRead(BTN_INTENSITY) == LOW) {
      intensityState++;
      if (intensityState > 2) intensityState = 0;
      
      if (intensityState == 0) intensitySpread = 0.8;       // Low spread
      else if (intensityState == 1) intensitySpread = 1.2;  // Normal spread
      else if (intensityState == 2) intensitySpread = 1.8;  // High/Wide spread
      lastDebounceTime = millis();
    }
  }
}

// ==========================================
// LED EFFECTS (8 MODES)
// ==========================================

// Mode 0: Center Out VU Meter
void modeCenterOut(int audioPeak) {
  fadeToBlackBy(leds, NUM_LEDS, 40); // Smooth fade trail
  int center = NUM_LEDS / 2;
  for (int i = 0; i < audioPeak; i++) {
    CRGB color = CHSV(i * (255 / (NUM_LEDS/2)), 255, 255);
    if(center + i < NUM_LEDS) leds[center + i] = color;
    if(center - i >= 0) leds[center - i] = color;
  }
}

// Mode 1: The Signature "Heartbeat" Effect ❤️
void modeHeartbeat(int audioPeak) {
  fadeToBlackBy(leds, NUM_LEDS, 60); // Fast fade for heartbeat pulse
  int center = NUM_LEDS / 2;
  CRGB heartColor = CRGB::Red; 
  if (audioPeak > (NUM_LEDS/2)*0.8) heartColor = CRGB::HotPink; // Changes color on heavy bass
  
  for (int i = 0; i < audioPeak; i++) {
    if(center + i < NUM_LEDS) leds[center + i] = heartColor;
    if(center - i >= 0) leds[center - i] = heartColor;
  }
}

// Mode 2: Rainbow Pulse
void modeRainbowPulse(int audioPeak) {
  static uint8_t hue = 0;
  hue++;
  if (audioPeak > 10) {
    fill_solid(leds, NUM_LEDS, CHSV(hue, 255, audioPeak * 2));
  } else {
    fadeToBlackBy(leds, NUM_LEDS, 20);
  }
}

// Mode 3: Shooting Stars
void modeShootingStars(int audioPeak) {
  fadeToBlackBy(leds, NUM_LEDS, 20);
  if (audioPeak > 30) {
    int pos = random16(NUM_LEDS);
    leds[pos] += CRGB::White;
  }
}

// Mode 4: Bass Flash
void modeBassFlash(int audioPeak) {
  if (audioPeak > (NUM_LEDS/4)) {
    fill_solid(leds, NUM_LEDS, CRGB::White);
  } else {
    fadeToBlackBy(leds, NUM_LEDS, 50);
  }
}

// Mode 5: Audio Wipe
void modeColorWipe(int audioPeak) {
  static uint8_t hue = 0;
  fadeToBlackBy(leds, NUM_LEDS, 10);
  for (int i = 0; i < audioPeak * 2 && i < NUM_LEDS; i++) {
    leds[i] = CHSV(hue + (i*2), 255, 255);
  }
  hue += 2;
}

// Mode 6: Mirror Plasma
void modeMirrorPlasma(int audioPeak) {
  static uint16_t x = 0;
  x += audioPeak * 10; // Speed depends on audio
  for (int i = 0; i < NUM_LEDS / 2; i++) {
    uint8_t noise = inoise8(i * 20, x);
    CRGB color = CHSV(noise, 255, 255);
    leds[(NUM_LEDS/2) + i] = color;
    leds[(NUM_LEDS/2) - i - 1] = color;
  }
}

// Mode 7: Audio Fire
void modeFireReactive(int audioPeak) {
  fadeToBlackBy(leds, NUM_LEDS, 40);
  int fireSpread = map(audioPeak, 0, NUM_LEDS/2, 0, NUM_LEDS);
  for (int i = 0; i < fireSpread; i++) {
    leds[i] = HeatColor(qsub8(255, (255 * i) / fireSpread));
  }
}