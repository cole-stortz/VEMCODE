#ifndef VB_SERVO_H
#define VB_SERVO_H
class Servo {
public:
    void attach(int pin) { pin_ = pin; }
    void write(int angle) { if (pin_ >= 0) { angle_ = angle; api->analogWrite(pin_, angle * 255 / 180); } }
    int read() const { return angle_; }
    bool attached() const { return pin_ >= 0; }
    void detach() { pin_ = -1; }
private:
    int pin_ = -1;
    int angle_ = 0;
};
#endif
