#include <Arduino.h>
#include <U8g2lib.h>
 
#include <SPI.h>

#define ENCODER_A_PIN 2
#define ENCODER_B_PIN 7
#define NOT_INITED 2

#define POSITIVE_ENCODER_LIMIT 255
#define NEGATIVE_ENCODER_LIMIT -255

#define SPI_SPEED_EVOL 4000000  //in HZ (4 Mhz, maximum for Nano)
#define EVOL_CS A2



SPISettings spi_settings(SPI_SPEED_EVOL, MSBFIRST, SPI_MODE0);



volatile int encoder_value = 0;
volatile int encoder_previous_value = 0;
volatile int delta = 0;

volatile int a0; 
volatile int c0;

//0..0xf values are just an exaple to store valid data for debugging purposes. 
uint8_t evol_volume_values[16] = {0, 1 , 2, 3 , 4 , 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};  //16 byte arr to store 4  by 4 groups of eVol byte volume values


/*
init display
*/
U8G2_ST7565_ERC12864_1_4W_HW_SPI display(U8G2_R0, A1, A0, 5); //should be DEFINED constants


void setup() {

  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  pinMode(EVOL_CS, OUTPUT);
  digitalWrite(EVOL_CS, HIGH);  


  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), readEncoder, CHANGE);

  display.begin(); 
  display.setContrast (15);  //should be defined
  display.enableUTF8Print();
  Serial.begin(9600);

}

void loop() {

    display.firstPage();
    do {  
      display.setFont(u8g2_font_mystery_quest_42_tn);
      // display.setCursor(25, 20);
      // display.print("Rukozhop-Audio");
      // display.setCursor(25, 40);
      // display.print("VER:1.1");
      // delta =  encoder_value - encoder_previous_value;
      display.setCursor(30, 40); //should not be magic numbers
      display.print(encoder_value);
      if (encoder_value != encoder_previous_value) {
        encoder_previous_value = encoder_value;
        sendVolumeToEvol(evol_volume_values, 16);
      }
  } while ( display.nextPage());

}

void readEncoder() {

  int a = digitalRead(ENCODER_A_PIN);
  int b = digitalRead(ENCODER_B_PIN);

if (a != a0) {                  
    a0 = a;
    if (b != c0) {
      c0 = b;
      ChangeValue(a == b);
    }
  }
}

void ChangeValue (bool Up) {
  encoder_value = max(min((encoder_value + (Up ? 1 : -1)), POSITIVE_ENCODER_LIMIT), NEGATIVE_ENCODER_LIMIT);
  Serial.println(encoder_value);
}

void sendVolumeToEvol(uint8_t values[], int size) {
  SPI.beginTransaction(spi_settings); 
  digitalWrite(EVOL_CS, LOW);  
  SPI.transfer(values, size);
  digitalWrite(EVOL_CS, HIGH);   
  SPI.endTransaction();
}
