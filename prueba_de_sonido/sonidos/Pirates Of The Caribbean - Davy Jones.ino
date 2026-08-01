// Can be moved in header file i.e notes.h
#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))
#define D3 147
#define E3 165
#define G4 392
#define F4 349
#define Ab4 466
#define E4 330
#define D4 294
#define A4 440
#define G3 196
#define C3 131
#define A2 110

const int midi1[58][3] = {
 {D3, 1492, 8},
 {E3, 742, 8},
 {D3, 1492, 8},
 {E3, 742, 8},
 {D3, 1492, 8},
 {G4, 742, 8},
 {F4, 1492, 758},
 {F4, 1492, 8},
 {G4, 742, 8},
 {Ab4, 367, 8},
 {G4, 367, 8},
 {F4, 1492, 8},
 {E4, 742, 8},
 {D4, 1680, 570},
 {G4, 1492, 8},
 {A4, 742, 8},
 {F4, 1492, 8},
 {D4, 742, 8},
 {E4, 1492, 8},
 {G3, 742, 8},
 {D3, 1492, 758},
 {C3, 742, 8},
 {A2, 1492, 758},
 {D4, 1492, 758},
 {F4, 1492, 0},
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
