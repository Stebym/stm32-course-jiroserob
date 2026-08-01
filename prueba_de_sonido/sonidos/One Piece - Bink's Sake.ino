// Can be moved in header file i.e notes.h
#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))
#define Ab3 233
#define C4 262
#define D4 294
#define Ab4 466
#define C5 523
#define D5 587
#define F5 698
#define Ab5 932
#define Ab6 1865
#define E3 165
#define Db3 156
#define G4 392
#define F4 349
#define Db4 311
#define Gb3 208
#define G3 196
#define Gb4 415
#define Ab2 117
#define F3 175
#define G5 784
#define Db5 622
#define Gb5 831
#define C3 131
#define Db6 1245

const int midi1[434][3] = {
 {Ab3, 156, 10},
 {C4, 150, 10},
 {D4, 168, 0},
 {D4, 119, 8},
 {Ab3, 502, 2},
 {Ab4, 156, 10},
 {C5, 150, 10},
 {D5, 168, 0},
 {D5, 495, 0},
 {F5, 494, 2},
 {Ab5, 512, 0},
 {Ab6, 989, 2002},
 {Ab4, 162, 0},
 {Ab4, 167, 0},
 {C5, 372, 0},
 {Ab4, 121, 10},
 {Ab4, 495, 0},
 {Ab3, 1000, 2},
 {Ab4, 162, 0},
 {Ab4, 167, 0},
 {C5, 372, 0},
 {Ab4, 121, 10},
 {Ab3, 512, 0},
 {Ab3, 495, 0},
 {Ab3, 500, 1},
 {Ab3, 510, 3},
 {Ab4, 162, 0},
 {Ab4, 167, 0},
 {C5, 367, 0},
 {Ab4, 126, 2},
 {Ab4, 495, 0},
 {Ab3, 495, 0},
 {E3, 494, 2},
 {Ab4, 162, 0},
 {Ab4, 167, 0},
 {C5, 367, 0},
 {Ab4, 119, 10},
 {Db3, 512, 0},
 {Ab3, 495, 0},
 {Ab3, 1000, 2},
 {G4, 119, 8},
 {F4, 367, 0},
 {Db4, 120, 8},
 {D4, 119, 8},
 {Gb3, 502, 2},
 {G4, 119, 8},
 {Db4, 120, 8},
 {Db4, 118, 8},
 {G3, 494, 2},
 {Db4, 119, 8},
 {G3, 495, 0},
 {D4, 118, 8},
 {Gb4, 119, 10},
 {G4, 119, 8},
 {Db4, 120, 8},
 {Ab2, 495, 0},
 {F3, 494, 2},
 {G4, 119, 8},
 {F4, 367, 0},
 {Db4, 120, 8},
 {D4, 119, 8},
 {Gb3, 502, 2},
 {G4, 119, 8},
 {Db4, 120, 8},
 {Db4, 118, 8},
 {G3, 494, 2},
 {Db4, 119, 8},
 {G3, 495, 0},
 {D4, 118, 8},
 {Gb4, 119, 10},
 {Db4, 119, 8},
 {F4, 120, 8},
 {Db3, 498, 0},
 {Ab3, 502, 2},
 {G5, 119, 8},
 {F5, 367, 0},
 {Db5, 120, 8},
 {D5, 119, 8},
 {Gb3, 502, 2},
 {G5, 119, 8},
 {Db5, 120, 8},
 {Db5, 118, 8},
 {G3, 494, 2},
 {Db5, 119, 8},
 {G3, 495, 0},
 {D5, 118, 8},
 {Gb5, 119, 10},
 {G5, 119, 8},
 {Db5, 120, 8},
 {Ab2, 495, 0},
 {F3, 494, 2},
 {G5, 119, 8},
 {F5, 367, 0},
 {Db5, 120, 8},
 {D5, 119, 8},
 {Gb3, 502, 2},
 {G5, 119, 8},
 {Db5, 120, 8},
 {Db5, 118, 8},
 {G3, 494, 2},
 {Db5, 119, 8},
 {G3, 495, 0},
 {D5, 118, 8},
 {Gb5, 119, 10},
 {Db5, 119, 8},
 {F5, 120, 8},
 {Db3, 498, 0},
 {Ab3, 502, 2},
 {Ab4, 162, 0},
 {Ab4, 167, 0},
 {C5, 372, 0},
 {Ab4, 129, 2},
 {Ab4, 495, 0},
 {Ab3, 1000, 2},
 {Ab4, 162, 0},
 {Ab4, 167, 0},
 {C5, 372, 0},
 {Ab4, 121, 10},
 {Ab3, 512, 0},
 {Ab3, 495, 0},
 {Ab3, 500, 1},
 {Ab3, 510, 3},
 {Ab4, 162, 0},
 {Ab4, 167, 0},
 {C5, 367, 0},
 {Ab4, 126, 2},
 {Ab4, 495, 0},
 {C3, 495, 0},
 {E3, 494, 2},
 {Ab4, 162, 0},
 {Ab4, 167, 0},
 {C5, 367, 0},
 {Ab4, 119, 10},
 {Db3, 512, 0},
 {Ab3, 495, 0},
 {Db6, 495, 0},
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
