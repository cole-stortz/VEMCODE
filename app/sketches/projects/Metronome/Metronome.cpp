//IMPORTANT NOTE: This code only reflects the Tinkercad simulator setup. If using the Arduino Modulino components (buzzer and knob) listed in the components section of this project page, code for those pieces would look slightly different. Adjust as needed 

#include <LiquidCrystal.h>

//pin variables
const int SPEAKER = 13;
const int YELLOW_LED = 12;
const int BLUE_LED1 = 11;
const int BLUE_LED2  = 10;
const int BLUE_LED3 = 9;
const int DIAL = A0;
//lcd pin variables
const int RS = 2;
const int E = 4;
const int DB4 = 5;
const int DB5 = 6;
const int DB6 = 7;
const int DB7 = 8;

//instantiate lcd object
LiquidCrystal lcd(RS, E, DB4, DB5, DB6, DB7);

//for counting beats in the time signature
int beatCount = 1;

void setup()
{
  //set input/output pins
  pinMode(SPEAKER, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(BLUE_LED1, OUTPUT);
  pinMode(BLUE_LED2, OUTPUT);
  pinMode(BLUE_LED3, OUTPUT);
  pinMode(DIAL, INPUT);
  
  //declare rows and columns of lcd
  lcd.begin(16, 2);
}

void loop()
{
  //map the dial's value to the beats per minute range of 50 to 300
  int bpm = map(analogRead(DIAL),0, 1023, 50, 300);
  
  //lcd print
  lcd.setCursor(0,0);
  lcd.print("Time Sig: 4/4");
  lcd.setCursor(0, 1);
  lcd.print("BPM: ");
  lcd.setCursor(5, 1);
  lcd.print(bpm);
  //clear third digit on display if bpm is two digits
  if(bpm < 100) {
    lcd.setCursor(7, 1);
    lcd.print("  ");
  }
 
  //set time of each individual beat based on bpm
  //60000 = 1 minute
  int beatInterval = 60000 / bpm;
  
  //in 4/4 time signature there are four beats
  //first beat of each 4 beats will be a different tone
  //and different color light to indicate start of new measure
  if(beatCount == 1) {
    //higher note at start of measure
    //tone plays for half the length of the beat to leave a pause
    tone(SPEAKER, 1025, beatInterval / 2);
    //turn on and off appropraite lights
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(BLUE_LED1, LOW);
    digitalWrite(BLUE_LED2, LOW);
    digitalWrite(BLUE_LED3, LOW);
    //keep respective light on for whole beat
    delay(beatInterval);
  } else {
    //lower tone for the rest of the measure
    tone(SPEAKER, 775, beatInterval / 2);
    //tone is the same for beats 2-4
    //but will have different lights on
    if(beatCount == 2) {
      digitalWrite(BLUE_LED1, HIGH);
      digitalWrite(YELLOW_LED, LOW);
      digitalWrite(BLUE_LED2, LOW);
      digitalWrite(BLUE_LED3, LOW);
      delay(beatInterval);
    } else if(beatCount == 3) {
      digitalWrite(BLUE_LED2, HIGH);
      digitalWrite(YELLOW_LED, LOW);
      digitalWrite(BLUE_LED1, LOW);
      digitalWrite(BLUE_LED3, LOW);
      delay(beatInterval);
    } else {
      digitalWrite(BLUE_LED3, HIGH);
      digitalWrite(YELLOW_LED, LOW);
      digitalWrite(BLUE_LED1, LOW);
      digitalWrite(BLUE_LED2, LOW);
      delay(beatInterval);
    }
  }
  
  //incriment beat count or reset beat count after 4
  if(beatCount < 4)
    beatCount++;
  else
    beatCount = 1;
  
}