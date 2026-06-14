/*
  ******************************************************************************
  * File Name     : Lambo_LineFollow.ino
  * Discription   : This is the TURNING line following algorithm for the robot, Lambo Rambo. 
  * Contributors  : Cole Stortz (main), Duncan Wood (second), Yannis Cassis, Kian Roseborough, Ala Assaf
  * Start Date    : 4/8/2024 
  * Last Modified : 6/5/2024

  * Comments:
  * - Parts; 3 motors, Two color sensors, two 3d printed hexagons, teensy 4.1 microcontroller
  * - Will follow a black line and if it reaches a blue line it will flip 180
  ******************************************************************************
  * @attention
  *
  * Copyright (c) LAMBO TEAM
  *
  ******************************************************************************
*/

// @board Teensy 4.1
#include <Servo.h>
Servo Arm;
Servo Gripper;

// Pin connections for Motor A
#define MOTOR1_PWM 6
#define MOTOR1_CWISE 7
#define MOTOR1_ANTI_CWISE 8

// Pin connections for Motor B
#define MOTOR2_PWM 0
#define MOTOR2_CWISE 1
#define MOTOR2_ANTI_CWISE 2

// Pin connections for Motor C
#define MOTOR3_PWM 3
#define MOTOR3_CWISE 4
#define MOTOR3_ANTI_CWISE 5

// Motor Speed (PWM) value out of 255
int PWM_VAL = 100;
#define PWM_VAL_MOTOR3 120
#define PWM_VAL_MOTOR2 100

// Define the colors
#define BLACK 5
#define WHITE 1
#define YELLOW 6
#define GREEN 4
#define BLUE 3
#define RED 2

// Define direction values
#define STOP     1
#define FORWARD  2
#define BACKWARD 3
#define RIGHT    4 
#define LEFT     5
#define FORWARDRIGHT 6
#define FORWARDLEFT  7
#define BACKWARDRIGHT 8
#define BACKWARDLEFT  9
#define TURNLEFT      10
#define TURNRIGHT     11
#define TURNLEFTREVERSE 12
#define TURNRIGHTREVERSE  13 
#define ROTATE          14
int CurrentDirection;

// Define box sizes
#define SMALL 0;
#define BIG   1;


// TCS3200 color sensor pin assignments
// Define the digital pins the sensors are connected to on the Teensy
const int numSensors = 4; // Number of color sensors
const int S0[numSensors] = {33, 31, 14, 19}; // S0 pins for each sensor
const int S1[numSensors] = {34, 32, 15, 20}; // S1 pins for each sensor
const int S2[numSensors] = {35, 40, 16, 21}; // S2 pins for each sensor
const int S3[numSensors] = {36, 41, 17, 22}; // S3 pins for each sensor
const int sensorOut[numSensors] = {37, 13, 18, 23}; // Sensor output pins for each sensor

// Calibrate the color sensors using white and black paper. Take the values reported when white paper is plac\ed under the sensor to be the minimum and values reported when black paper is placed under to be the maximum
int redMin[numSensors] = {0, 0, 0, 0};
int redMax[numSensors] = {800, 800, 800, 800};
int greenMin[numSensors] = {0, 0, 0, 0};
int greenMax[numSensors] = {800, 800, 800, 800};
int blueMin[numSensors] = {0, 0, 0, 0};
int blueMax[numSensors] = {800, 800, 800, 800};


const int trigPin1 = 9; // Trigger pin for the first sensor
const int echoPin1 = 10; // Echo pin for the first sensor
const int trigPin2 = 11; // Trigger pin for the second sensor
const int echoPin2 = 12; // Echo pin for the second sensor
const int armPin = 25; // Pin for the arm servo
const int gripperPin = 24; // Pin for the gripper servo
const int forceSensorPin = 27; // Force sensor pin
const int serialBaudRate = 115200; // Baud rate for serial communication
const int delayDuration = 10; // Delay in milliseconds
const int minDistance = 9; // Distance at which gripper starts closing
const int minGripperPosition = 30; // Minimum gripper position

// variables for the ultrasonic sensor
long duration;
int distance;

const int maxGripperPosition = 110; // Maximum gripper position
const int forceThreshold = 600; // Force threshold

int gripperPosition = maxGripperPosition; // Initialize gripper position
int forceValue;
int IRSensor = 30;


// Variables for the 
int BOXCOLOR;
int BOXSIZE;
int box_detected = 0;
int arm_pos = 35;
int robot_case = 0;
int LINE_COLOR=BLUE;


// Define the variables for the line following algorithm
int colorsensorValues[numSensors] = {0};


// Function prototypes
void setup();
void loop();
void stopMotors();
void moveMotorsForward();
void moveMotorsBackward();
void moveMotorsRight();
void moveMotorsLeft();
void moveMotorsForwardRight();
void moveMotorsForwardLeft();
void moveMotorsBackwardRight();
void moveMotorsBackwardLeft();
void moveMotorsTurnLeft(); 
void moveMotorsTurnRight();
void moveTest();
void ultrasonicRead();
void readColorSensors();
unsigned long readColorFrequency(int sensorIndex, int s2, int s3);
int mapValue(int x, int in_min, int in_max, int out_min, int out_max);
void followLine();
float getDistance1();
float getDistance2();
void closeGripper();
void displayForceSensorReading();
void findLine();
void pickUpBox();
void placeBox();
void followLineReverse();

void setup() {
  Serial.begin(57600); // initialize serial connection for debugging

  // Initialize the arm and gripper pins
  Arm.attach(armPin);
  Gripper.attach(gripperPin);
  pinMode(trigPin1, OUTPUT);
  pinMode(echoPin1, INPUT);
  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);
  pinMode(forceSensorPin, INPUT);
  Serial.begin(serialBaudRate);

  pinMode(IRSensor, INPUT);

  // Initialize gripper at max position
  Gripper.write(gripperPosition);

  // Set all motor controller pins as outputs
  pinMode(MOTOR1_PWM, OUTPUT);
  pinMode(MOTOR2_PWM, OUTPUT);
  pinMode(MOTOR3_PWM, OUTPUT);
  pinMode(MOTOR1_CWISE, OUTPUT);
  pinMode(MOTOR1_ANTI_CWISE, OUTPUT);
  pinMode(MOTOR2_CWISE, OUTPUT);
  pinMode(MOTOR2_ANTI_CWISE, OUTPUT);
  pinMode(MOTOR3_CWISE, OUTPUT);
  pinMode(MOTOR3_ANTI_CWISE, OUTPUT);

  

  // Initial state - Turn off all the motors
  stopMotors();

  // Set up pins for each color sensor
  for (int i = 0; i < numSensors; i++) {
    pinMode(S0[i], OUTPUT);
    pinMode(S1[i], OUTPUT);
    pinMode(S2[i], OUTPUT);
    pinMode(S3[i], OUTPUT);
    pinMode(sensorOut[i], INPUT);
    
    // Set S0 and S1 for frequency scaling to 20%
    digitalWrite(S0[i], HIGH); // S0=1
    digitalWrite(S1[i], LOW);  // S1=0, for 20% frequency scaling
  }

  //MoveStart();
}


// Main loop for the robot algorithm
// Each case is a different phase of the robot algorithm, in total there are 4 phases...
//    - Case 1: Pick up the box, determine which color and size, then move to the appropriate line
//    - Case 2: Follow the line that is the same color as the box it picked up, look for the fork in the road
//    - Case 3: Once the fork is reached, analize what size the box is and move left if big, right if small.
//              Then move forward for a bit then lower the arm to put down the box, reverse the previous steps 
//              to go back to the start.
//    - Case 4: After you put down the box, follow the line back to the start, but this time stop the robot 
//              when it reaches the initial green line. This ends the loop.
// RIGHT CLOCK AND GO TO DEFINITION TO READ EACH ONE AND WHAT IT DOES.
void loop() {
  Serial.print("--- Case: ");
  Serial.println(robot_case);
  if(robot_case == 0){
    pickUpBox();
  } else if(robot_case == 1){
    PWM_VAL = 100;
    readColorSensors();
    ultrasonicRead();
    followLine();
    delay(10);
  } else if(robot_case == 2){
    readColorSensors();
    ultrasonicRead();
    placeBox();
  } else if(robot_case == 3){
    readColorSensors();
    ultrasonicRead();
    followLineReverse();
    delay(10);
  }
  
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////CASE DEFINITIONS///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// This is the first case function
// It will first lower the arm into position, then go forward untill it sees the platform.
// If it sees the platform it begins to close the claw, it will close untill a reading on the force sensor.
// This gives us the different size boxes depending on how long it took to close. To read the color of the box
// there is IR sensor that is calebrated to see the difference in red and blue. If the sensor is activated it
// sees red, if not it sees blue. After it picks up the box it will go left if red, right if blue. In end it will
// switch cases to break out of this function.
void pickUpBox(){
  // Read distance from ultrasonic sensor
  float distance = getDistance1(); // Get the distance from the correct ultrasonic sensor

  forceValue = analogRead(forceSensorPin); // read the force value of the force sensor
  Serial.print("Force Sensor Value: ");
  Serial.println(forceValue);
  int sensorStatus = digitalRead(IRSensor); // read the IR sensor value
  if (sensorStatus == 1) { // if the sensor is not activated set to blue, else go red
    BOXCOLOR = BLUE;
  } else {
    BOXCOLOR = RED;
  }
  Serial.print("Distance: ");
  Serial.println(distance);
  Serial.print("Force: ");
  Serial.println(forceValue);
  Serial.print("IR: ");
  Serial.println(sensorStatus);
  Serial.print("BoxColor: ");
  Serial.println(BOXCOLOR == BLUE ? "BLUE" : "RED");
  Serial.print("BoxDetected: ");
  Serial.println(box_detected);

  // Check if the distance is 7.5 cm or less to start closing the gripper
  if (distance <= minDistance) {
    stopMotors();
    closeGripper();
  } else {
    if(box_detected == 1){
      stopMotors();
      for(int i = 35; i<101; i++){
        Arm.write(i);
        delay(50);
      }
      LINE_COLOR = BOXCOLOR;
      Serial.print("Box grabbed! LineColor: ");
      Serial.println(LINE_COLOR == RED ? "RED -> going LEFT" : "BLUE -> going RIGHT");
      if(LINE_COLOR == RED){
        // Go Left
        analogWrite(MOTOR1_PWM, PWM_VAL);
        analogWrite(MOTOR2_PWM, PWM_VAL*1.9);
        analogWrite(MOTOR3_PWM, PWM_VAL);
        digitalWrite(MOTOR1_CWISE, LOW);
        digitalWrite(MOTOR1_ANTI_CWISE, HIGH);
        digitalWrite(MOTOR2_CWISE, HIGH);
        digitalWrite(MOTOR2_ANTI_CWISE, LOW);
        digitalWrite(MOTOR3_CWISE, LOW);
        digitalWrite(MOTOR3_ANTI_CWISE, HIGH);
        delay(2200);
        stopMotors();
        delay(1000);
        moveMotorsForward();
        delay(1000);

      } else if(LINE_COLOR == BLUE){
        // Go Right
        analogWrite(MOTOR1_PWM, PWM_VAL);
        analogWrite(MOTOR2_PWM, PWM_VAL*1.9);
        analogWrite(MOTOR3_PWM, PWM_VAL);
        digitalWrite(MOTOR1_CWISE, HIGH);
        digitalWrite(MOTOR1_ANTI_CWISE, LOW);
        digitalWrite(MOTOR2_CWISE, LOW);
        digitalWrite(MOTOR2_ANTI_CWISE, HIGH);
        digitalWrite(MOTOR3_CWISE, HIGH);
        digitalWrite(MOTOR3_ANTI_CWISE, LOW);
        delay(2200);
        stopMotors();
        delay(1000);
        moveMotorsForward();
        delay(1000);
      }
      robot_case = 1;
    } else {
      moveMotorsBackward(); // Go to the platfor if distance is greater than min distance
      Gripper.write(100); // Gripper open
      Arm.write(35); // Arm up
    }
  }
  delay(100); // Adjust delay as needed
}

void closeGripper() {
  // Close the gripper until it reaches the minimum position or force threshold is exceeded
  if (gripperPosition > minGripperPosition && forceValue < forceThreshold) {
    gripperPosition--;
    Gripper.write(gripperPosition);
    Serial.print("Gripper: ");
    Serial.print(gripperPosition);
    Serial.print(" | Force: ");
    Serial.println(forceValue);
    if (gripperPosition < 75) {
      BOXSIZE = 0;
    } else {
      BOXSIZE = 1;
    }
  } else {
    Serial.print("BOX GRABBED - size: ");
    Serial.println(BOXSIZE == 0 ? "SMALL" : "BIG");
    box_detected = 1;
    moveMotorsForward();
  }
}


// Line following algorithm (virtually the same as follow line reverse)
// - If the front sensor sees the line color, go forward
// - If either of the side sensors see the line color and the other sensors see black, turn in the 
//   direction of the side sensor. 
// - Contunue to turn until the front sensor sees the line color again to go forward. This centers 
//   the robot on the line much more accuratly
// - If both of the side sensors see the line color at the same time, switch cases to the place function.
void followLine() {
  Serial.print("followLine sensors [0,2,3]: ");
  Serial.print(colorsensorValues[0]);
  Serial.print(",");
  Serial.print(colorsensorValues[2]);
  Serial.print(",");
  Serial.println(colorsensorValues[3]);
  if (colorsensorValues[2] == LINE_COLOR && colorsensorValues[3] == BLACK && colorsensorValues[0] == BLACK) {
    Serial.println("Action: TurnLeft");
    moveMotorsTurnLeft();
  } else if (colorsensorValues[0] == LINE_COLOR && colorsensorValues[3] == BLACK && colorsensorValues[2] == BLACK) {
    Serial.println("Action: TurnRight");
    moveMotorsTurnRight();
  } else if (colorsensorValues[3] == LINE_COLOR) {
    Serial.println("Action: Forward");
    moveMotorsForward();
  } else if (colorsensorValues[2] == LINE_COLOR && colorsensorValues[0] == LINE_COLOR) {
    Serial.println("Action: Fork reached -> case 2");
    robot_case = 2;
  }
}
// Same algorithm to follow the line, but instead of reading for both side sensors reading the line color 
// at the same time, read if either of them read green. If either sensor 0 or 2 read green, stop for 100s
// to end the code and complete the task.
void followLineReverse() {
  Serial.print("followLineReverse sensors [0,2,3]: ");
  Serial.print(colorsensorValues[0]);
  Serial.print(",");
  Serial.print(colorsensorValues[2]);
  Serial.print(",");
  Serial.println(colorsensorValues[3]);
  if (colorsensorValues[2] == LINE_COLOR && colorsensorValues[3] == BLACK && colorsensorValues[0] == BLACK) {
    Serial.println("Action: TurnLeft");
    moveMotorsTurnLeft();
  } else if (colorsensorValues[0] == LINE_COLOR && colorsensorValues[3] == BLACK && colorsensorValues[2] == BLACK) {
    Serial.println("Action: TurnRight");
    moveMotorsTurnRight();
  } else if (colorsensorValues[3] == LINE_COLOR) {
    Serial.println("Action: Forward");
    moveMotorsForward();
  } else if (colorsensorValues[2] == GREEN || colorsensorValues[0] == GREEN) {
    Serial.println("GREEN line reached - DONE");
    stopMotors();
    delay(100000);
  }
}

// This is the place box funtion.
// This function is done with finite movements and ment to only run once. It will read the box size and if the box
// size is small it will go to the right fork, if it is big it will go left. Then it will go forward for a definite
// amount of time then move the arm down, place the box, turn around, then reverse the initial direction to go to 
// the starting of the fork. Once completed it will activate the last case of the robot and follow the line reverse.
void placeBox(){
  float distance2 = getDistance2();
  Serial.print("placeBox - BOXSIZE: ");
  Serial.println(BOXSIZE == 1 ? "BIG" : "SMALL");
  if (BOXSIZE == 1){
    Serial.println("Big box -> going LEFT");
    moveMotorsLeft();
    delay(650);
    moveMotorsForward();
    delay(2000);
    stopMotors();
    Serial.println("Lowering arm");
    for(int i=100; i<151; i++){
      Arm.write(i);
      delay(50);
    }
    delay(500);
    Serial.println("Opening gripper");
    Gripper.write(100);
    delay(500);
    Arm.write(100);
    Serial.println("Spinning 180");
    moveMotors180();
    moveMotorsForward();
    delay(2000);
    moveMotorsLeft();
    delay(700);
    stopMotors();
    delay(200);
    moveMotorsForward();
    delay(1000);
    Serial.println("placeBox done -> case 3");
    robot_case = 3;
  }
  if (BOXSIZE == 0){
    Serial.println("Small box -> going RIGHT");
    moveMotorsRight();
    delay(650);
    moveMotorsForward();
    delay(2000);
    stopMotors();
    Serial.println("Lowering arm");
    for(int i=100; i<151; i++){
      Arm.write(i);
      delay(50);
    }
    delay(500);
    Serial.println("Opening gripper");
    Gripper.write(100);
    delay(500);
    Arm.write(100);
    Serial.println("Spinning 180");
    moveMotors180();
    moveMotorsForward();
    delay(2000);
    moveMotorsRight();
    delay(700);
    stopMotors();
    delay(200);
    moveMotorsForward();
    delay(1000);
    Serial.println("placeBox done -> case 3");
    robot_case = 3;
  }
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////// MOTOR DEFINITION//////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void stopMotors() {
  // Stop all motors
  CurrentDirection = STOP;
  analogWrite(MOTOR2_PWM, PWM_VAL);
  digitalWrite(MOTOR1_CWISE, LOW);
  digitalWrite(MOTOR1_ANTI_CWISE, LOW);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, LOW);
  digitalWrite(MOTOR3_CWISE, LOW);
  digitalWrite(MOTOR3_ANTI_CWISE, LOW);
}

void moveMotorsForward() {
  CurrentDirection = FORWARD;
  Serial.println("Forward");
    // Set motor speeds
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL);
  analogWrite(MOTOR3_PWM, PWM_VAL+12);
  digitalWrite(MOTOR1_CWISE, LOW);
  digitalWrite(MOTOR1_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, LOW);
  digitalWrite(MOTOR3_CWISE, HIGH);
  digitalWrite(MOTOR3_ANTI_CWISE, LOW);
} 

void moveMotorsBackward() {
  CurrentDirection = BACKWARD;
  Serial.println("Backward");
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL);
  analogWrite(MOTOR3_PWM, PWM_VAL+12);
  digitalWrite(MOTOR1_CWISE, HIGH);
  digitalWrite(MOTOR1_ANTI_CWISE, LOW);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, LOW);
  digitalWrite(MOTOR3_CWISE, LOW);
  digitalWrite(MOTOR3_ANTI_CWISE, HIGH);
}

void moveMotorsRight() {
    CurrentDirection=RIGHT;
    // Set motor speeds
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL*1.9);
  analogWrite(MOTOR3_PWM, PWM_VAL);
  digitalWrite(MOTOR1_CWISE, HIGH);
  digitalWrite(MOTOR1_ANTI_CWISE, LOW);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR3_CWISE, HIGH);
  digitalWrite(MOTOR3_ANTI_CWISE, LOW);
}

void moveMotorsLeft() {
  CurrentDirection=LEFT;
  Serial.println("Left");
    // Set motor speeds
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL*1.9);
  analogWrite(MOTOR3_PWM, PWM_VAL);
  digitalWrite(MOTOR1_CWISE, LOW);
  digitalWrite(MOTOR1_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR2_CWISE, HIGH);
  digitalWrite(MOTOR2_ANTI_CWISE, LOW);
  digitalWrite(MOTOR3_CWISE, LOW);
  digitalWrite(MOTOR3_ANTI_CWISE, HIGH);
}

void moveMotorsForwardRight() {
  CurrentDirection=FORWARDRIGHT;
  Serial.println("Forward Right");
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL);
  analogWrite(MOTOR3_PWM, PWM_VAL+10);
  digitalWrite(MOTOR1_CWISE, LOW);
  digitalWrite(MOTOR1_ANTI_CWISE, LOW);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR3_CWISE, HIGH);
  digitalWrite(MOTOR3_ANTI_CWISE, LOW);
}

void moveMotorsForwardLeft() {
  CurrentDirection=FORWARDLEFT;
  Serial.println("Forward Left");
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL);
  analogWrite(MOTOR3_PWM, PWM_VAL+10);
  digitalWrite(MOTOR1_CWISE, LOW);
  digitalWrite(MOTOR1_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR2_CWISE, HIGH);
  digitalWrite(MOTOR2_ANTI_CWISE, LOW);
  digitalWrite(MOTOR3_CWISE, LOW);
  digitalWrite(MOTOR3_ANTI_CWISE, LOW);
}

void moveMotorsBackwardRight() {
  CurrentDirection=BACKWARDRIGHT;
  Serial.println("Backward Right");
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL);
  analogWrite(MOTOR3_PWM, PWM_VAL-5);
  digitalWrite(MOTOR1_CWISE, LOW);
  digitalWrite(MOTOR1_ANTI_CWISE, LOW);
  digitalWrite(MOTOR2_CWISE, HIGH);
  digitalWrite(MOTOR2_ANTI_CWISE, LOW);
  digitalWrite(MOTOR3_CWISE, LOW);
  digitalWrite(MOTOR3_ANTI_CWISE, HIGH);
}

void moveMotorsBackwardLeft() {
  CurrentDirection=BACKWARDLEFT;
  Serial.println("Backward Left");
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL);
  analogWrite(MOTOR3_PWM, PWM_VAL-5);
  digitalWrite(MOTOR1_CWISE, HIGH);
  digitalWrite(MOTOR1_ANTI_CWISE, LOW);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR3_CWISE, LOW);
  digitalWrite(MOTOR3_ANTI_CWISE, LOW);
}

void moveMotorsTurnLeft() {
  CurrentDirection=TURNLEFT;
  Serial.println("Turn Left");
  analogWrite(MOTOR1_PWM, PWM_VAL*0.75);
  analogWrite(MOTOR2_PWM, PWM_VAL*0.75);
  analogWrite(MOTOR3_PWM, PWM_VAL*0.75);
  digitalWrite(MOTOR1_CWISE, LOW);
  digitalWrite(MOTOR1_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, LOW);
  digitalWrite(MOTOR3_CWISE, LOW);
  digitalWrite(MOTOR3_ANTI_CWISE, HIGH);
}

void moveMotorsTurnRight() {
  CurrentDirection=TURNRIGHT;
  analogWrite(MOTOR1_PWM, PWM_VAL*0.75);
  analogWrite(MOTOR2_PWM, PWM_VAL*0.75);
  analogWrite(MOTOR3_PWM, PWM_VAL*0.75);
  Serial.println("Turn Right");
  digitalWrite(MOTOR1_CWISE, HIGH);
  digitalWrite(MOTOR1_ANTI_CWISE, LOW);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, LOW);
  digitalWrite(MOTOR3_CWISE, HIGH);
  digitalWrite(MOTOR3_ANTI_CWISE, LOW);
}
void moveMotorsTurnLeftreverse() {
  CurrentDirection=TURNLEFTREVERSE;
  Serial.println("Turn Left");
  int adjustedSpeed = PWM_VAL; // Decrease the speed
  analogWrite(MOTOR2_PWM, adjustedSpeed); // Double the speed of motor 2 and adjust the other motors to the adjustedSpeed
  analogWrite(MOTOR1_PWM, adjustedSpeed * 1.25);
  analogWrite(MOTOR3_PWM, adjustedSpeed);
  digitalWrite(MOTOR1_CWISE, HIGH);
  digitalWrite(MOTOR1_ANTI_CWISE, LOW);
  digitalWrite(MOTOR2_CWISE, HIGH);
  digitalWrite(MOTOR2_ANTI_CWISE, LOW);
  digitalWrite(MOTOR3_CWISE, HIGH);
  digitalWrite(MOTOR3_ANTI_CWISE, LOW);
}

void moveMotorsTurnRightreverse() {
  CurrentDirection=TURNRIGHTREVERSE;
  Serial.println("Turn Right");
  int adjustedSpeed = PWM_VAL; // Decrease the speed
  analogWrite(MOTOR2_PWM, adjustedSpeed); // Double the speed of motor 2 and adjust the other motors to the adjustedSpeed
  analogWrite(MOTOR1_PWM, adjustedSpeed);
  analogWrite(MOTOR3_PWM, adjustedSpeed * 1.25);
  digitalWrite(MOTOR1_CWISE, LOW);
  digitalWrite(MOTOR1_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR3_CWISE, LOW);
  digitalWrite(MOTOR3_ANTI_CWISE, HIGH);
}
void moveMotorsLeftCircle() {
  Serial.println("spinning");
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL);
  analogWrite(MOTOR3_PWM, PWM_VAL);
  digitalWrite(MOTOR1_CWISE, LOW);
  digitalWrite(MOTOR1_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR3_CWISE, LOW);
  digitalWrite(MOTOR3_ANTI_CWISE, LOW);
  delay(750);
  moveMotorsForward();
  delay(500);
}
void moveMotors180() {
  CurrentDirection = ROTATE;
  Serial.println("spinning");
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL);
  analogWrite(MOTOR3_PWM, PWM_VAL);
  digitalWrite(MOTOR1_CWISE, LOW);
  digitalWrite(MOTOR1_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR3_CWISE, LOW);
  digitalWrite(MOTOR3_ANTI_CWISE, HIGH);
  delay(1800);
  stopMotors();
  delay(500);
}
void moveMotors180Left() {
  CurrentDirection = ROTATE;
  Serial.println("spinning");
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL);
  analogWrite(MOTOR3_PWM, PWM_VAL);
  digitalWrite(MOTOR1_CWISE, LOW);
  digitalWrite(MOTOR1_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR2_CWISE, LOW);
  digitalWrite(MOTOR2_ANTI_CWISE, HIGH);
  digitalWrite(MOTOR3_CWISE, LOW);
  digitalWrite(MOTOR3_ANTI_CWISE, HIGH);
  delay(1500);
  moveMotorsForward();
  delay(500);
  
}
void moveMotors180Right() {
  CurrentDirection = ROTATE;
  Serial.println("spinning");
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL);
  analogWrite(MOTOR3_PWM, PWM_VAL);
  digitalWrite(MOTOR1_CWISE, HIGH);
  digitalWrite(MOTOR1_ANTI_CWISE, LOW);
  digitalWrite(MOTOR2_CWISE, HIGH);
  digitalWrite(MOTOR2_ANTI_CWISE, LOW);
  digitalWrite(MOTOR3_CWISE, HIGH);
  digitalWrite(MOTOR3_ANTI_CWISE, LOW);
}

void moveTest() {
  delay(1000);
  analogWrite(MOTOR1_PWM, PWM_VAL);
  analogWrite(MOTOR2_PWM, PWM_VAL);
  analogWrite(MOTOR3_PWM, PWM_VAL);
  moveMotorsForward();
  delay(200);
  moveMotorsBackward();
  delay(400);
  moveMotorsForward();
  delay(200);
  moveMotorsLeft();
  delay(200);
  moveMotorsRight();
  delay(400);
  moveMotorsLeft();
  delay(200);
}
void defaultMode(){
  if (CurrentDirection==STOP){
    stopMotors();
  }
  if (CurrentDirection==FORWARD){
    moveMotorsForward();
    delay(50);
  }
  if (CurrentDirection==BACKWARD){
    moveMotorsBackward();
    delay(50);
  }
  if (CurrentDirection==RIGHT){
    moveMotorsRight();
    delay(50);
  }
  if (CurrentDirection==LEFT){
    moveMotorsLeft();
    delay(50);
  }
  if (CurrentDirection==FORWARDRIGHT){
    moveMotorsForwardRight();
    delay(50);
  }
  if (CurrentDirection==FORWARDLEFT){
    moveMotorsForwardLeft();
    delay(50);
  }
  if (CurrentDirection==BACKWARDRIGHT){
    moveMotorsBackwardRight();
    delay(50);
  }
  if (CurrentDirection==BACKWARDLEFT){
    moveMotorsBackwardLeft();
    delay(50);
  }
  if (CurrentDirection==TURNLEFT){
    moveMotorsTurnLeft();
    delay(50);
  }
  if (CurrentDirection==TURNRIGHT){
    moveMotorsTurnRight();
    delay(50);
  }
  if (CurrentDirection==TURNLEFTREVERSE){
    moveMotorsTurnLeftreverse();
    delay(50);
  }
  if (CurrentDirection==TURNRIGHTREVERSE){
    moveMotorsTurnRightreverse();
    delay(50);
  }
  if (CurrentDirection==ROTATE){
    moveMotors180();
  }
  
}
void oppositeMode(){
  if (CurrentDirection==STOP){
    stopMotors();
  }
  if (CurrentDirection==FORWARD){
    moveMotorsBackward();
    delay(200);
  }
  if (CurrentDirection==BACKWARD){
    moveMotorsForward();
    delay(200);
  }
  if (CurrentDirection==RIGHT){
    moveMotorsLeft();
    delay(200);
  }
  if (CurrentDirection==LEFT){
    moveMotorsRight();
    delay(200);
  }
  if (CurrentDirection==FORWARDRIGHT){
    moveMotorsForwardLeft();
    delay(200);
  }
  if (CurrentDirection==FORWARDLEFT){
    moveMotorsForwardRight();
    delay(200);
  }
  if (CurrentDirection==BACKWARDRIGHT){
    moveMotorsBackwardLeft();
    delay(200);
  }
  if (CurrentDirection==BACKWARDLEFT){
    moveMotorsBackwardRight();
    delay(200);
  }
  if (CurrentDirection==TURNLEFT){
    moveMotorsTurnRight();
    delay(200);
  }
  if (CurrentDirection==TURNRIGHT){
    moveMotorsTurnLeft();
    delay(200);
  }
  if (CurrentDirection==TURNLEFTREVERSE){
    moveMotorsTurnRightreverse();
    delay(200);
  }
  if (CurrentDirection==TURNRIGHTREVERSE){
    moveMotorsTurnLeftreverse();
    delay(200);
  }
  if (CurrentDirection==ROTATE){
    moveMotors180();
  }
}
 


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////ULTRASONIC DEFINITION//////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ultrasonicRead() {
  digitalWrite(trigPin1, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin1, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin1, LOW);
  delayMicroseconds(10);

  // Reads the ECHO_PIN, returns the sound wave in microseconds
  duration = pulseIn(echoPin1, HIGH);
  // Calculating the distance
  distance = duration * 0.034 / 2; // Speed of sound wave divided by 2 (send & receive)
  Serial.print(distance);
  Serial.println("cm");
}

float getDistance1() {
  // Trigger ultrasonic sensor
  digitalWrite(trigPin1, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin1, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin1, LOW);

  // Measure the echo time to calculate distance
  float duration = pulseIn(echoPin1, HIGH);
  float distance = duration * 0.034 / 2; // Calculate distance in cm
  //Serial.print("Distance: ");
  //Serial.println(distance);
  return distance;
}

float getDistance2() {
  // Trigger ultrasonic sensor
  digitalWrite(trigPin2, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin2, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin2, LOW);

  // Measure the echo time to calculate distance
  float duration = pulseIn(echoPin2, HIGH);
  float distance = duration * 0.034 / 2; // Calculate distance in cm
  //Serial.print("Distance: ");
  //Serial.println(distance);
  return distance;
}

 

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////COLOR SENSOR DEFINITION////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void readColorSensors(){
  // Labels based on followLine() logic: [0]=Right, [1]=Rear, [2]=Left, [3]=Front
  const char* sensorLabels[4] = {"[0] Right", "[1] Rear", "[2] Left", "[3] Front"};

  for (int i = 0; i < numSensors; i++) {
    unsigned long redFrequency, greenFrequency, blueFrequency;

    // Read frequencies for each sensor
    redFrequency = readColorFrequency(i, LOW, LOW);
    blueFrequency = readColorFrequency(i, LOW, HIGH);
    greenFrequency = readColorFrequency(i, HIGH, HIGH);

    // Map the raw frequencies to the calibrated range
    int mappedRed = mapValue(redFrequency, redMin[i], redMax[i], 0, 255);
    int mappedGreen = mapValue(greenFrequency, greenMin[i], greenMax[i], 0, 255);
    int mappedBlue = mapValue(blueFrequency, blueMin[i], blueMax[i], 0, 255);

    // Debuging: print the unmapped values
    Serial.print("Sensor ");
    Serial.print(sensorLabels[i]);
    Serial.print(" - Red: ");
    Serial.print(redFrequency);
    Serial.print(" | Green: ");
    Serial.print(blueFrequency);
    Serial.print(" | Blue: ");
    Serial.println(greenFrequency);

    // Debugging: print the mapped values
    Serial.print("Sensor ");
    Serial.print(sensorLabels[i]);
    Serial.print(" - Red: ");
    Serial.print(mappedRed);
    Serial.print(" | Green: ");
    Serial.print(mappedGreen);
    Serial.print(" | Blue: ");
    Serial.println(mappedBlue);

    
    // Determine the color based on the mapped values
   if (mappedRed > 100 && mappedGreen > 100 && mappedBlue > 100){
      colorsensorValues[i] = BLACK;
      Serial.println("Color Est: Black");
    } else if (mappedRed < mappedBlue && mappedRed < mappedGreen && mappedBlue>90) {
      colorsensorValues[i] = RED;
      Serial.println("Color Est: Red");
    } else if (mappedBlue < mappedRed && mappedBlue < mappedGreen && mappedBlue < 200) {
      colorsensorValues[i] = BLUE;
      Serial.println("Color Est: Blue");
    } else if (mappedGreen < mappedRed && mappedGreen < mappedBlue) {
      colorsensorValues[i] = GREEN;
      Serial.println("Color Est: Green");
    } else {
      Serial.println("Color not recognized");
      colorsensorValues[i] = 0;
    }
    Serial.println(' ');
  }
}

unsigned long readColorFrequency(int sensorIndex, int s2, int s3) {
  // Set S pins for the specific sensor
  digitalWrite(S2[sensorIndex], s2);
  digitalWrite(S3[sensorIndex], s3);
  // Read the output frequency from the color sensor using pulseIn with a timeout
  return pulseIn(sensorOut[sensorIndex], LOW, 1000000);
}

int mapValue(int x, int in_min, int in_max, int out_min, int out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}