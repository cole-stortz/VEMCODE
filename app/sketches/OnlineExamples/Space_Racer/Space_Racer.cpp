#include <LiquidCrystal.h>

// Constants
const int R_BUTTON = 3;
const int L_BUTTON = 2;
const int SPEAKER = 4;

const int RS = 8;
const int E = 9;
const int DB4 = 10;
const int DB5 = 11;
const int DB6 = 12;
const int DB7 = 13;

LiquidCrystal lcd(RS, E, DB4, DB5, DB6, DB7);

// Custom characters
byte ship[8] = {
  B00010,
  B00110,
  B01110,
  B11111,
  B01110,
  B00110,
  B00010,
  B00000
};

byte asteroid[8] = {
  B00110,
  B01111,
  B11101,
  B11011,
  B11111,
  B01110,
  B00100,
  B00000
};

byte flameA[8] = {
  B00000,
  B00001,
  B00011,
  B00111,
  B00011,
  B00001,
  B00000,
  B00000
};

byte flameB[8] = {
  B00000,
  B11000,
  B11100,
  B11110,
  B11100,
  B11000,
  B00000,
  B00000
};

// Ship row: 0 = left, 1 = right
int shipRow = 1;
int lastShipRow = 1;

// Asteroid
int asteroidCol = 0;
int asteroidRow = 0;

void setup()
{
  Serial.begin(9400);
  lcd.begin(16,2);

  pinMode(L_BUTTON, INPUT_PULLUP);
  pinMode(R_BUTTON, INPUT_PULLUP);

  lcd.createChar(0, ship);
  lcd.createChar(1, asteroid);
  lcd.createChar(2, flameA);
  lcd.createChar(3, flameB);

  lcd.clear();

  randomSeed(analogRead(A0));
}

void loop()
{
  // --- BUTTON INPUT ---
  int rState = digitalRead(R_BUTTON);
  int lState = digitalRead(L_BUTTON);

  if (rState == LOW) shipRow = 1;
  if (lState == LOW) shipRow = 0;

  // --- ASTEROID LOGIC (LEFT → RIGHT) ---

  // Respawn asteroid when it reaches ship area
  if (asteroidCol >= 13) {
    asteroidCol = 0;              
    asteroidRow = random(0, 2);   
  }

  // Clear previous asteroid position
  if (asteroidCol > 0) {
    lcd.setCursor(asteroidCol - 1, asteroidRow);
    lcd.write(' ');
  }

  // Draw asteroid
  lcd.setCursor(asteroidCol, asteroidRow);
  lcd.write(byte(1));

  // Move asteroid RIGHT
  asteroidCol++;

  // Collision detection
  if (asteroidCol == 13 && asteroidRow == shipRow) {
    tone(SPEAKER, 400, 200);

    
  }
  

  // --- SHIP DRAWING ---

  // Clear previous ship position (no wrap)
  lcd.setCursor(12, lastShipRow);
  lcd.write(' ');
  lcd.write(' ');
  lcd.write(' ');
   lcd.write(' ');

  

  // Draw ship
  lcd.setCursor(13, shipRow);
  lcd.write(byte(0));

  // Draw flame
  lcd.setCursor(14, shipRow);
  lcd.write(byte(2));
  lcd.setCursor(15, shipRow);
  lcd.write(byte(3));

  lastShipRow = shipRow;

  delay(80);
}