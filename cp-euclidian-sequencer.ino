// Circuit Playground Euclidian MIDI Sequencer
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

// TODO:
// Change to USB-MIDI.h (for consistency)

#include <Adafruit_CircuitPlayground.h>
#include <MIDIUSB.h>

// CONSTANTS
const uint8_t   LED_COUNT         = 10;
const uint8_t   MAX_VELOCITY      = 5;  
const uint8_t   DEFAULT_VELOCITY  = 4; 
const uint8_t   NOTE_COUNT        = 4;    
const uint8_t   NOTES[4]          = {36, 38, 59, 47}; // selection of MIDI note values
const uint16_t  CAP_THRESHOLD     = 1000;
const uint16_t  CAP_DEBOUNCE      = 1000;
const uint8_t   OUT_CHANNEL       = 0;    // 0 - 15 set for your synth



// GLOBAL VARIAGLES (with defaults)

uint8_t   steps                   = 2;    // steps in sequence
uint8_t   triggers                = 0;    // number of played notes in sequence

uint8_t   note                    = 0;    // index of NOTES
uint8_t   pitch;                          // MIDI note table value
uint16_t  sequence;                       // sequence as a binary number
uint8_t   led_velocity            = DEFAULT_VELOCITY;     // 0 - 5 as displayed
uint8_t   velocity                = 96;   // seven bits representing MIDI velocity
uint8_t   pulse_count;                    // 24 pulses per quarter note
uint32_t  last_cap_touch;                 // millis() of last cap touch for debounce
bool      a_button_down           = false;
bool      b_button_down           = false;
bool      switch_is_right;
bool      red_led                 = false;


void noteOn(byte c, byte p, byte v) {
  // The Nord Drum 1 doesn't care about note off messages.
  midiEventPacket_t noteOn = {0x09, 0x90 | c, p, v};
  MidiUSB.sendMIDI(noteOn);
}

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

uint8_t getRed(int b) {
  if (b == 0) {
    return 16;  
  }
  else {
    return 0;
  }
}

uint8_t getGreen(int pix) {
  if (bitRead(sequence, pix)) {
    return 128; //velocity;
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
  CircuitPlayground.setPixelColor((9 - pix), getRed(pix), 
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

void checkCapTouch() {
  long m = millis();
  if (m - last_cap_touch > CAP_DEBOUNCE) {
    if (CircuitPlayground.readCap(6) > CAP_THRESHOLD) {
      sequence = rotateRight(sequence);
      last_cap_touch = m;
    }
  }
}

void advanceSequence() {
  checkCapTouch();
  CircuitPlayground.redLED(red_led);
  red_led = !red_led;
  sequence = rotateRight(sequence);
  if bitRead(sequence, 0) {
    noteOn(OUT_CHANNEL, pitch, velocity);
    MidiUSB.flush();
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
    addStep();
    sequence = generateEuclidian(triggers, steps);
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
    addTrigger();
    sequence = generateEuclidian(triggers, steps);
    updateSequenceDisplay();
  }
  else {
    increaseVelocity();
    velocity = calculateVelocity();
    updateLedBrightness();
    updateConfigDisplay();
  }
}

void checkMIDI() {
  midiEventPacket_t rx;
  do {
    rx = MidiUSB.read();
    //Count pulses and send note 
    if(rx.byte1 == 0xF8){
      pulse_count++;
      if(pulse_count >= 24){
        advanceSequence();
      };
    }
    //Clock start byte
    else if ((rx.byte1 == 0xFA) || (rx.byte1 == 0xFC)){
      red_led = false;
      pulse_count = 0;
    }
  } while (rx.header != 0);
}

bool switchChanged() {
  return (switch_is_right != switchRight()); 
}

void setup() {
  CircuitPlayground.begin();
  sequence = generateEuclidian(triggers, steps);
  velocity = calculateVelocity();
  switch_is_right = switchRight();
  updateLedBrightness();
  updateNeoPixels();
  pitch = NOTES[note];
  pulse_count = 0;
  last_cap_touch = millis();
}

void loop() {
  if (switchChanged()) {
    switch_is_right = switchRight();
    updateNeoPixels();
  }
  checkMIDI();
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
