#include <Servo.h>
#include <LiquidCrystal.h>

// LCD pins: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);

Servo lidServo;

// PING))) ultrasonic sensors
const int handSensorPin = 12;   // front sensor for hand detection
const int fullSensorPin = 13;   // inside sensor for bin full detection

// Actuators
const int servoPin = A0;

const int redPin = 9;
const int greenPin = 11;
const int bluePin = 10;

// Distance settings in cm
const int handDistance = 50;  // hand detected if closer than 20 cm
const int fullDistance = 20;   // bin full if trash is closer than 8 cm

void setup() {
  lcd.begin(16, 2);

  lidServo.attach(servoPin);
  lidServo.write(0); // lid closed

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Trash Bin");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);
}

void loop() {
  int handDist = readPing(handSensorPin);
  int fullDist = readPing(fullSensorPin);
  Serial.print("Hand: ");
  Serial.println(handDist);
  Serial.print("full: ");
  Serial.println(fullDist);

  bool binFull = fullDist > 0 && fullDist <= fullDistance;
  bool handDetected = handDist > 0 && handDist <= handDistance;

  if (binFull) {
    setRGB(255, 0, 0); // red

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("BIN FULL");
    lcd.setCursor(0, 1);
    lcd.print("Please Empty");

    lidServo.write(0); // keep closed
  } 
  else {
    setRGB(0, 255, 0); // green

    if (handDetected) {
      lidServo.write(90);   // open lid

	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Lid Open");
	
	lcd.setCursor(0,1);
	lcd.print("Throw Trash");

	delay(3000);   // keep lid open for 3 sec

	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Remove Hand");

	lcd.setCursor(0,1);
	lcd.print("Closing Lid");
	
	delay(1500);   // let user read message

	lidServo.write(0);    // close lid

	delay(1000);
	lcd.clear();
      
    } 
    else {
      lcd.setCursor(0, 0);
      lcd.print("Smart Bin Ready ");
      lcd.setCursor(0, 1);
      lcd.print("Not Full        ");
    }
  }

  delay(300);
}

// Function for PING))) sensor
int readPing(int pin) {
  long duration;
  int distance;

  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(2);
  digitalWrite(pin, HIGH);
  delayMicroseconds(5);
  digitalWrite(pin, LOW);

  pinMode(pin, INPUT);
  duration = pulseIn(pin, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  distance = duration / 29 / 2;
  return distance;
}

// RGB LED control
void setRGB(int redValue, int greenValue, int blueValue) {
  analogWrite(redPin, redValue);
  analogWrite(greenPin, greenValue);
  analogWrite(bluePin, blueValue);
}