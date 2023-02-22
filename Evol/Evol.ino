
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
#define ENCODER_DT_PIN 7   // Can be any other digital pin
#define ENCODER_CLICKS_PER_ROTATION 36

//encoder section

//encoder state vars
volatile int encoderValue = 0;
volatile int aPrevious;
volatile int bPrevious;


/* Encoder interrupt routine
   reads and compares previous and current states
   execute call-back with bool value indicating CW / CCW
*/

void readEncoder() {
  int a = digitalRead(ENCODER_CLK_PIN);
  int b = digitalRead(ENCODER_DT_PIN);
  if (a != aPrevious) {                  
    aPrevious = a;
    if (b != bPrevious) {
      bPrevious = b;
      processEncoderRotation(a == b);
    }
  }
}

// Encoder rotation process routine
void processEncoderRotation (bool direction) {
  encoderValue = max(min((encoderValue + (direction ? 1 : -1)), ENCODER_CLICKS_PER_ROTATION), 0);
  Serial.println(encoderValue);
  if (direction == true) {
    Serial.println ("CW");
  }
  else {
    Serial.println("CCW");
  }
}
//end of encoder section

int STEPS = 0;
int POSMIN = 0;
int POSMAX = 255;
int lastPos, newPos;

bool fmenu = false;
bool fmenu_i = false;
bool fparam = false;
bool fHP = false;
bool fSUB = false;
bool fm2 = false;

char *menu[] = {"TW L","TW R","MID L","MID R","SUB L","SUB R", "HP L","HP R", "STEP"};
int offset[11] = {0,0,0,0,0,0,0,0,0,0,92}; //9-sub; 10-vol; 8-step;
int null_offset[11];

int mi = 0;
int li = 0;
int pi = 0;
int si = 0;
int counter = 0;
int hpLastState;
int currentStateHP;
int pShift;

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
        //не наебем не проживем
        POSMIN = -30;
        POSMAX = 30;
        lastPos = pi;      
        //encoder.setPosition(pi / STEPS); 
      }
      else if (fmenu && fparam) {
        //Serial.println("out params");
        fparam = false;
        EEPROM.put(0, offset);
        //не наебем не проживем
        POSMIN = 0;
        POSMAX = 8;
        lastPos = mi; 
      }
      else if (!fSUB){
        //Serial.println("go sub");
        //не наебем не проживем
        POSMIN = -20;
        POSMAX = 20;
        lastPos = offset[9];
        //encoder.setPosition(lastPos / STEPS);
        out_sub();
        fSUB = true;
      }
      else if (fSUB){
        EEPROM.put(0, offset);
        POSMIN = 1;
        POSMAX = 255; 
        lastPos = counter;      
        out_volume(hpLastState);
        fSUB = false;
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
        main_menu(mi);
        fmenu = true;
        //не наебем не проживем
        POSMIN = 0;
        POSMAX = 8;
        lastPos = mi;

      }
      else {
        fmenu = false;
        fparam = false;
        EEPROM.put(0, offset);
        display.clearDisplay();
        display.display();
        //не наебем не проживем
        POSMIN = 1;
        POSMAX = 255;
        //encoder.setPosition(counter / STEPS);
        lastPos = counter;        
        out_volume(hpLastState);
      }      
      break;      
  }
}

void setup()   {
  Serial.begin(9600);
  display.begin();
  display.setContrast(50);
  display.setTextSize(1);
  display.clearDisplay();
  display.display();
  pinMode(btnPin,INPUT_PULLUP);
  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), readEncoder, CHANGE);
  
   
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
  
  //дефолт для управления крутилкой


}

void loop() {

  //encoder state now is accessible via encoderValue


  //probably this code need to be refactored after that
  
  if (lastPos != newPos && !fmenu && !fparam && !fSUB) {
    int l = newPos;
    if (newPos < lastPos) {  
      newPos -= STEPS;
      l -= STEPS;
    }
    else {
      newPos += STEPS;
      l += STEPS;
    }
  }
  // если положение изменилось - выводим на монитор
  if (lastPos != newPos) {
    //Serial.println(newPos);
    if (newPos < lastPos) {
      if (fmenu && !fparam){
        mi --;
        lastPos = mi;
        if (mi < 5 && fm2 == true) {
          fm2 = false;
          main_menu(mi);
        }
        go_menu(mi, false);
      }
      else if (fmenu && fparam) {
        pi --;
        //весело шагаем по просторам volume
        if (mi == 8 && pi < 0) { pi = 0; }
        lastPos = pi;
        set_param(mi, pi, pShift);
      }
      else if (fSUB) {
        si --;
        lastPos = si;
        out_sub();
      }      
      else {
        counter = newPos;
        out_volume(hpLastState);    
        lastPos = counter;
      }
    }
    else {
      if (fmenu && !fparam){
        mi ++;
        lastPos = mi;
        if (mi > 4 && fm2 == false){
          fm2 = true;
          main_menu(mi);
        }
        go_menu(mi, true);
      }
      else if (fmenu && fparam) {
        pi ++;
        lastPos = pi;
        set_param(mi, pi, pShift);
      }
      else if (fSUB) {
        si ++;
        lastPos = si;
        out_sub();
      }
      else {
        counter = newPos;
        lastPos = counter;
        out_volume(hpLastState);    
      }      
    }
  }
  //Опрашиваем кнопку для хожденя по мукам
  btn.read();

  //где то тут мы проверили что кто то вставил наушники и нарисовали ему на экран 
  bool currentStateHP = digitalRead(HP);
  if (currentStateHP != hpLastState && currentStateHP == 1){
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
  //текущее состояние гарнитуры
  hpLastState = currentStateHP; 
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
  //Serial.println("vol c: "+String(counter));
  return db;
}
