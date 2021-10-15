int sensorValue;
float voltage;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); // Initialize digital pin LED_BUILTIN as an output.
  pinMode(A0, INPUT); // Declare Arduino pin as an input.
}

void loop() {
  sensorValue = analogRead(A0); // Reading status of Arduino analog pin
  voltage = sensorValue * (5.0 / 1023.0);
  
  if(voltage > 1.6) {
	// Frequency swipe when triggered from the SensorTile.box
	for (int i = 4000; i <= 24000; i = i+400) {
	  tone(8, i);
	  delay(5);
	}
    noTone(8);
  }
}
