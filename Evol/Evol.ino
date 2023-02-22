
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "BfButton.h"
#include <EEPROM.h>
#include <Bounce2.h>

// Software SPI (slower updates, more flexible pin options):
// pin 15 - Serial clock out (SCLK)
// pin 66 - Serial data out (DIN)
// pin 18 - Data/Command select (D/C)
// pin 19 - LCD chip select (CS)
// pin 21 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(15, 16, 18, 19, 21);

#define HP 20 //пустой сейчай и низачто не в ответе

#define btnPin 5 //GPIO #3-Push button on encoder
//#define DT 8 //GPIO #4-DT on encoder (Output B)
//#define CLK 7 //GPIO #5-CLK on encoder (Output A)
#define ENCODER_CLK_PIN 2  //Nano has interrupt on pin 2
#define ENCODER_DT_PIN 3   // Can be any other digital pin
#define ENCODER_RANGE 255
#define ENCODER_OFF_STATE 2

//encoder state vars
volatile int encoderValue = 0;
volatile int aPrevious = ENCODER_OFF_STATE;
volatile int bPrevious = ENCODER_OFF_STATE;
volatile int aCurrVal;
volatile bool fdir;
volatile int STEPS = 0;

volatile bool fmenu = false;
volatile bool fmenu_i = false;
volatile bool fparam = false;
volatile bool fHP = false;
volatile bool fSUB = false;
volatile bool fm2 = false;

char *menu[] = {"TW L","TW R","MID L","MID R","SUB L","SUB R", "HP L","HP R", "STEP"};
int offset[11] = {0,0,0,0,0,0,0,0,0,0,92}; //9-sub; 10-vol; 8-step;
int null_offset[11];

volatile int mi = 0;
volatile int li = 0;
volatile int pi = 0;
volatile int si = 0;
volatile int counter = 0;
volatile int hpLastState;
volatile int currentStateHP;
volatile int pShift;

unsigned long lastButtonPress = 0;  

//Объект под гарнитуру, будем слушать что кому то захотелось чота куда то вставить или переключить )
Bounce debouncer = Bounce();

BfButton btn(BfButton::STANDALONE_DIGITAL, btnPin, true, LOW);

//Button press hanlding function
void pressHandler (BfButton *btn, BfButton::press_pattern_t pattern) {
  switch (pattern) {
    case BfButton::SINGLE_PRESS:
      //Serial.println("Single push");
      if (fmenu && !fparam) {
        //Serial.println("in params");
        fparam = true;
        pi = offset[mi];
        encoderValue = pi;
      }
      else if (fmenu && fparam) {
        //Serial.println("out params");
        fparam = false;
        EEPROM.put(0, offset);
        encoderValue = mi;
      }
      else if (!fSUB){
        //Serial.println("go sub");
        fSUB = true;
        encoderValue = offset[9];
        out_sub();
      }
      else if (fSUB){
        //Serial.println("out sub");
        EEPROM.put(0, offset);
        fSUB = false;
        encoderValue = counter;
        out_volume(hpLastState);
      }
      break;
      
    case BfButton::DOUBLE_PRESS:
      //Serial.println("Double push");
      //Serial.println("Need mute on/off");
      break;
      
    case BfButton::LONG_PRESS:    
      //Serial.println("Long push");
      if (!fmenu) {
        //Serial.println("in main menu");
        fmenu = true;
        encoderValue = mi;
        main_menu(mi);
      }
      else {
        EEPROM.put(0, offset);
        fmenu = false;
        fparam = false;
        display.clearDisplay();
        display.display();
        encoderValue = counter;
        out_volume(hpLastState);
      }      
      break;      
  }
}

void readEncoder() {
  int a = digitalRead(ENCODER_CLK_PIN);
  int b = digitalRead(ENCODER_DT_PIN);
  if (a != aPrevious) {                  
    aPrevious = a;
    if (bPrevious == ENCODER_OFF_STATE) {
      bPrevious = b;
      return;
    }
    if (b != bPrevious) {
      bPrevious = b;
      processEncoderRotation(a == b);
      bPrevious = ENCODER_OFF_STATE;
    }
  }
}  

void processEncoderRotation (bool direction) {
  //encoderValue = max(min((encoderValue + (direction ? 1 : -1)), ENCODER_RANGE), 0);
  encoderValue = encoderValue + (direction ? 1 : -1);
  if (direction == true) {
    fdir =  true;
  }
  else {
    fdir =  false;
  }
}

void setup()   {
  Serial.begin(9600);
  display.begin();
  display.setContrast(50);
  display.setTextSize(1);
  display.clearDisplay();
  display.display();

  //pinMode(CLK,INPUT_PULLUP);
  //pinMode(DT,INPUT_PULLUP);
  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), readEncoder, CHANGE);
  
  pinMode(btnPin,INPUT_PULLUP);
   
  //тек состояние гарнитуры 
  hpLastState = digitalRead(HP);
  
  //Button settings
  btn.onPress(pressHandler)
  .onDoublePress(pressHandler) // default timeout
  .onPressFor(pressHandler, 1000); // custom timeout for 1 second

  // Даем бибилотеке знать, к какому пину мы подключили кнопку
  debouncer.attach(HP);
  debouncer.interval(50); // Интервал, в течение которого мы не буем получать значения с пина

  //get def_volume
  EEPROM.get(0, null_offset);
  if (memcmp (null_offset, offset, 11) == 0) {
    //Serial.println("Области памяти идентичные.");
    EEPROM.get(0, offset);    
  }
  else {
    //Serial.println("Области памяти неидентичные.");
    EEPROM.put(0, offset);
    EEPROM.get(0, offset);
  }

  //чо у нас там за громкость то?
  counter = offset[10];
  STEPS = offset[8];
  out_volume(hpLastState);
  encoderValue = counter;
  aCurrVal = counter;

}

void loop() {
  //Крутим вертим всем чем хотим
  //проверяем положение ручки энкодера

  if (encoderValue != aCurrVal) {
    aCurrVal = encoderValue;
    if (fdir) {
      if (fmenu && !fparam){
        if (encoderValue > 8) {encoderValue=8;}
        mi = encoderValue;
        if (mi > 4 && fm2 == false){
          fm2 = true;
          main_menu(mi);
        }
        go_menu(mi, true);
      }
      else if (fmenu && fparam) {
        pi = encoderValue;        
        set_param(mi, pi, pShift);
      }
      else if (fSUB) {
        si = encoderValue; 
        out_sub();
      }
      else {
        counter = encoderValue+STEPS;
        out_volume(hpLastState);    
      } 
    }
    else {
      if (fmenu && !fparam){
        if (encoderValue < 0) {encoderValue=0;}
        mi = encoderValue;
        if (mi < 5 && fm2 == true) {
          fm2 = false;
          main_menu(mi);
        }
        go_menu(mi, false);
      }
      else if (fmenu && fparam) {
        pi = encoderValue;
        //весело шагаем по просторам volume
        if (mi == 8 && pi < 0) { pi = 0; }
        set_param(mi, pi, pShift);
      }
      else if (fSUB) {
        si = encoderValue;
        out_sub();
      }      
      else {
        counter = encoderValue-STEPS;
        out_volume(hpLastState);    
      }
    }    
  }
  
  //Опрашиваем кнопку для хожденя по мукам
  btn.read();

  //где то тут мы проверили что кто то вставил наушники и нарисовали ему на экран 
  bool currentStateHP = digitalRead(HP);
  if (currentStateHP != hpLastState && currentStateHP == 1){
    hpLastState = currentStateHP; 
    out_volume(currentStateHP);
  }
  //Это состояние переключателя который рисовал Мишка для ушей
  debouncer.update();
  // Получаем значение кнопки
  int value = debouncer.read();
  // Теперь мы точно знаем, в каком состоянии находится наша кнопка
  if ( value == LOW ) {
    //Serial.println("HP OFF");
    fHP = false;
  }
  else {    
    //Serial.println("HP ON");
    fHP = true;
  } 
}

void out_volume(int pos)
{
  offset[10] = counter;
  display.setCursor(0,25);
  display.setTextColor(BLACK);
  display.fillRect(0, 25, display.width(), 10, 0);
  if (pos == LOW) {
    display.print("Dyn Vol: "); 
  }
  else {
    display.print("HP Vol: ");
  }
  display.print(String(count_db(),1));
  display.display();  
}

void out_sub()
{
  offset[9] = si;
  display.setCursor(0, 25);
  display.setTextColor(1);
  display.fillRect(0,25,display.width(),10,0);
  display.print("SUB: ");
  display.print(String(si/(float)2,1));
  display.display();
}

void main_menu(int mi){
  pShift = 0;
  int k;
  if (mi < 5) {
    pShift = 0;
    k = 5;
  }
  else {
    pShift = 5;
    k = 4;
  }
  display.clearDisplay();
  //Миша захотел две страницы
  //for (int i = 0; i < 8; i++){
  for (int i = 0 + pShift; i < k + pShift; i++){
    display.setCursor(0, 0+((i-pShift)*10));
    if (mi == i ) {
      display.setTextColor(0,1);
    }
    else {
      display.setTextColor(1,0);
    }
    display.print(String(menu[i])+":  ");
    display.setCursor(40, 0+((i-pShift)*10));
    if (i == 8 ) {
      display.println(String(offset[i]+1));
    }
    else {
      display.println(String(offset[i]/(float)2,1));      
    }    
    display.display(); 
  }
}

void go_menu(int m, bool mf){
  if (m < 5) {
    pShift = 0;
  }
  else {
    pShift = 5;
  }
  int p = m;
  if (mf == true) {
    p--;
    drawLine(p,pShift,1,0);
    drawLine(mi,pShift,0,1);
  }
  else {
    if (m != 4) {
      p++;
      drawLine(p,pShift,1,0);
    }
    drawLine(mi,pShift,0,1); 
  }
}

void drawLine(int pos, int shift, int text, int font) {
  display.fillRect(0,((pos-shift)*10),display.width(),10,0);
  display.setCursor(0,((pos-shift)*10));
  display.setTextColor(text,font);
  display.print(String(menu[pos])+":  ");
  display.setCursor(40,((pos-shift)*10));
  display.setTextColor(text,font);
  if (pos == 8 ) {
    //шаг для volume захотелось
    display.println(String(offset[pos]+1)); 
  }
  else{
    display.println(String(offset[pos]/(float)2,1)); 
  }
  display.display();
  delay(30);
}

void set_param(int li, int pi, int shift){
  offset[li] = pi;
  display.setCursor(40, ((li-shift)*10)); 
  display.fillRect(40, ((li-shift)*10), display.width(), 10, 0);
  if (li == 8 ) {
    //шаг для volume захотелось
    display.print(String(pi+1));
    STEPS = pi;
  }
  else {
    display.print(String(pi/(float)2,1));
  }
  display.display();
}

//get dB +31.5/-95.5
float count_db(){
  float db = 31.5 - (0.5*(255-counter));
  return db;
}
