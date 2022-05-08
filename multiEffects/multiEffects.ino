/*
  Audio Hacker Library
  Copyright (C) 2013 nootropic design, LLC
  All rights reserved.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
  Echo Effect for Audio Hacker.

  See Audio Hacker project page for details.
  http://nootropicdesign.com/audiohacker/projects.html
 */

#include <AudioHacker.h>
#include<Wire.h>//This library is used for I2C communication
#include<SoftwareSerial.h>
SoftwareSerial SUART(2, 3); //SRX = 2 of UNO

#define DEBUG
#define ADD_BUTTON 5
#define REVERSE_BUTTON 6
/*Different modes*/
# define ECHO 1
# define VOICECHANGER 2
# define REVERSE 3
# define CRUSH_LOW_PASS 4
# define CRUSH_BAND_PASS 5
# define CRUSH_HIGH_PASS 6

int mode = 0;
/* -------------- ECHO EFFECT ------------------*/
unsigned int playbackBuf;
unsigned int sampleRate;
unsigned int readBuf[2];
unsigned int writeBuf;
boolean evenCycle = true;
unsigned int timer1Start;
volatile unsigned int timer1EndEven;
volatile unsigned int timer1EndOdd;
unsigned long lastDebugPrint = 0;
volatile long address = 0;
unsigned int echoDelay;
boolean echoWrapped = false;
/* ---------------------------------------------*/
/* --------------- VOICE CHARNGER --------------*/
#define GRAINSIZE 512

volatile unsigned int output = 2048;
//unsigned int timer1Start;
volatile unsigned int timer1End;
volatile unsigned int counter = 0;
byte counterMod = 2;
//unsigned long lastDebugPrint = 0;
boolean normal = false;
volatile long readAddress = 0;
volatile long writeAddress = 0;
unsigned int buf[GRAINSIZE];
/* ---------------------------------------------*/
/* --------------- REVERSE ---------------------*/
#define HALF_MEMORY 65534
unsigned int output_Reverse;
unsigned int sampleRate_Reverse;
unsigned int timer1Start_Reverse;
volatile long writeAddress_Reverse = 0;
volatile long readAddress_Reverse = HALF_MEMORY;
volatile boolean bufferFilled = false;
/* ---------------------------------------------*/
/* --------------- filter ---------------------*/
#define LOW_PASS 0
#define BAND_PASS 1
#define HIGH_PASS 2
boolean on = true;

int filterCutoff = 255;
int filterResonance = 255;
long feedback;
int buf0 = 0;
int buf1 = 0;
byte filterType = LOW_PASS;

//unsigned long lastDebugPrint = 0;
/* ---------------------------------------------*/

void setup() {
#ifdef DEBUG
  Serial.begin(115200);        // connect to the serial port
#endif
   /* --------------- ECHO + CTUSH ---------------------*/
  playbackBuf = 2048;
  sampleRate = DEFAULT_RECORDING_SAMPLE_RATE;
  timer1Start = UINT16_MAX - (F_CPU / sampleRate);
  
   /* --------------- REVERSE ---------------------*/
  output_Reverse = 2048; // silence
  sampleRate_Reverse = 18000;
  timer1Start_Reverse = UINT16_MAX - (F_CPU / sampleRate_Reverse);
  /* ---------------------------------------------*/
  // Setup Switch Button
  pinMode(ADD_BUTTON, INPUT);
  digitalWrite(ADD_BUTTON, HIGH);
  pinMode(REVERSE_BUTTON, INPUT);
  digitalWrite(REVERSE_BUTTON, HIGH);
// /* ---------------- Wire Communication ----------*/
//  Wire.begin();
  /* ---------------------------------------------*/
  /* ---------------- Wire Communication ----------*/
   Serial.begin(9600);
   SUART.begin(9600);
   /* ---------------------------------------------*/
  AudioHacker.begin();

#ifdef DEBUG
  Serial.print("sample rate = ");
  Serial.print(sampleRate);
  Serial.print(" Hz");
  Serial.println();
#endif
}

void loop() {
/* ----------- echo ------------------- */
  delay(300);
  // echoDelay is number of memory slots back into the past to read for the echo.
  // must be a factor of 3 because we pack 2 samples into each 3-byte sequence of SRAM.
  echoDelay = analogRead(0) * 30; 
/* ------------------------------------ */
/* ------------- reverse -------------- */
  counterMod = map(analogRead(0), 0, 1024, 2, 11);
  normal = (counterMod == 10);
/* ------------------------------------ */
/* ------------- CRUSH -------------- */
  filterCutoff = analogRead(0) >> 2;
  filterResonance = analogRead(1) >> 2;
  feedback = (long)filterResonance + (long)(((long)filterResonance * ((int)255 - (255-filterCutoff))) >> 8);
/* -------------- changing mode ---------------- */
  
  if (digitalRead(ADD_BUTTON) == LOW) {
    mode++;
   if (mode > 6) {  
    mode  = 0;
   }
   Serial.println(mode);
  }
  if (digitalRead(REVERSE_BUTTON) == LOW) {
    mode--;
    if (mode < 0) {
      mode = 6;
    }
    Serial.println(mode);
  }
  transmit(mode);
//  screenDisplay(mode);
/* ------------------------------------ */
#ifdef DEBUG
  if ((millis() - lastDebugPrint) >= 1000) {
    lastDebugPrint = millis();

    //each 3 echoDelay2 is 2 samples.  
    unsigned int delayMillis = (float)(((float)((echoDelay * 2) / 3)) / (float)(sampleRate/1000.0));

    Serial.print("echo delay = ");
    Serial.print(delayMillis);
    Serial.print(" ms    even cycles remaining = ");
    Serial.print(UINT16_MAX - timer1EndEven);
    Serial.print("   odd cycles remaining = ");
    Serial.print(UINT16_MAX - timer1EndOdd);
    Serial.println();
  }
#endif
}


void transmit(int mode) {
    SUART.write(mode / 256);
    SUART.write(mode % 256);
//    delay(250);
#ifdef DEBUG
  Serial.print("Transmit: ");
  Serial.println(mode);
#endif
}

ISR(TIMER1_OVF_vect) {
  if (mode == ECHO){
    TCNT1 = timer1Start;
    unsigned int signal;
    unsigned int echo;
    int mix;
  
    AudioHacker.writeDAC(playbackBuf);
  
    // Read ADC
    signal = AudioHacker.readADC();
  
  
    if (evenCycle) {
      long echoAddress = address - echoDelay;
      if (echoAddress < 0) {
        echoAddress += MAX_ADDR;
      }
      AudioHacker.readSRAMPacked(1, echoAddress, readBuf);
      if ((!echoWrapped) && (echoAddress > address)) {
        // avoid reading from unwritten memory
        echo = 2048;
        readBuf[1] = 2048;
      } else {
        echo = readBuf[0];
      }
    } else {
      echo = readBuf[1];
    }
    if (echoDelay == 0) {
      echo = 2048;
    }
  
    if (evenCycle) {
      writeBuf = signal;
    } else {
      AudioHacker.writeSRAMPacked(1, address, writeBuf, signal);
      address += 3;
      if (address > MAX_ADDR) {
        address = 0;
        echoWrapped = true;
      }
    }
  
  
    mix = signal-2048;
    echo = echo >> 1; // attenuate echo
    mix += (echo - 1024); // since we are dividing echo by 2, decrement by 1024
    if (mix < -2048) {
      mix = -2048;
    } else {
      if (mix > 2047) {
        mix = 2047;
      }
    }
    playbackBuf = mix + 2048;
  
  
  #ifdef DEBUG
    if (evenCycle) {
      timer1EndEven = TCNT1;
    } else {
      timer1EndOdd = TCNT1;
    }
  #endif
    evenCycle = !evenCycle;
    
  } 
  /* =========================================================== */
  else if (mode == VOICECHANGER) {
    TCNT1 = timer1Start;
    unsigned int signal;
    boolean updateTime = false;
  
    AudioHacker.writeDAC(output);
  
    signal = AudioHacker.readADC();
    buf[writeAddress] = signal;
  
    counter++;
    if (((counter % counterMod) != 0) || (normal)) {
      output = buf[readAddress];
      readAddress++;
  
      // cross-fade output if we are approaching the end of the grain
      // mix the output with the current realtime signal
      unsigned int distance = GRAINSIZE - writeAddress;
      if (distance <= 16) {
        // weighted average of output and current input yields 16 bit number
        unsigned int s = (output * distance) + (signal * (16-distance)); 
        s = s >> 4; // reduce to 12 bits.
        output = s;
      }
    }
  
    writeAddress++;
    if (writeAddress >= GRAINSIZE) {
      writeAddress = 0; // loop around to beginning of grain
      readAddress = 0;
    }
#ifdef DEBUG
  timer1End = TCNT1;
#endif
  } 
  /* =========================================================== */
  else if (mode == REVERSE) {
    TCNT1 = timer1Start_Reverse;
    unsigned int input;
  
    AudioHacker.writeDAC(output_Reverse);
  
    // Read ADC
    input = AudioHacker.readADC();
  
    if (bufferFilled) {
      AudioHacker.readSRAM(0, readAddress_Reverse, (byte *)&output_Reverse, 2);
      readAddress_Reverse -= 2; // each sample uses 2 bytes. Read backwards in memory.
      if (readAddress_Reverse < 0) {
        readAddress_Reverse = MAX_ADDR-1;
      }
    }
  
    AudioHacker.writeSRAM(0, writeAddress_Reverse, (byte *)&input, 2);
    writeAddress_Reverse += 2; // each sample uses 2 bytes. Write forward in memory.
    if (writeAddress_Reverse > HALF_MEMORY) {
      bufferFilled = true;
    }
    if (writeAddress_Reverse > MAX_ADDR) {
      writeAddress_Reverse = 0;
    }
  }
  /* =========================================================== */
  else if (mode == CRUSH_LOW_PASS) {
    TCNT1 = timer1Start;
    AudioHacker.writeDAC(playbackBuf);
    int reading = AudioHacker.readADC() - 2048;

    int highPass = reading - buf0;
    int bandPass = buf0 - buf1;
    int tmp = highPass + (feedback * bandPass >> 8);
    buf0 += ((long)filterCutoff * tmp) >> 8;
    buf1 += ((long)filterCutoff * (buf0 - buf1)) >> 8;
    reading = buf1;
    playbackBuf = reading + 2048;
  }  
  else if (mode == CRUSH_BAND_PASS) {
    TCNT1 = timer1Start;
    AudioHacker.writeDAC(playbackBuf);
    int reading = AudioHacker.readADC() - 2048;

    int highPass = reading - buf0;
    int bandPass = buf0 - buf1;
    int tmp = highPass + (feedback * bandPass >> 8);
    buf0 += ((long)filterCutoff * tmp) >> 8;
    buf1 += ((long)filterCutoff * (buf0 - buf1)) >> 8;
    reading = bandPass;
    playbackBuf = reading + 2048;
  }
  else if (mode == CRUSH_HIGH_PASS) {
    TCNT1 = timer1Start;
    AudioHacker.writeDAC(playbackBuf);
    int reading = AudioHacker.readADC() - 2048;

    int highPass = reading - buf0;
    int bandPass = buf0 - buf1;
    int tmp = highPass + (feedback * bandPass >> 8);
    buf0 += ((long)filterCutoff * tmp) >> 8;
    buf1 += ((long)filterCutoff * (buf0 - buf1)) >> 8;
    reading = highPass;
    playbackBuf = reading + 2048;
  }
}
