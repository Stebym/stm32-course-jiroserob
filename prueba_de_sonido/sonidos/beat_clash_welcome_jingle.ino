// Can be moved in header file i.e notes.h
#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))
#define C4 262
#define E4 330
#define G4 392
#define C5 523
#define E5 659
#define G5 784
#define C6 1047

const int midi1[7][3] = {
 {C4, 120, 20},
 {E4, 120, 20},
 {G4, 120, 20},
 {C5, 180, 20},
 {E5, 180, 20},
 {G5, 250, 20},
 {C6, 400, 0},
};

void playMidi(int pin, const int notes[][3], size_t len){
 for (int i = 0; i < len; i++) {
    tone(pin, notes[i][0]);
    delay(notes[i][1]);
    noTone(pin);
    delay(notes[i][2]);
  }
}
// Generated using https://github.com/ShivamJoker/MIDI-to-Arduino

// main.ino or main.cpp
void setup() {
  // put your setup code here, to run once:
  // play midi by passing pin no., midi, midi len
  playMidi(11, midi1, ARRAY_LEN(midi1));
}

void loop() {
  // put your main code here, to run repeatedly:
}
