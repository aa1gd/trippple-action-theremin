What is this?
- A digital theremin that can play notes and chords following partwriting rules. It runs on a Raspberry Pi Pico, and uses tinyusb to function as a MIDI device. It was created for a final project in an AP Music Theory class.

Why is "triple" spelled incorrectly?
- It's not. Trippple has three p's. -Jacob

How to operate it?
Melody Mode:
- Right hand controls pitch. Left hand controls volume. Closer means quieter, farther away means louder.

Chords Mode:
- Right hand controls chord degree. Left hand controls chord inversion. When the desired chord has been selected (the LEDs will show you current selection), tap the foot pedal. The same exact chord cannot be played twice in a row.
- Screen will display the played chord's notes.

Figured Bass Mode:
- Like chords, but right hand controls the bass note. Left hand controls inversion.

Duet Mode:
- Both hands play notes at max volume; left hand is one octave lower than the right.

Knobs:
- Turn left knob to change key, press it to switch from major to minor
- Turn middle knob to change the operating mode. Press it to clear all currently playing notes (used if something went wrong)
- Turn right knob to change the MIDI channel. Can be used to switch synthesizer instruments.

What parts do I need?
- 1x Raspberry Pi Pico
- 2x HC-SR04 ultrasonic sensors
- 1x SSD1306 128x64 OLED display
- 2x SN74HC138N 3-8 decoder/demux ICs (used to control the LEDs)
- 3x Rotary encoders
- 15x LEDs + current limiting resistors
- 3D printer (for enclosure)
- A lot of patience

How do I compile it?
- It uses the Pico SDK. See https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf

Known bugs:
- First chord played after startup can't be a root position i/I chord
- If the seventh of a seventh chord is suspended (remains on the same note), it will forget to go down.
- (Not a bug, but beware: all global variables must be static or else tinyusb will hate you. You may notice I went a bit overboard with adding "static" keywords to everything)
- Entire project was done in 6 days. The code is sloppy and inevitably has many more bugs I didn't find yet.