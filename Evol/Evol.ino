#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <BfButton.h>
#include <Bounce2.h>
#include <EEPROM.h>

#define INIT_ADDR 1023
#define INIT_KEY 99

//#define NOT_INITED 2
#define ENCODER_A_PIN 2
#define ENCODER_B_PIN 7
#define BTN_PIN 3
#define HP 20 //под гарнитуру?

#define POSITIVE_ENCODER_LIMIT 255
#define NEGATIVE_ENCODER_LIMIT 1
#define ENCODER_OFF_STATE 2

#define SPI_SPEED_EVOL 4000000  //in HZ (4 Mhz, maximum for Nano)
#define EVOL_CS A2

SPISettings spi_settings(SPI_SPEED_EVOL, MSBFIRST, SPI_MODE0);
//0..0xf values are just an exaple to store valid data for debugging purposes. 
uint8_t evol_volume_values[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};  //16 byte arr to store 4  by 4 groups of eVol byte volume values

volatile int encoder_value = 92;
volatile int encoder_previous_value = 92;
volatile int delta = 0;

volatile int a0; 
volatile int c0;
volatile int Offset;

volatile bool fmenu, fparam, fHP, fSUB, fvol = false;
volatile int mi, pi = 0;
volatile int hpLastState, currentStateHP;
unsigned long btime;
int MenuLength;
volatile int address;

const float rotaryAccelerationCoef = 500;
volatile bool lastMovementDirection,currentDirection = false;
unsigned long lastMovementAt;
unsigned long accelerationLongCutoffMillis = 200;
unsigned long accelerationShortCutffMillis = 35;    



/*
init display
*/
U8G2_ST7565_ERC12864_1_4W_HW_SPI display(U8G2_R0, A1, A0, 5); //should be DEFINED constants

//Объект под гарнитуру, будем слушать что кому то захотелось чота куда то вставить или переключить?
Bounce debouncer = Bounce();

BfButton btn(BfButton::STANDALONE_DIGITAL, BTN_PIN, true, LOW);

typedef struct _evolMenuItems{
  char *Caption;
  int input1;
  int input2;
  int value;
};

typedef struct _evolData{
  int Vol;
  int Sub;
  _evolMenuItems evolMenu[9] = {
    {"TW L", 0, 2, 0},    
    {"TW R", 1, 3, 0},
    {"MID L", 4, 6, 0},
    {"MID R", 5, 7, 0},
    {"SUB L", 8, 10, 0},
    {"SUB R", 9, 11, 0},
    {"HP L", 12, 14, 0},
    {"HP R", 13, 15, 0},
    {"STEP", 0, 0, 1}, //need last position in menu
  };
};

volatile _evolData evolParams;

void setup() {

  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  pinMode(EVOL_CS, OUTPUT);
  digitalWrite(EVOL_CS, HIGH);  
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), readEncoder, CHANGE);

  //Button settings
  pinMode(BTN_PIN,INPUT_PULLUP);
  btn.onPress(pressHandler)
  .onDoublePress(pressHandler) // default timeout
  .onPressFor(pressHandler, 1000); // custom timeout for 1 second  

  //тек состояние гарнитуры 
  hpLastState = digitalRead(HP);
  
  display.begin(); 
  display.setContrast (15);  //should be defined
  display.enableUTF8Print();
  Serial.begin(9600);
  //EEPROM.begin();
  
  if (EEPROM.read(INIT_ADDR) != INIT_KEY) {
    EEPROM.write(INIT_ADDR, INIT_KEY);
    evolParams.Vol = 92;
    evolParams.Sub = 0;
    EEPROM.put(0, evolParams);
  }
  else{
    //Serial.println("it's good");
    EEPROM.get(0, evolParams); 
  }

  //evolParams.Vol = 92;
  //evolParams.Sub = 0;
  //EEPROM.put(0, evolParams);  
  //EEPROM.get(0, evolParams);  
  encoder_value = evolParams.Vol;
  encoder_previous_value = encoder_value;
  out_volume(hpLastState);
  btime = millis();
  MenuLength = sizeof(evolParams.evolMenu) / sizeof(evolParams.evolMenu[0]);
  //я так понимаю надо сразу насрать в эфир?
  setVolumeToEvol();
  sendVolumeToEvol(evol_volume_values, 16);
}

void loop() {
  btn.read();  
  if (encoder_value != encoder_previous_value) {
    if (encoder_previous_value < encoder_value) { currentDirection = 1; } else { currentDirection = 0; } //need for speed/step func
    encoder_previous_value = encoder_value;
    //sendVolumeToEvol(evol_volume_values, 16);
    if (!fmenu && !fSUB) {
      fvol = true;
      if (evolParams.evolMenu[MenuLength-1].value > 0) {
        Serial.println("step");
        rotaryStep();
      }
      else {
        if (lastMovementDirection == currentDirection) {  
          rotatySpeed();
          Serial.println("speed");
        }
      }    
      lastMovementAt = millis();
      lastMovementDirection = currentDirection; 
      encoder_previous_value = encoder_value;
      evolParams.Vol = encoder_value;
      out_volume(hpLastState);
      setVolumeToEvol();
      sendVolumeToEvol(evol_volume_values, 16);
    }
    else if (fmenu && !fparam) {
      if (encoder_value < 0) { encoder_value = 0; } else if (encoder_value > MenuLength-1) { encoder_value = MenuLength-1; }
      mi = encoder_value;
      DrawMenu();
    }
    else if (fmenu && fparam) {
      if (mi == MenuLength-1 && encoder_value < 0) { encoder_value = 0; }
      evolParams.evolMenu[mi].value = encoder_value;
      DrawMenu();  
      setVolumeToEvol();
      sendVolumeToEvol(evol_volume_values, 16);
    }
    else if (fSUB) {
      evolParams.Sub = encoder_value;
      out_sub();
      setVolumeToEvol();
      sendVolumeToEvol(evol_volume_values, 16);  
    }
  }
  /* save in 5 minute */
  if (fvol && (millis() - btime) > 300000) {
    btime = millis();
    fvol  = false;
    EEPROM.put(0, evolParams);
  }
  
  /* IN/OUT HP */
  /*
  bool currentStateHP = digitalRead(HP);
  if (currentStateHP != hpLastState && currentStateHP == 1){
    hpLastState = currentStateHP; 
    out_volume(currentStateHP);
  }
  */
  /*  
  //Это состояние переключателя который рисовал Мишка для ушей
  debouncer.update();
  int value = debouncer.read();
  if ( value == LOW ) {
    //Serial.println("HP OFF");
    fHP = false;
  }
  else {    
    //Serial.println("HP ON");
    fHP = true;
  } 
  */  
}

void readEncoder() {
  int a = digitalRead(ENCODER_A_PIN);
  int b = digitalRead(ENCODER_B_PIN);
  if (a != a0) {                  
    a0 = a;
    if (c0 == ENCODER_OFF_STATE) {
      c0 = b;
      return;
    }
    if (b != c0) {
      c0 = b;
      processEncoderRotation(a == b);
      c0 = ENCODER_OFF_STATE;
    }
  }
}  

void processEncoderRotation (bool Up) {
  encoder_value = encoder_value + (Up ? 1 : -1);
}

void sendVolumeToEvol(uint8_t values[], int size) {
  SPI.beginTransaction(spi_settings); 
  digitalWrite(EVOL_CS, LOW);  
  SPI.transfer(values, size);
  digitalWrite(EVOL_CS, HIGH);   
  SPI.endTransaction();
}

void setVolumeToEvol() {
  int i;
  for (i = 0; i < MenuLength-1; i++) {
    evol_volume_values[evolParams.evolMenu[i].input1] = max(min((evolParams.Vol + evolParams.evolMenu[i].value), POSITIVE_ENCODER_LIMIT), NEGATIVE_ENCODER_LIMIT); 
    evol_volume_values[evolParams.evolMenu[i].input2] = max(min((evolParams.Vol + evolParams.evolMenu[i].value), POSITIVE_ENCODER_LIMIT), NEGATIVE_ENCODER_LIMIT); 
  }  
}

void out_volume(int pos) {
  display.firstPage();
    do {  
      display.setFont(u8g2_font_luIS18_tf);
      display.setCursor(0,40);
      if (pos == LOW) {
        display.print("DN:");
      }
      else {
        display.print("HP: ");
      }
      display.setCursor(60,40);
      display.print(String(count_db(),1));
    } while ( display.nextPage());
}

void out_sub() {
  display.firstPage();
    do {  
      display.setFont(u8g2_font_luIS18_tr);
      display.setCursor(0,40);
      display.print("SUB:");
      display.setCursor(60,40);
      int sub = evolParams.Sub;     
      display.print(String(sub/(float)2,1));
    } while ( display.nextPage());
}

//get dB +31.5/-95.5
float count_db() {
  int p = evolParams.Vol;  
  float db = 31.5 - (0.5*(255-p));
  return db;
}

void DrawMenu() {
  int k, p, CursorPos, y;
  display.firstPage();
  do {  
    display.setFont(u8g2_font_6x10_mr);
    if (mi < 6) {
      Offset = 0;
      k = 6;
      CursorPos = mi-Offset;
    }
    else {
      Offset = 6;
      k = 3;
      CursorPos = mi-Offset;
    }
    y = 0;     
    //for (int i = 0 ; i < k ; i++) { //хотите артефакт верните
    for (int i = 0 ; i < 6 ; i++) { //ахуеть дайте 2 литра и немного пива, если не дорисовывать экран получите артефакт при выходе из меню 
      display.setCursor(1, y = y+10);
      if (i == CursorPos) {
        display.setFontMode(0);
        display.setDrawColor(0);
        display.print(">");          
      }
      else {
        display.setFontMode(1);
        display.setDrawColor(1);
      }
      //условие решения проблемы артефакта
      if (k == 3 && i > 2) { //ахуеть дайте 2 литра и немного пива, если не дорисовывать экран получите артефакт при выходе из меню
        display.setCursor(0, y);
        display.print("");
      }
      else {
        display.setCursor(10, y);
        display.print(String(evolParams.evolMenu[i+Offset].Caption));
        display.setCursor(50, y);
        p = evolParams.evolMenu[i+Offset].value;
        display.print(p);        
      }
    }
  } while ( display.nextPage());  
}  

//Button press hanlding function
void pressHandler (BfButton *btn, BfButton::press_pattern_t pattern) {
  switch (pattern) {
    case BfButton::SINGLE_PRESS:
      //Serial.println("Single push");      
      if (fmenu && !fparam) {
        //Serial.println("in params");
        fparam = true;
        mi = encoder_value;
        pi = evolParams.evolMenu[mi].value;
        encoder_previous_value = pi; 
        encoder_value = pi;
      }
      else if (fmenu && fparam) {
        //Serial.println("out params");
        fparam = false;
        EEPROM.put(0, evolParams);
        encoder_previous_value = mi;
        encoder_value = mi;
      }
      else if (!fSUB){
        //Serial.println("go sub");
        fSUB = true;
        encoder_previous_value = evolParams.Sub;
        encoder_value = evolParams.Sub;
        out_sub();
      }
      else if (fSUB){
        //Serial.println("out sub");
        fSUB = false;
        EEPROM.put(0, evolParams);
        encoder_previous_value = evolParams.Vol;
        encoder_value = evolParams.Vol;
        out_volume(hpLastState);                
      }
      break;
      
    case BfButton::DOUBLE_PRESS:
      Serial.println("Double push");
      //Serial.println("Need mute on/off");
      break;
      
    case BfButton::LONG_PRESS:    
      //Serial.println("Long push");
      if (!fmenu) {
        //Serial.println("in main menu");
        fmenu = true;
        mi = 0;
        encoder_previous_value = mi;
        encoder_value = mi;
        DrawMenu();
      }
      else {
        //Serial.println("out main menu");
        fmenu = false;
        fparam = false;
        EEPROM.put(0, evolParams);
        encoder_previous_value = evolParams.Vol;
        encoder_value = evolParams.Vol;
        out_volume(hpLastState);
      }            
      break;      
  }
}

void rotatySpeed(){ // acceleration for rotary )
  unsigned long millisAfterLastMotion = millis() - lastMovementAt;
  if (millisAfterLastMotion < accelerationLongCutoffMillis) {
    if (millisAfterLastMotion < accelerationShortCutffMillis) {
			millisAfterLastMotion = accelerationShortCutffMillis; // limit to maximum acceleration
		}
		if (currentDirection > 0) {
      encoder_value += rotaryAccelerationCoef / millisAfterLastMotion;
		}
		else {
		  encoder_value -= rotaryAccelerationCoef / millisAfterLastMotion;
		}
    encoder_value = max(min(encoder_value, POSITIVE_ENCODER_LIMIT), NEGATIVE_ENCODER_LIMIT);
  } 
}

void rotaryStep(){
  if (currentDirection > 0) {
    delta = (evolParams.evolMenu[MenuLength-1].value-1);
  }
  else {
    delta = (evolParams.evolMenu[MenuLength-1].value-1) * -1;
  }
  encoder_value = max(min(encoder_value+delta, POSITIVE_ENCODER_LIMIT), NEGATIVE_ENCODER_LIMIT);  
}
