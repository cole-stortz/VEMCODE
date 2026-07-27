// DHT11/DHT22 temperature + humidity sensor -- type the temp/humidity values
// into the two boxes on the canvas component and confirm they show up here.
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  dht.begin();
  Serial.println("DHT ready -- edit the temp/humidity fields on the canvas");
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  Serial.print("Temp: ");
  Serial.print(t);
  Serial.print(" C  Humidity: ");
  Serial.print(h);
  Serial.println(" %");
  delay(1000);
}
