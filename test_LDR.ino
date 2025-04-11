#define LDR 35

void setup() {
	Serial.begin(9600);
	pinMode(LDR, INPUT);
}

void loop() {
	Serial.println(analogRead(LDR));
	delay(100);
}
