# cp-euclidian-sequencer
This is a USB-MIDI powered Euclidian sequencer that runs on an Adafruit Circuit Playground microcontroller.

Copyright 2024, Tom Hoffman.
GPL 3.0 License.

## NOTES:
This sequencer is dependent on an incoming MIDI clock.
Outgoing channel range 0-15 (may display as 1-16 on synth).
Sequence is represented as a binary number (not an array).
Advancing the sequence is a rightward bitshift.
Least significant bit (rightmost) corresponds to cp.pixels[9].
Nord Drum 1 notes (MIDI percussion values): 
36 (bass drum)
38 (snare) 
59 (ride cymbal) 
47 (low-mid tom)

Color codes:
Configuration view (switch left)
Note index (+1): blue
Velocity: green
Sequence view (switch right)
All steps: blue.
Active step: green.

Seems to work without lag up to about 1600 bmp (as indicated by 
the reported clock rate), based on obvious lag when clock is switched off. 
This is not serious benchmarking at this point.
