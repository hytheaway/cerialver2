#include <MozziGuts.h>
#include <Oscil.h>
#include <mozzi_rand.h>
#include <mozzi_midi.h>
#include <EventDelay.h>
#include <Mux.h>
#include <SPI.h>
#include <tables/sin8192_int8.h>
#include <tables/whitenoise8192_int8.h>
#include <tables/pinknoise8192_int8.h>
#include <EventDelay.h>
#include <Ead.h>

#define CONTROL_RATE 128  // Hz, powers of 2 are most reliable

using namespace admux;

// -----THESE LINES SHOULD BE PLACED AT THE TOP OF YOUR CODE NEAR THE OTHER MUX DEFINITION-------
Mux aux(Pin(32, INPUT, PinType::Digital), Pinset(16, 17, 12));  // this creates a new multiplexer object, tells your program which pins it is connected to, and specifies that it is being read as an analog signal
int auxVals[] = { 0, 0, 0, 0, 0, 0, 0, 0 };                     // an array to store the value of each multiplexer input

int auxReadVal = 0;

// variables for potentiometers
int potVals[] = { 0, 0 };

Oscil<SIN8192_NUM_CELLS, AUDIO_RATE> osc1(SIN8192_DATA);
Oscil<SIN8192_NUM_CELLS, AUDIO_RATE> osc2(SIN8192_DATA);
Oscil<SIN8192_NUM_CELLS, AUDIO_RATE> osc3(SIN8192_DATA);
Oscil<SIN8192_NUM_CELLS, AUDIO_RATE> osc4(SIN8192_DATA);
Oscil<SIN8192_NUM_CELLS, AUDIO_RATE> osc5(SIN8192_DATA);

Oscil<PINKNOISE8192_NUM_CELLS, AUDIO_RATE> kick(PINKNOISE8192_DATA);
Oscil<PINKNOISE8192_NUM_CELLS, AUDIO_RATE> snare(PINKNOISE8192_DATA);
Oscil<PINKNOISE8192_NUM_CELLS, AUDIO_RATE> hihat(PINKNOISE8192_DATA);

Ead env1(CONTROL_RATE);  // resolution will be CONTROL_RATE
Ead env2(CONTROL_RATE);
Ead env3(CONTROL_RATE);
Ead env4(CONTROL_RATE);
Ead env5(CONTROL_RATE);

Ead kickEnv(CONTROL_RATE);
Ead snareEnv(CONTROL_RATE);
Ead hihatEnv(CONTROL_RATE);

int envGain1 = 0;
int envGain2 = 0;
int envGain3 = 0;
int envGain4 = 0;
int envGain5 = 0;

int envGainKick = 0;
int envGainSnare = 0;
int envGainHiHat = 0;

int leftPotVolume = 0;
int rightPotVolume = 0;

int am9Chord[] = { 69, 72, 76, 79, 83 };
int dm9Chord[] = { 62, 65, 69, 72, 79 };
int am9LowerChord[] = { 57, 60, 64, 67, 71 };
int dm9LowerChord[] = { 50, 53, 57, 60, 64 };

EventDelay quarterNoteDelay;
EventDelay eighthNoteDelay;
int quarterNoteLength = 500;
int eighthNoteLength = int(quarterNoteLength / 2);

const int imc0Pin = 18;
const int imc1Pin = 23;
int imc0Val = 0;
int imc0Prev = 0;
int imc0Prev1 = 0;
int imc1Val = 0;
int imc1Prev = 0;

int imcMsList[] = { 0, 0 };

// 0: nothing
// 1: am9
// 2: dm9
int baseChordPulseList[] = { 2, 1, 2, 1, 2, 1, 2 };
int i = 0;

int buttonPressed[] = { 0, 0, 0, 0, 0, 0, 0, 0 };


// 0: nothing
// 1: kick
// 2: snare
int percPulseList[] = { 1, 1, 2, 0, 2, 2, 1, 1, 2, 0, 2, 2, 0, 1 };
int percPulseListBreakdown[] = { 1, 0, 1, 0, 2, 0, 0, 0, 2, 0, 2, 0, 1, 0 };
int percPulseListDefault[] = { 1, 1, 2, 0, 2, 2, 1, 1, 2, 0, 2, 2, 0, 1 };
int hiHatList[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
int hiHatListBreakdown[] = { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
int hiHatListUltraBreakdown[] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0 };
int hiHatListDefault[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
int j = 0;

int chordWordBank[] = { 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4 };

// counter that allows for two pulses from leader to determine beat length
int k = 0;

void setup() {
  Serial.begin(115200);
  pinMode(34, INPUT);
  startMozzi();

  kick.setFreq(50);
  snare.setFreq(100);
  hihat.setFreq(500);

  pinMode(imc0Pin, INPUT);
  pinMode(imc1Pin, INPUT);

  setAtkDec(20, 500);  // sets all envelopes at once, function defined at bottom of code

  kickSetAtkDec(5, 100);
  snareSetAtkDec(5, 50);
  hatSetAtkDec(5, 25);
}


void loop() {
  audioHook();
}


void updateControl() {
  // -----THIS LINE SHOULD BE PLACED AT THE TOP OF YOUR updateControl() FUNCTION----
  readPots();  // reads potentiometers
  readImc();

  // order of multiplexer (from top to bottom) {5, 7, 6, 4, 0, 1, 2, 3}

  if (k < 2) {
    leftPotVolume = 0;
    rightPotVolume = 0;
  } else {
    int imc0Val1 = digitalRead(imc0Pin);
    if (imc0Prev1 != imc0Val1 && imc0Val1 == HIGH) {
      int chordPulseList[] = { 0, 0, 0, 0, 0, 0, 0 };
      for (int i = 0; i < 7; i++) {
        chordPulseList[i] = baseChordPulseList[i];
      }
      if (chordPulseList[i] == 1) {
        osc1.setFreq(mtof(am9Chord[0]));
        osc2.setFreq(mtof(am9Chord[1]));
        osc3.setFreq(mtof(am9Chord[2]));
        osc4.setFreq(mtof(am9Chord[3]));
        osc5.setFreq(mtof(am9Chord[4]));
      } else if (chordPulseList[i] == 2) {
        osc1.setFreq(mtof(dm9Chord[0]));
        osc2.setFreq(mtof(dm9Chord[1]));
        osc3.setFreq(mtof(dm9Chord[2]));
        osc4.setFreq(mtof(dm9Chord[3]));
        osc5.setFreq(mtof(dm9Chord[4]));
      } else if (chordPulseList[i] == 0) {
        osc1.setFreq(0);
        osc2.setFreq(0);
        osc3.setFreq(0);
        osc4.setFreq(0);
        osc5.setFreq(0);
      } else if (chordPulseList[i] == 3) {
        osc1.setFreq(mtof(am9LowerChord[0]));
        osc2.setFreq(mtof(am9LowerChord[1]));
        osc3.setFreq(mtof(am9LowerChord[2]));
        osc4.setFreq(mtof(am9LowerChord[3]));
        osc5.setFreq(mtof(am9LowerChord[4]));
      }
      else if (chordPulseList[i] == 4){
        osc1.setFreq(mtof(dm9LowerChord[0]));
        osc2.setFreq(mtof(dm9LowerChord[1]));
        osc3.setFreq(mtof(dm9LowerChord[2]));
        osc4.setFreq(mtof(dm9LowerChord[3]));
        osc5.setFreq(mtof(dm9LowerChord[4]));
      }

      env1.start();
      env2.start();
      env3.start();
      env4.start();
      env5.start();



      if (i == 7) {

        // randomize next bar of chords
        if (buttonPressed[0] == 1) {
          randomizeChordList();
          buttonPressed[0] = 0;
        } else {
          buttonPressed[0] = 0;
        }


        if (buttonPressed[1] == 1) {
          toggleShortADSR();
          buttonPressed[1] = 0;
        } else {
          buttonPressed[1] = 0;
        }


        if (buttonPressed[2] == 1) {
          toggleLongADSR();
          buttonPressed[2] = 0;
        } else {
          buttonPressed[2] = 0;
        }

        // switch back to default pattern
        if (buttonPressed[4] == 1) {
          defaultSwitch();
          buttonPressed[4] = 0;
        } else {
          buttonPressed[4] = 0;
        }

        // switch to breakdown pattern
        if (buttonPressed[5] == 1) {
          breakdownSwitch();
          buttonPressed[5] = 0;
        } else {
          buttonPressed[5] = 0;
        }

        // switch to ultra breakdown
        if (buttonPressed[6] == 1) {
          ultraBreakdownSwitch();
          buttonPressed[6] = 0;
        } else {
          buttonPressed[6] = 0;
        }


        if (buttonPressed[7] == 1) {
          buttonPressed[7] = 0;
        } else {
          buttonPressed[7] = 0;
        }

        i = 0;
      }
      i++;
    }
    imc0Prev1 = imc0Val1;

    envGain1 = env1.next();
    envGain2 = env2.next();
    envGain3 = env3.next();
    envGain4 = env4.next();
    envGain5 = env5.next();

    envGainKick = kickEnv.next();
    envGainSnare = snareEnv.next();
    envGainHiHat = hihatEnv.next();

    leftPotVolume = map(potVals[0], 0, 4095, 0, 255);
    rightPotVolume = map(potVals[1], 0, 4095, 0, 31);

    if (readAux(5) == 1) {
      // Serial.println("you pressed button 1");
      buttonPressed[0] = 1;
    }
    if (readAux(7) == 1) {
      // Serial.println("you pressed button 2");
      buttonPressed[1] = 1;
    }
    if (readAux(6) == 1) {
      // Serial.println("you pressed button 3");
      buttonPressed[2] = 1;
    }
    if (readAux(4) == 1) {
      // Serial.println("you pressed button 4");
      buttonPressed[3] = 1;
    }
    if (readAux(0) == 1) {
      // Serial.println("you pressed button 5");
      buttonPressed[4] = 1;
    }
    if (readAux(3) == 1) {
      // Serial.println("you pressed button 6");
      buttonPressed[5] = 1;
    }
    if (readAux(2) == 1) {
      // Serial.println("you pressed button 7");
      buttonPressed[6] = 1;
    }
    if (readAux(1) == 1) {
      // Serial.println("you pressed button 8");
      buttonPressed[7] = 1;
    }
  }

  int imc1Val = digitalRead(imc1Pin);
  if (imc1Prev != imc1Val && imc1Val == HIGH) {
    if (percPulseList[j] == 1) {
      kickEnv.start();
    } else if (percPulseList[j] == 2) {
      snareEnv.start();
    }
    if (hiHatList[j] == 1) {
      hihatEnv.start();
    }

    if (buttonPressed[3] == 1) {
      randomizeAtkDec();
      buttonPressed[3] = 0;
    } else {
      buttonPressed[3] = 0;
    }

    if (j == 14) {
      j = 0;
    }
    j++;
  }
  imc1Prev = imc1Val;
}

void randomizeChordList() {
  for (int i = 0; i < 7; i++) {
    int myRandomNum = irand(0, 12);
    baseChordPulseList[i] = chordWordBank[myRandomNum];
  }
  // Serial.println("randomize chord list is operating");
}

void randomizeAtkDec() {
  int myRandomNum = irand(25, 500);
  int myRandomNum2 = irand(25, 500);
  setAtkDec(myRandomNum, myRandomNum2);
  // Serial.print("randomize atk: ");
  // Serial.print(myRandomNum);
  // Serial.print(" dec: ");
  // Serial.println(myRandomNum2);
}

void toggleShortADSR() {
  setAtkDec(20, 150);
}

void toggleLongADSR() {
  setAtkDec(300, 200);
}

void breakdownSwitch() {
  // Serial.println("switching to breakdown");
  for (int i = 0; i < 14; i++) {
    percPulseList[i] = percPulseListBreakdown[i];
    hiHatList[i] = hiHatListBreakdown[i];
    hatSetAtkDec(5, 500);
  }
}

void defaultSwitch() {
  // Serial.println("switching to default");
  for (int i = 0; i < 14; i++) {
    percPulseList[i] = percPulseListDefault[i];
    hiHatList[i] = hiHatListDefault[i];
    hatSetAtkDec(5, 25);
  }
}

void ultraBreakdownSwitch() {
  // Serial.println("switching to ultra breakdown");
  for (int i = 0; i < 14; i++) {
    percPulseList[i] = percPulseListBreakdown[i];
    hiHatList[i] = hiHatListUltraBreakdown[i];
    hatSetAtkDec(5, 1000);
  }
}

void printArray(int myArray[], int length) {
  Serial.print("Array: ");
  for (int i = 0; i < length; i++) {
    Serial.print(myArray[i]);
  }
  Serial.println("");
}

int calcChordVolume() {
  return envGain1 * osc1.next() + envGain2 * osc2.next() + envGain3 * osc3.next() + envGain4 * osc4.next() + envGain5 * osc5.next();
}

int calcPercVolume() {
  return envGainKick * kick.next() + envGainSnare * snare.next() + envGainHiHat * hihat.next();
}

int updateAudio() {
  return MonoOutput::fromAlmostNBit(24, calcChordVolume() * rightPotVolume + calcPercVolume() * leftPotVolume);
}


void readPots() {
  mozziAnalogRead(0);
  potVals[0] = mozziAnalogRead(39);

  mozziAnalogRead(1);
  potVals[1] = mozziAnalogRead(36);
}

int readAux(int pin) {
  aux.read(pin);  //read once and throw away for reliability
  return aux.read(pin);
}

long irand(long howsmall, long howbig) {
  howbig++;
  if (howsmall >= howbig) {
    return howsmall;
  }
  long diff = howbig - howsmall;
  return (xorshift96() % diff) + howsmall;
}

void setAtkDec(int atk, int dec) {
  env1.setAttack(atk);
  env1.setDecay(dec);
  env2.setAttack(atk);
  env2.setDecay(dec);
  env3.setAttack(atk);
  env3.setDecay(dec);
  env4.setAttack(atk);
  env4.setDecay(dec);
  env5.setAttack(atk);
  env5.setDecay(dec);
}

void kickSetAtkDec(int atk, int dec) {
  kickEnv.setAttack(atk);
  kickEnv.setDecay(dec);
}

void snareSetAtkDec(int atk, int dec) {
  snareEnv.setAttack(atk);
  snareEnv.setDecay(dec);
}

void hatSetAtkDec(int atk, int dec) {
  hihatEnv.setAttack(atk);
  hihatEnv.setDecay(dec);
}

// follower imc reading
void readImc() {
  int imc0Val = digitalRead(imc0Pin);
  if (imc0Prev != imc0Val && imc0Val == HIGH) {
    k++;
    // Serial.println(" imc0 high");
  }
  imc0Prev = imc0Val;

  int imc1Val = digitalRead(imc1Pin);
  if (imc1Prev != imc1Val && imc1Val == HIGH) {
    // Serial.println("imc1 high");
  }

  if (k > 2) {
    k = 2;
  }
}
