
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "BfButton.h"
#include <EEPROM.h>
#include <Bounce2.h>
#include <Encoder.h>
#include "RotaryEncoder.h"          // библиотека для энкодера
#include <Servo.h>          // библиотека для сервопривода
Servo servo;

// Software SPI (slower updates, more flexible pin options):
// pin 15 - Serial clock out (SCLK)
// pin 66 - Serial data out (DIN)
// pin 18 - Data/Command select (D/C)
// pin 19 - LCD chip select (CS)
// pin 21 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(15, 16, 18, 19, 21);

#define HP 20 //пустой сейчай и низачто не в ответе

#define btnPin 5 //GPIO #3-Push button on encoder
#define DT 8 //GPIO #4-DT on encoder (Output B)
#define CLK 7 //GPIO #5-CLK on encoder (Output A)

//задаем шаг энкодера, макс./мин. значение поворота
RotaryEncoder encoder(7, 8);  // пины подключение энкодера (CLK,DT)
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
        encoder.setPosition(pi);
      }
      else if (fmenu && fparam) {
        //Serial.println("out params");
        fparam = false;
        EEPROM.put(0, offset);
        //не наебем не проживем
        POSMIN = 0;
        POSMAX = 8;
        lastPos = mi; 
        //encoder.setPosition(mi / STEPS); 
        encoder.setPosition(lastPos);
      }
      else if (!fSUB){
        //Serial.println("go sub");
        //не наебем не проживем
        POSMIN = -20;
        POSMAX = 20;
        lastPos = offset[9];
        //encoder.setPosition(lastPos / STEPS);
        encoder.setPosition(lastPos);
        out_sub();
        fSUB = true;
      }
      else if (fSUB){
        EEPROM.put(0, offset);
        POSMIN = 1;
        POSMAX = 255; 
        lastPos = counter;      
        //encoder.setPosition(counter / STEPS);
        encoder.setPosition(counter);
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
        //encoder.setPosition(mi / STEPS);
        encoder.setPosition(lastPos);
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
        encoder.setPosition(lastPos);
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

  pinMode(CLK,INPUT_PULLUP);
  pinMode(DT,INPUT_PULLUP);
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
  
  //дефолт для управления крутилкой
  servo.attach(11);    // пин для подключения серво
  encoder.setPosition(counter); //ну и выствим тек значение громкости

}

void loop() {
  //Крутим вертим всем чем хотим
  //проверяем положение ручки энкодера
  encoder.tick();
  newPos = encoder.getPosition();
  if (newPos < POSMIN) { 
    encoder.setPosition(POSMIN); 
    newPos = POSMIN;
  }
  else if (newPos > POSMAX) { 
    encoder.setPosition(POSMAX); 
    newPos = POSMAX; 
  }
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
    encoder.setPosition(newPos);
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
