#include <LedControl.h>

LedControl lc = LedControl(11, 13, 10, 3);

#define BTN_ROTATE 3   // D2 = spin
#define BTN_DOWN 2     // D3 = drop
#define BTN_RIGHT 5    // D5 = rechts
#define BTN_LEFT 4     // D6 = links


#define WIDTH 8
#define HEIGHT 24

bool field[HEIGHT][WIDTH];
bool frame[HEIGHT][WIDTH];
bool lastFrame[HEIGHT][WIDTH];

bool gameOver = false;
int score = 0;

unsigned long lastFall = 0;
unsigned long lastMove = 0;
unsigned long lastRotate = 0;

bool lastLeft = HIGH;
bool lastRight = HIGH;
bool lastRotateBtn = HIGH;
bool lastDown = HIGH;

int fallSpeed = 500;
int moveDelay = 120;
int rotateDelay = 150;

const unsigned long modeHoldTime = 800;
const unsigned long modeDebounce = 300;
unsigned long modePressStart = 0;
unsigned long lastModeSwitch = 0;
bool modeTriggered = false;

int gameMode = 0; // 0 = Tetris, 1 = Dino

const byte pieces[7][4][4] = {
  { {0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0} },
  { {0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0} },
  { {0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0} },
  { {0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0} },
  { {1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0} },
  { {0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0} },
  { {1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0} }
};

byte piece[4][4];
int currentPiece;
int nextPiece;
int currentX = 2;
int currentY = 0;

// Dino
int dinoY = 19;
int dinoJumpTicks = 0;
bool dinoJumping = false;
int obstacleY = 0;
int obstacleX = 6;
unsigned long lastDinoMove = 0;
unsigned long dinoMoveDelay = 220;

void spawnPiece();
void resetTetris();
void resetDino();
void switchMode(int newMode);
void handleModeButton();
void resetButtonStates();
void handleInput();
void renderFrame();
void drawGameOver();
void drawDino();
void drawDinoGameOver();
void updateDino();
void clearFrame();
void drawDisplay();
bool isValid(int newX, int newY);
void placePiece();
void clearLines();
void rotatePiece();

void clearFrame() {
  memset(frame, 0, sizeof(frame));
}

void resetButtonStates() {
  lastLeft = digitalRead(BTN_LEFT);
  lastRight = digitalRead(BTN_RIGHT);
  lastRotateBtn = digitalRead(BTN_ROTATE);
  lastDown = digitalRead(BTN_DOWN);
}

void resetTetris() {
  memset(field, 0, sizeof(field));
  score = 0;
  gameOver = false;
  nextPiece = random(0, 7);
  spawnPiece();
  lastFall = millis();
  lastMove = millis();
  lastRotate = millis();
  resetButtonStates();
}

void resetDino() {
  memset(field, 0, sizeof(field));
  clearFrame();
  score = 0;
  gameOver = false;

  dinoY = 19;
  dinoJumpTicks = 0;
  dinoJumping = false;
  obstacleY = 0;
  obstacleX = 6;
  lastDinoMove = millis();

  resetButtonStates();
}

void switchMode(int newMode) {
  gameMode = newMode;
  modeTriggered = false;
  modePressStart = 0;
  lastModeSwitch = millis();

  if (gameMode == 0) {
    resetTetris();
  } else {
    resetAvoider();   // statt resetDino()
  }
}

void setup() {
  randomSeed(analogRead(A0));

  for (int i = 0; i < 3; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, 8);
    lc.clearDisplay(i);
  }

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_ROTATE, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);


  memset(lastFrame, 0, sizeof(lastFrame));
  switchMode(0);
}

void drawDisplay() {
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      if (frame[y][x] != lastFrame[y][x]) {
        int rotatedY = HEIGHT - 1 - y;
        int rotatedX = WIDTH - 1 - x;

        int module = rotatedY / 8;
        int col = rotatedY % 8;
        int row = rotatedX;

        lc.setLed(2 - module, row, col, frame[y][x]);
      }
    }
  }
  memcpy(lastFrame, frame, sizeof(frame));
}

bool isValid(int newX, int newY) {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (piece[py][px]) {
        int fx = newX + px;
        int fy = newY + py;
        if (fx < 0 || fx >= WIDTH || fy >= HEIGHT) return false;
        if (fy >= 0 && field[fy][fx]) return false;
      }
    }
  }
  return true;
}

void placePiece() {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (piece[py][px]) {
        int fx = currentX + px;
        int fy = currentY + py;
        if (fy >= 0 && fy < HEIGHT && fx >= 0 && fx < WIDTH) field[fy][fx] = true;
      }
    }
  }
}

void clearLines() {
  for (int y = HEIGHT - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < WIDTH; x++) {
      if (!field[y][x]) {
        full = false;
        break;
      }
    }

    if (full) {
      score++;
      for (int ty = y; ty > 0; ty--) {
        for (int x = 0; x < WIDTH; x++) {
          field[ty][x] = field[ty - 1][x];
        }
      }
      memset(field[0], 0, WIDTH);
      y++;
    }
  }
}

void rotatePiece() {
  byte temp[4][4];
  byte backup[4][4];
  memcpy(backup, piece, sizeof(piece));

  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      temp[x][3 - y] = piece[y][x];
    }
  }

  memcpy(piece, temp, sizeof(piece));

  if (!isValid(currentX, currentY)) {
    if (isValid(currentX - 1, currentY)) currentX--;
    else if (isValid(currentX + 1, currentY)) currentX++;
    else memcpy(piece, backup, sizeof(piece));
  }
}

void spawnPiece() {
  currentPiece = nextPiece;
  nextPiece = random(0, 7);

  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      piece[y][x] = pieces[currentPiece][y][x];
    }
  }

  currentX = 2;
  currentY = 0;

  if (!isValid(currentX, currentY)) gameOver = true;
}

void handleInput() {
  bool left = digitalRead(BTN_LEFT);
  bool right = digitalRead(BTN_RIGHT);
  bool rotateBtn = digitalRead(BTN_ROTATE);
  bool down = digitalRead(BTN_DOWN);

  if (left == LOW && lastLeft == HIGH && millis() - lastMove > moveDelay) {
    if (isValid(currentX - 1, currentY)) currentX--;
    lastMove = millis();
  }

  if (right == LOW && lastRight == HIGH && millis() - lastMove > moveDelay) {
    if (isValid(currentX + 1, currentY)) currentX++;
    lastMove = millis();
  }

  if (rotateBtn == LOW && lastRotateBtn == HIGH && millis() - lastRotate > rotateDelay) {
    rotatePiece();
    lastRotate = millis();
  }

  if (down == LOW && lastDown == HIGH) {
    while (isValid(currentX, currentY + 1)) {
      currentY++;
      score += 2;
    }
  }

  lastLeft = left;
  lastRight = right;
  lastRotateBtn = rotateBtn;
  lastDown = down;
}

void renderFrame() {
  clearFrame();

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      if (field[y][x]) frame[y][x] = true;
    }
  }

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (piece[py][px]) {
        int x = currentX + px;
        int y = currentY + py;
        if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH) frame[y][x] = true;
      }
    }
  }
}

void drawGameOver() {
  clearFrame();

  int leftX = 0;
  int rightX = 4;

auto px = [&](int y, int x, bool val) {
  int mirroredX = WIDTH - 1 - x;   // horizontal zurückspiegeln
  if (y >= 0 && y < HEIGHT && mirroredX >= 0 && mirroredX < WIDTH) {
    frame[y][mirroredX] = val;
  }
};

  int y = 0;

  px(y, leftX + 1, 1); px(y, leftX + 2, 1);
  px(y + 1, leftX, 1);
  px(y + 2, leftX, 1); px(y + 2, leftX + 2, 1); px(y + 2, leftX + 3, 1);
  px(y + 3, leftX, 1); px(y + 3, leftX + 3, 1);
  px(y + 4, leftX + 1, 1); px(y + 4, leftX + 2, 1); px(y + 4, leftX + 3, 1);

  px(y, rightX + 1, 1); px(y, rightX + 2, 1);
  px(y + 1, rightX, 1); px(y + 1, rightX + 3, 1);
  px(y + 2, rightX, 1); px(y + 2, rightX + 1, 1); px(y + 2, rightX + 2, 1); px(y + 2, rightX + 3, 1);
  px(y + 3, rightX, 1); px(y + 3, rightX + 3, 1);
  px(y + 4, rightX, 1); px(y + 4, rightX + 3, 1);

  y = 6;

  px(y, leftX, 1); px(y, leftX + 3, 1);
  px(y + 1, leftX, 1); px(y + 1, leftX + 1, 1); px(y + 1, leftX + 2, 1); px(y + 1, leftX + 3, 1);
  px(y + 2, leftX, 1); px(y + 2, leftX + 3, 1);
  px(y + 3, leftX, 1); px(y + 3, leftX + 3, 1);
  px(y + 4, leftX, 1); px(y + 4, leftX + 3, 1);

  px(y, rightX, 1); px(y, rightX + 1, 1); px(y, rightX + 2, 1); px(y, rightX + 3, 1);
  px(y + 1, rightX, 1);
  px(y + 2, rightX, 1); px(y + 2, rightX + 1, 1); px(y + 2, rightX + 2, 1);
  px(y + 3, rightX, 1);
  px(y + 4, rightX, 1); px(y + 4, rightX + 1, 1); px(y + 4, rightX + 2, 1); px(y + 4, rightX + 3, 1);

  y = 13;

  px(y, leftX + 1, 1); px(y, leftX + 2, 1);
  px(y + 1, leftX, 1); px(y + 1, leftX + 3, 1);
  px(y + 2, leftX, 1); px(y + 2, leftX + 3, 1);
  px(y + 3, leftX, 1); px(y + 3, leftX + 3, 1);
  px(y + 4, leftX + 1, 1); px(y + 4, leftX + 2, 1);

  px(y, rightX, 1); px(y, rightX + 3, 1);
  px(y + 1, rightX, 1); px(y + 1, rightX + 3, 1);
  px(y + 2, rightX + 1, 1); px(y + 2, rightX + 2, 1);
  px(y + 3, rightX + 1, 1); px(y + 3, rightX + 2, 1);
  px(y + 4, rightX + 1, 1);

  y = 19;

  px(y, leftX, 1); px(y, leftX + 1, 1); px(y, leftX + 2, 1); px(y, leftX + 3, 1);
  px(y + 1, leftX, 1);
  px(y + 2, leftX, 1); px(y + 2, leftX + 1, 1); px(y + 2, leftX + 2, 1);
  px(y + 3, leftX, 1);
  px(y + 4, leftX, 1); px(y + 4, leftX + 1, 1); px(y + 4, leftX + 2, 1); px(y + 4, leftX + 3, 1);

  px(y, rightX, 1); px(y, rightX + 1, 1); px(y, rightX + 2, 1);
  px(y + 1, rightX, 1); px(y + 1, rightX + 3, 1);
  px(y + 2, rightX, 1); px(y + 2, rightX + 1, 1); px(y + 2, rightX + 2, 1);
  px(y + 3, rightX, 1); px(y + 3, rightX + 2, 1);
  px(y + 4, rightX, 1); px(y + 4, rightX + 3, 1);

  drawDisplay();
}

// === AVOIDER-MODUS: Hindernisse fallen von oben, Spieler auf Zeile 23 ===

int avoiderX = 3;

struct Obstacle {
  int x;
  int y;
  bool active;
} obstacles[20];

unsigned long lastObstacleTime = 0;
unsigned long obstacleInterval = 420;
unsigned long obstacleFallDelay = 90;
unsigned long lastObstacleFall = 0;
unsigned long avoiderStartTime = 0;

void resetAvoider() {
  memset(field, 0, sizeof(field));
  clearFrame();
  score = 0;
  gameOver = false;

  avoiderX = 3;

  for (int i = 0; i < 20; i++) {
    obstacles[i].active = false;
    obstacles[i].x = 0;
    obstacles[i].y = 0;
  }

  lastObstacleTime = millis();
  lastObstacleFall = millis();
  avoiderStartTime = millis();
  obstacleInterval = 420;
  obstacleFallDelay = 90;

  resetButtonStates();
}

void drawAvoider() {
  clearFrame();

  frame[23][avoiderX] = true;

  for (int i = 0; i < 16; i++) {
    if (obstacles[i].active && obstacles[i].y >= 0 && obstacles[i].y < HEIGHT) {
      frame[obstacles[i].y][obstacles[i].x] = true;
    }
  }

  drawDisplay();
}

void updateAvoider() {
  bool left = digitalRead(BTN_LEFT);
  bool right = digitalRead(BTN_RIGHT);

  if (left == LOW && lastLeft == HIGH && millis() - lastMove > moveDelay) {
    if (avoiderX > 0) avoiderX--;
    lastMove = millis();
  }

  if (right == LOW && lastRight == HIGH && millis() - lastMove > moveDelay) {
    if (avoiderX < WIDTH - 1) avoiderX++;
    lastMove = millis();
  }

  unsigned long survived = millis() - avoiderStartTime;

  obstacleInterval = 420;
  obstacleFallDelay = 90;

  if (survived > 4000) {
    obstacleInterval = 360;
    obstacleFallDelay = 80;
  }
  if (survived > 8000) {
    obstacleInterval = 320;
    obstacleFallDelay = 72;
  }
  if (survived > 12000) {
    obstacleInterval = 280;
    obstacleFallDelay = 64;
  }
  if (survived > 16000) {
    obstacleInterval = 240;
    obstacleFallDelay = 58;
  }
  if (survived > 22000) {
    obstacleInterval = 210;
    obstacleFallDelay = 52;
  }

  if (millis() - lastObstacleTime >= obstacleInterval) {
    lastObstacleTime = millis();

    int spawnCount = 1;

    if (random(0, 100) < 45) spawnCount = 2;
    if (random(0, 100) < 18) spawnCount = 3;

    for (int s = 0; s < spawnCount; s++) {
      for (int i = 0; i < 20; i++) {
        if (!obstacles[i].active) {
          obstacles[i].active = true;
          obstacles[i].x = random(0, WIDTH);
          obstacles[i].y = 0;
          break;
        }
      }
    }
  }

  if (millis() - lastObstacleFall >= obstacleFallDelay) {
    lastObstacleFall = millis();

    for (int i = 0; i < 20; i++) {
      if (!obstacles[i].active) continue;

      obstacles[i].y++;

      if (obstacles[i].y >= 23) {
        if (obstacles[i].x == avoiderX) {
          gameOver = true;
        } else {
          score++;
        }
        obstacles[i].active = false;
      }
    }
  }

  lastLeft = left;
  lastRight = right;
}

void drawAvoiderGameOver() {
  clearFrame();
  int cx = 4, cy = 16;
  frame[cy-1][cx-1] = true;
  frame[cy-1][cx+1] = true;
  frame[cy][cx] = true;
  frame[cy+1][cx-1] = true;
  frame[cy+1][cx+1] = true;
  drawDisplay();
}



void loop() {
  if (gameOver) {
    drawGameOver();

    bool left = digitalRead(BTN_LEFT);
    bool right = digitalRead(BTN_RIGHT);

    if (right == LOW && lastRight == HIGH) {
      switchMode(0);   // Tetris
      return;
    }

    if (left == LOW && lastLeft == HIGH) {
      switchMode(1);   // Avoider
      return;
    }

    lastLeft = left;
    lastRight = right;
    return;
  }

  if (gameMode == 0) {
    handleInput();

    if (millis() - lastFall > fallSpeed) {
      if (isValid(currentX, currentY + 1)) currentY++;
      else {
        placePiece();
        clearLines();
        spawnPiece();
      }
      lastFall = millis();
    }

    renderFrame();
    drawDisplay();
  } else {
    updateAvoider();
    drawAvoider();
  }
}