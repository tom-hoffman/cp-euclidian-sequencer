// Circuit Playground Euclidian MIDI Sequencer (non-rotating)
// By Tom Hoffman
// GPL 3.0 License.

// NOTES:
// This sequencer is dependent on an incoming MIDI clock.
// Outgoing channel range 0-15 (may display as 1-16 on synth).
// Sequence is represented as a binary number (not an array).
// Advancing the sequence is a rightward bitshift.
// Least significant bit (rightmost) corresponds to cp.pixels[9].
// Nord Drum 1 notes (MIDI percussion values): 
// 36 (bass drum)
// 38 (snare) 
// 59 (ride cymbal) 
// 47 (low-mid tom)

// Color codes:
// Configuration view (switch left)
// Note index (+1): blue
// Velocity: green
// Sequence view (switch right)
// All steps: blue.
// Active step: green.

// Seems to work without lag up to about 1600 bmp (as indicated by 
// the reported clock rate), based on obvious lag when clock is switched off.
// This is not serious benchmarking at this point.

// TODO:

#include <Adafruit_CircuitPlayground.h>
#include <USB-MIDI.h>

USBMIDI_CREATE_DEFAULT_INSTANCE();
using namespace MIDI_NAMESPACE;

// CONSTANTS
const uint8_t   LED_COUNT         = 10;
const uint8_t   MAX_VELOCITY      = 5;  
const uint8_t   DEFAULT_VELOCITY  = 4; 
const uint8_t   NOTE_COUNT        = 4;    
const uint8_t   NOTES[4]          = {36, 38, 59, 47}; // selection of MIDI note values
const uint8_t   OUT_CHANNEL       = 1;    // 1 - 16 set for your synth



// GLOBAL VARIAGLES (with defaults)

uint8_t   steps                   = 2;    // steps in sequence
uint8_t   triggers                = 0;    // number of played notes in sequence
uint16_t  active_step             = 0b1;  // active step as bitmask
uint16_t  first_step              = 0b1;  // the first beat in the sequence as bitmask
uint8_t   note                    = 0;    // index of NOTES
uint8_t   pitch;                          // MIDI note table value
uint16_t  sequence;                       // sequence as a binary number
uint8_t   led_velocity            = DEFAULT_VELOCITY;     // 0 - 5 as displayed
uint8_t   velocity                = 96;   // seven bits representing MIDI velocity
uint8_t   pulse_count;                    // 24 pulses per quarter note
bool      a_button_down           = false;
bool      b_button_down           = false;
bool      switch_is_right;
bool      red_led                 = false;
bool      started                 = false;

uint16_t rotateRight(uint16_t n) {
  // Rotating bit shift to right.
  uint16_t shifted = n >> 1;
  if (bitRead(n, 0)) {
    return bitSet(shifted, steps - 1);
  }
  else {
    return bitClear(shifted, steps - 1);
  }
}

uint16_t rotateLeft(uint16_t n) {
  // Rotating bit shift to the left
  uint16_t shifted = n << 1;
  if (bitRead(n, steps - 1)) {
    shifted = bitSet(shifted, 0);
    return bitClear(shifted, steps);
  }
  else {
    return shifted;
  }
}

uint16_t pushBit(uint16_t acc, uint8_t b) {
  // acc is the 16 bit accumulator.
  // b is expected to be a bool cast to an integer.
  // b becomes the lsb in the sequence with bit shift.
  return (acc << 1) | b;
}

uint16_t generateEuclidian(uint16_t triggers, float steps) {
  // Based on Jeff Holtzkener's Javascript implementation at
  // https://medium.com/code-music-noise/euclidean-rhythms-391d879494df
  // Inputs: a number of triggers or played notes
  // from the number of total steps.
  // Outputs a number representing a rhythm pattern
  // when represented as a binary number.
  uint16_t previous;
  uint16_t pattern = 0;
  for (int i = 0; i < (steps + 1); i++) {
    uint16_t xVal = (triggers / steps) * i;
    pattern = pushBit(pattern, !(previous == xVal));
    previous = xVal;
  }
  return pattern;
}

uint8_t getRed(int pix) {
  if (bitRead(active_step, pix)) {
    return 64;  
  }
  else {
    return 0;
  }
}

uint8_t getGreen(int pix) {
  if (bitRead(sequence, pix)) {
    return 48; //velocity;
  }
  else {
    return 0;
  }
}

uint8_t getBlue(int pix) {
  // All steps
  if (pix < steps) {
    return 16;
  }
  else {
    return 0;
  }
}

void updateSequencePixel(int pix) {
  CircuitPlayground.setPixelColor((pix), getRed(pix), 
                                       getGreen(pix), 
                                       getBlue(pix));
}

void updateSequenceDisplay() {
  for (int i = 0; i < LED_COUNT; i++) {
    updateSequencePixel(i);
  }
}

void displayNote() {
  for (int i = 0; i < 5; i++) {
    if (i >= (4 - note)) {
      CircuitPlayground.setPixelColor(i, 0, 0, 16);
    }
    else {
      CircuitPlayground.setPixelColor(i, 0, 0, 0);
    }
  }
}

void displayLedVelocity() {
  for (int i = 5; i < 10; i++) {
    if (i <= 4 + led_velocity) {
      CircuitPlayground.setPixelColor(i, 0, 16, 0);
    }
    else {
      CircuitPlayground.setPixelColor(i, 0, 0, 0);
    }
  }
}

void updateConfigDisplay() {
  displayNote();
  displayLedVelocity();
}

void updateNeoPixels() {
  if (switch_is_right) {
    updateSequenceDisplay();
  }
  else {
    updateConfigDisplay();
  }
}

void updateLedBrightness() {
  CircuitPlayground.setBrightness((led_velocity * 20) + 20);
}

uint8_t calculateVelocity() {
  float m = float(led_velocity) / float(MAX_VELOCITY);
  return (m * 127);
}

bool switchRight() { 
  // displaying sequence
  return !(CircuitPlayground.slideSwitch());
}

bool switchLeft() {
  //displaying settings
  return CircuitPlayground.slideSwitch();
}

void addStep() {
  if (steps < 10) {
    steps++;
  }
  else {            // wrap
    steps = 1;
    active_step = 0b1;
    if (triggers) {   // if there are any triggers
      triggers = 1;   // reduce to one
    }
  }
}

void addTrigger() {
  if (triggers < steps) {
    triggers++;
  }
  else {
    triggers = 0;
  }
}

void advanceSequence() {
  CircuitPlayground.redLED(red_led);
  red_led = !red_led;
  active_step = rotateRight(active_step);
  if (active_step & sequence) {
    MIDI.sendNoteOn(pitch, velocity, OUT_CHANNEL);
  }
  if (switch_is_right) {
    updateSequenceDisplay();
  }
  pulse_count = 0;
}

void advanceNote() {
  note = ((++note) % NOTE_COUNT);
}

void processLeftButton() {
  if (switchRight()) {
    if (started) {
      addStep();
      sequence = generateEuclidian(triggers, steps);
    } 
    else { // if stopped, decrement first_step
      first_step = rotateLeft(active_step);
      active_step = first_step;
    }
  updateSequenceDisplay();
  }
  else {
    advanceNote();
    pitch = NOTES[note];
    updateConfigDisplay();
  }
}

void increaseVelocity() {
  led_velocity = ((++led_velocity) % (MAX_VELOCITY + 1));
}

void processRightButton() {
  if (switchRight()) {
    if (started) {
      addTrigger();
      sequence = generateEuclidian(triggers, steps);
    }
    else { // if stopped, advance first_step
      first_step = rotateRight(active_step);
      active_step = first_step;
    }
  updateSequenceDisplay();
  }
  else {
    increaseVelocity();
    velocity = calculateVelocity();
    updateLedBrightness();
    updateConfigDisplay();
  }
}

bool switchChanged() {
  return (switch_is_right != switchRight()); 
}

void onClock() {
  if (started) {
    pulse_count++;
    if (pulse_count >= 24) {
      advanceSequence();
    }
  }
}

void onStart() {
  onContinue();
  active_step = first_step;
}

void onContinue() {
  started = true;
  red_led = false;
  pulse_count = 0;
}

void onStop() {
  started = false;
  active_step = first_step;
  updateSequenceDisplay();
}

void setup() {
  CircuitPlayground.begin();
  MIDI.begin(OUT_CHANNEL);
  MIDI.setHandleClock(onClock);
  MIDI.setHandleStart(onStart);
  MIDI.setHandleContinue(onContinue);
  MIDI.setHandleStop(onStop);
  sequence = generateEuclidian(triggers, steps);
  velocity = calculateVelocity();
  switch_is_right = switchRight();
  updateLedBrightness();
  updateNeoPixels();
  pitch = NOTES[note];
  pulse_count = 0;
}

void loop() {
  MIDI.read();
  if (switchChanged()) {
    switch_is_right = switchRight();
    updateNeoPixels();
  }
  if (!a_button_down) {                   // if a not previously pressed
    if (CircuitPlayground.leftButton()) { // and the button is being pressed
      processLeftButton();              // process new press
      a_button_down = true;               // note ongoing press
    }
  }
  else {                                             // if it was previously pressed
    a_button_down = CircuitPlayground.leftButton();  // store the current state
  }
  if (!b_button_down) {
    if (CircuitPlayground.rightButton()) {
      processRightButton();
      b_button_down = true;
    }
  }
  else {                                             // if it was previously pressed
    b_button_down = CircuitPlayground.rightButton();  // store the current state
  }
}
