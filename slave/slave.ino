//Receiver Code
#include<SoftwareSerial.h>
#include "U8glib.h"
#include "displayArray.h"

SoftwareSerial  SUART(2, 3); //SRX of UNO = 2, STX of UNO = 3
U8GLIB_SH1106_128X64 u8g(U8G_I2C_OPT_NO_ACK);  // Display which does not send ACK

void displayOLED(int mode) {
    
  u8g.firstPage();//First page display
  do
  {
    if(mode == 0) u8g.drawBitmapP( 0, 0, 16, 64, mute);
    else if(mode == 1) u8g.drawBitmapP( 0, 0, 16, 64, echo);
    else if(mode == 2) u8g.drawBitmapP( 0, 0, 16, 64, low_pitch);
    else if(mode == 3) u8g.drawBitmapP( 0, 0, 16, 64, reverse);
    else if(mode == 4) u8g.drawBitmapP( 0, 0, 16, 64, crush_LOW);
    else if(mode == 5) u8g.drawBitmapP( 0, 0, 16, 64, crush_BAND);
    else if(mode == 6) u8g.drawBitmapP( 0, 0, 16, 64, crush_HIGH);
  } while (u8g.nextPage());
}

void setup()
{
  Serial.begin(9600);
  SUART.begin(9600);
}

void loop()
{
  unsigned int val;
  while (!SUART.available()) {}
  //delay(100); //allows all serial sent to be received together
  byte b1 = SUART.read();
  while (!SUART.available())
  {}
  //  delay(100); //allows all serial sent to be received together
  byte b2 = SUART.read();

  val = b1*256  + b2;// * 256 ; //46080 ==> 
  Serial.println(val);  //shows: 180
  displayOLED(val);
  
}
