#include <WiFi.h>
#include <WebServer.h>

#include <HX711.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <ESP32Servo.h>

const char* ssid = "ESP32_Hotspot";
const char* password = "12345678";

WebServer server(80);

//Laser Sensor
#define ldr 35

//servo
#define servoPin 25

//Load Cell Sensor
#define load_sck 19
#define load_dout 18

//Load Cell Sensor
#define calibration_reading -4279.9
#define SAMPLING_TIME 1000

int dropletCount;
float dropletsPerMinute = 0.0;
float weight = 0.0;
int servoAngle = 0;

#define BUFFER_SIZE 10
float weighted_avg = 0.4;

float avg_heartRate = 0;
float avg_spO2 = 0;

uint32_t lastBeatDetected = 0;
uint32_t tsLastReport = 0;

int readings = 0;

byte heartRate = 0;
byte spO2 = 0;

bool initialized = false;
bool calculatingValues = false;

HX711 scale;
PulseOximeter pox;
Servo servo;

void handleRoot() {
    constexpr const char page[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1, shrink-to-fit=no">
  <title>ESP32 Hotspot</title>

  <style>
    * { box-sizing: border-box; }

    body {
      margin: 0;
      padding: 0;
      background-color: #333;
      color: #fff;
      font-family: Arial, sans-serif;
    }

    h1 { margin: 0; }

    h2 {
      margin: 0 0 12px 0;
      color: #ddd;
      font-size: 14px;
      font-weight: normal;
    }

    .page-header {
      padding: 12px;
      background-color: #00aaff;
      text-align: center;
      font-size: 12px;
      box-shadow: 0px 8px 8px 0px #292929;
    }

    .page-body {
      min-height: 60vh;
      padding: 32px 24px;
    }

    .section {
      margin-bottom: 48px;
    }

    .value {
      font-size: 24px;
      font-weight: bold;
    }

    .slider-container {
      display: flex;
      align-items: center;
      justify-content: space-around;
    }

    .slider-container button {
      background-color: transparent;
      font-size: 32px;
      border: 0;
      cursor: pointer;
      color: #23c48e;
    }
    
    .slider-container input[type="range"] {
      accent-color: #23c48e;
    }

    .btn {
        background-color: transparent; /* Match slider-container buttons */
        font-size: 32px; /* Match text size */
        border: 0; /* Remove border */
        cursor: pointer;
        color: #23c48e; /* Match color */
        padding: 8px 16px; /* Adjust padding for similar size */
    }

    .btn-actions {
      text-align: center;
    }
    </style>
    <script>
        let currentAngle = 0; // Track the current angle

        function updateData() {
            fetch('/data')
            .then(res => res.json())
            .then(data => {
                document.getElementById('dpm').innerText = data.dpm;
                document.getElementById('weight').innerText = data.weight;

                document.getElementById('hr').innerText = (data.hr === "Place finger on Sensor") ? data.hr : data.hr + " bpm";
                document.getElementById('spo2').innerText = (data.spo2 === "Place finger on Sensor") ? data.spo2 : data.spo2 + " %";

                document.getElementById('angle').innerText = data.angle;
                document.getElementById('angleValue').innerText = data.angle; // Update button display
                currentAngle = data.angle; // Sync current angle
            });
        }

        function changeAngle(step) {
            currentAngle = Math.max(0, Math.min(10, currentAngle + Math.sign(step)));
            fetch(`/servo?angle=${currentAngle}`);
            document.getElementById('angleValue').innerText = currentAngle; // Update instantly
        }

        setInterval(updateData, 5000);
    </script>
</head>
<body>
    <header class = "page-header">
        <h1>ESP32 Sensor Data</h1>
    </header>

    <main class = "page-body">
        <div class = "section">
            <h2>DPM</h2>
            <div id = "dpm" class = "value"> 0 </div>
        </div>

        <div class = "section">
            <h2>Weight</h2>
            <div class = "value"> 
                <span id="weight">0</span> g 
            </div>
        </div>

        <div class = "section">
            <h2>Heart Rate</h2>
            <div class = "value">
                <span id = "hr">0</span> 
            </div>
        </div>

        <div class = "section">
            <h2>Peripheral Oxygen Saturation (SpO2)</h2>
            <div class = "value">
                <span id = "spo2">0</span>
            </div>
        </div>

        <div class = "section">
            <h2>Servo</h2>
            <div class = "value">
                <button class = "btn " type = "button" onClick = "changeAngle(-1)">&minus;</button>
                <span id = "angleValue" class = "value">0</span>
            <button class = "btn" type = "button" onclick="changeAngle(+1)">&plus;</button>
            </div>
        </div>
    </main>
</body>
</html>
    )rawliteral";
    server.send(200, "text/html", page);
}

void handleData() {
    String json = "{";
    json += "\"dpm\":" + String(dropletsPerMinute) + ",";
    json += "\"weight\":" + String(weight) + ",";
    json += "\"hr\":\"" + (avg_heartRate == 0.0 ? String("Place finger on Sensor") : String(avg_heartRate)) + "\",";
    json += "\"spo2\":\"" + (avg_spO2 == 0.0 ? String("Place finger on Sensor") : String(avg_spO2)) + "\",";
    json += "\"angle\":" + String(servoAngle) + "}";
    server.send(200, "application/json", json);
}

int diff = 0;
int prevSliderValue = 0;
void handleServo() {
    if (server.hasArg("angle")) {
        int sliderValue = server.arg("angle").toInt();
        servoAngle = map(sliderValue, 0, 10, 500, 2400);
        servo.write(servoAngle);
    }
    server.send(200, "text/plain", "OK");
}

void setup() {
    Serial.begin(115200);
    WiFi.softAP(ssid, password);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    scale.begin(load_dout, load_sck);
    scale.set_scale(calibration_reading);
    scale.tare();
    
    pox.begin();
    pox.setOnBeatDetectedCallback(beatDetected);
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    displayInitially();
    pinMode(ldr, INPUT);

    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    servo.setPeriodHertz(50);
    servo.attach(servoPin, 1000, 2000);
    servo.writeMicroseconds(1000); //center position

    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.on("/servo", handleServo);
    server.begin();
}

void loop() { 
    pox.update();
    if (SAMPLING_TIME < millis() - tsLastReport) {
        calcAvg(pox.getHeartRate(), pox.getSpO2());
        tsLastReport = millis();
        debug();
    }

    if (millis() - lastBeatDetected > 3000) {
        avg_heartRate = 0;
        avg_spO2 = 0;
        displayInitially();
    }

    if (scale.is_ready()) weight = abs(scale.get_units());
    
    handleDropletCount();
    dropletsPerMinute = (dropletCount / (millis() / 60000.0));
    server.handleClient();
}

void displayInitially() {
    if (!initialized) {
        Serial.println("Place finger on Heartrate Sensor");
        initialized = true;
    }
}

void beatDetected() {
    lastBeatDetected = millis();
}

void displayDuringCalc(int i) {
    if (!calculatingValues) {
        calculatingValues = true;
        initialized = false;
        Serial.println("Processing");
    }
    Serial.print(".");
}

void calcAvg(int heartRate, int spO2) {
    if (heartRate > 30 && heartRate < 220 && spO2 > 50) {
        avg_heartRate = weighted_avg * (heartRate) + (1-weighted_avg) * avg_heartRate;
        avg_spO2 = weighted_avg * (spO2) + (1 - weighted_avg) * avg_spO2;
        
        //displayDuringCalc(readings);
    } else {
        avg_heartRate = 0.0;
        avg_spO2 = 0.0;
    }
    readings++;
    if (readings == BUFFER_SIZE) {
        readings = 0;
        debug();
    }
}

void debug() {
    Serial.print(dropletsPerMinute);    
    Serial.print(" dpm | ");
    Serial.print(weight);
    Serial.print("g | ");
    Serial.print(avg_heartRate);
    Serial.print(" bpm | ");
    Serial.print(avg_spO2);
    Serial.print(" % | ");
    Serial.print(servoAngle);
    Serial.print("° | ");
    Serial.print(dropletCount);
    Serial.print(" drops");
    Serial.println();
}

bool dropFlag = false;
void handleDropletCount() {
    int dropVal = analogRead(ldr);
    if (dropVal < 4010 && !dropFlag) dropFlag = true;
    if (dropVal >= 4010 && dropFlag) {
        dropletCount ++;
        dropFlag = false;
        Serial.println(dropVal);
        delay(50);
    }
}
