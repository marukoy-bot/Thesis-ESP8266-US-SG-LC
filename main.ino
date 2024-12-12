#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6yNIZeYyD"
#define BLYNK_TEMPLATE_NAME "Thesis"
#define BLYNK_AUTH_TOKEN "OVtAkBjO2ES1bBrmj56ooWVQkmlRTfwN"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <HX711.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "McDonalds Kiosk";
char pword[] = "00000000";

//US Sensor 1
// #define trig_1 D0
// #define echo_1 D1

//relay
#define relay D3

//US Sensor 2
#define trig_2 D1
#define echo_2 D2

//US Sensor 3
#define trig_3 D4
#define echo_3 D5

//Strain Gauge Sensor
#define strain A0

//Load Cell Sensor
#define load_sck D6
#define load_dout D7

//Load Cell Sensor
//change after re-calibrating!
#define calibration_reading -4279.9

BlynkTimer timer;
HX711 scale;

bool connected = false;

void setup() 
{
    Serial.begin(9600);
    // pinMode(trig_1, OUTPUT);
    // pinMode(echo_1, INPUT);
    pinMode(trig_2, OUTPUT);
    pinMode(echo_2, INPUT);
    pinMode(trig_3, OUTPUT);
    pinMode(echo_3, INPUT);
    pinMode(strain, INPUT);
    pinMode(relay, OUTPUT);
    Blynk.begin(auth, ssid, pword, "blynk.cloud", 80);
    scale.begin(load_dout, load_sck);
    scale.set_scale(calibration_reading);
    timer.setInterval(1000L, sendData); 
}

void sendData()
{
    //Blynk.virtualWrite(V0, getDistanceCm(trig_1, echo_1));
    Blynk.virtualWrite(V0, getDistanceCm(trig_2, echo_2));
    Blynk.virtualWrite(V1, getDistanceCm(trig_3, echo_3));
    String strainRead = (String)getStrainVal(strain) + " | " + (String)getStrainPercentage(strain) + "%";
    Blynk.virtualWrite(V3, strainRead);
    Blynk.virtualWrite(V4, getWeight());
    delay(1000);
}

BLYNK_WRITE(V2)
{
    digitalWrite(relay, !param.asInt());
}

float getDistanceCm(int trig, int echo)
{
    long duration = 0;
    float distance = 0;
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);

    duration = pulseIn(echo, HIGH);
    distance = duration * 0.017;
    return distance;
}

float getStrainVal(int bf350)
{
    return analogRead(bf350);
}

float getStrainPercentage(int bf350)    
{
    float value = getStrainVal(bf350);
    float percentage = map(value, 0, 1024, 0, 100);
    return percentage;
}

float getWeight()
{
    return scale.get_units(20);
}

void loop() 
{
    Blynk.run();
    timer.run();
}
