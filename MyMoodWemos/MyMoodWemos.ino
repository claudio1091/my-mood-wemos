/*
   ESP8266 MQTT Lights for Home Assistant.

   Created DIY lights for Home Assistant using MQTT and JSON.
   This project supports single-color, RGB, and RGBW lights.

   Copy the included `config-sample.h` file to `config.h` and update
   accordingly for your setup.

   See https://github.com/corbanmailloux/esp-mqtt-rgb-led for more information.
*/

// Set configuration options for LED type, pins, WiFi, and MQTT in the following file:
#include "config-my-mood.h"

#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Ticker.h>

// Maintained state for reporting to HA
byte red = 255;
byte green = 255;
byte blue = 255;
byte brightness = 255;

// Real values to write to the LEDs (ex. including brightness and state)
byte realRed = 0;
byte realGreen = 0;
byte realBlue = 0;

bool stateOn = false;

// Globals for fade/transitions
bool startFade = false;
unsigned long lastLoop = 0;
int transitionTime = 30;
bool inFade = false;
int loopCount = 0;
int stepR, stepG, stepB;
int redVal, grnVal, bluVal;

// Globals for colorfade
bool colorfade = false;
int currentColor = 0;
// {red, grn, blu, wht}
const byte colors[][4] = {
  {255, 255, 255, -1},
  {255, 0, 0, -1},
  {0, 255, 0, -1},
  {0, 0, 255, -1},
  {255, 80, 0, -1},
  {163, 0, 255, -1},
  {0, 255, 255, -1},
  {255, 255, 0, -1}
};
const int numColors = 8;

DHT_Unified dht(CONFIG_DHT_PIN, DHT_TYPE);
unsigned long lastSampleTime = 0;

Ticker ticker;
bool publishNewState = true;
bool lightState = true;

void setup() {
  if (CONFIG_DEBUG) {
    Serial.begin(115200);
  }

  pinMode(CONFIG_PIN_RED, OUTPUT);
  pinMode(CONFIG_PIN_GREEN, OUTPUT);
  pinMode(CONFIG_PIN_BLUE, OUTPUT);

  analogWriteRange(255);

  setup_wifi();

  // Set up the DHT sensor
  dht.begin();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW); // Built in LED ON

  // Registra o ticker para publicar de tempos em tempos
  ticker.attach_ms(PUBLISH_INTERVAL, publish);
  ticker.attach_ms(GET_INTERVAL, getLightState);
}

void publish() {
  publishNewState = true;
}

void getLightState() {
  lightState = true;
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network

  if (CONFIG_DEBUG) {
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(CONFIG_WIFI_SSID);
  }

  WiFi.mode(WIFI_STA); // Disable the built-in WiFi access point.
  WiFi.begin(CONFIG_WIFI_SSID, CONFIG_WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  if (CONFIG_DEBUG) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  blinkBuiltInLed(2);
}

void processLightFirebase(FirebaseObject lightParam) {
  stateOn = lightParam.getBool("/state");
  
  red = lightParam.getInt("/color/r");
  green = lightParam.getInt("/color/g");
  blue = lightParam.getInt("/color/b");
  brightness = lightParam.getInt("/brightness");
  transitionTime = lightParam.getInt("/transition");
  colorfade = lightParam.getBool("/colorFade");
  
  startFade = true;
  inFade = false; // Kill the current fade
}

void setColor(int inR, int inG, int inB) {
  blinkBuiltInLed(3);

  if (CONFIG_INVERT_LED_LOGIC) {
    inR = (255 - inR);
    inG = (255 - inG);
    inB = (255 - inB);
  }

  analogWrite(CONFIG_PIN_RED, inR);
  analogWrite(CONFIG_PIN_GREEN, inG);
  analogWrite(CONFIG_PIN_BLUE, inB);

  if (CONFIG_DEBUG) {
    Serial.print("Setting LEDs: {");

    Serial.print("r: ");
    Serial.print(inR);
    Serial.print(" , g: ");
    Serial.print(inG);
    Serial.print(" , b: ");
    Serial.print(inB);

    Serial.println("}");
  }
}

void loop() {
  // Apenas publique quando passar o tempo determinado
  if (publishNewState) {
    Serial.println("Publish new State");

    // Obtem os dados do sensor DHT 
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (!isnan(humidity) && !isnan(temperature)) {
      // Manda para o firebase
      Firebase.pushFloat("temperature", temperature);
      Firebase.pushFloat("humidity", humidity);    
      publishNewState = false;
    } else {
      Serial.println("Error Publishing");
    }
  }

  if (lightState) {
    FirebaseObject lightParam = Firebase.get("/rgbstrip");
    processLightFirebase(lightParam);
  }

  if (colorfade && !inFade) {
    realRed = map(colors[currentColor][0], 0, 255, 0, brightness);
    realGreen = map(colors[currentColor][1], 0, 255, 0, brightness);
    realBlue = map(colors[currentColor][2], 0, 255, 0, brightness);
    currentColor = (currentColor + 1) % numColors;
    startFade = true;
  }

  if (startFade) {
    // If we don't want to fade, skip it.
    if (transitionTime == 0) {
      setColor(realRed, realGreen, realBlue);

      redVal = realRed;
      grnVal = realGreen;
      bluVal = realBlue;

      startFade = false;
    } else {
      loopCount = 0;
      stepR = calculateStep(redVal, realRed);
      stepG = calculateStep(grnVal, realGreen);
      stepB = calculateStep(bluVal, realBlue);

      inFade = true;
    }
  }

  if (inFade) {
    startFade = false;
    unsigned long now = millis();
    if (now - lastLoop > transitionTime) {
      if (loopCount <= 1020) {
        lastLoop = now;

        redVal = calculateVal(stepR, redVal, loopCount);
        grnVal = calculateVal(stepG, grnVal, loopCount);
        bluVal = calculateVal(stepB, bluVal, loopCount);

        setColor(redVal, grnVal, bluVal); // Write current values to LED pins

        if (CONFIG_DEBUG) {
          Serial.print("Loop count: ");
          Serial.println(loopCount);
        }

        loopCount++;
      } else {
        inFade = false;
      }
    }
  }
}

// From https://www.arduino.cc/en/Tutorial/ColorCrossfader
/* BELOW THIS LINE IS THE MATH -- YOU SHOULDN'T NEED TO CHANGE THIS FOR THE BASICS

  The program works like this:
  Imagine a crossfade that moves the red LED from 0-10,
    the green from 0-5, and the blue from 10 to 7, in
    ten steps.
    We'd want to count the 10 steps and increase or
    decrease color values in evenly stepped increments.
    Imagine a + indicates raising a value by 1, and a -
    equals lowering it. Our 10 step fade would look like:

    1 2 3 4 5 6 7 8 9 10
  R + + + + + + + + + +
  G   +   +   +   +   +
  B     -     -     -

  The red rises from 0 to 10 in ten steps, the green from
  0-5 in 5 steps, and the blue falls from 10 to 7 in three steps.

  In the real program, the color percentages are converted to
  0-255 values, and there are 1020 steps (255*4).

  To figure out how big a step there should be between one up- or
  down-tick of one of the LED values, we call calculateStep(),
  which calculates the absolute gap between the start and end values,
  and then divides that gap by 1020 to determine the size of the step
  between adjustments in the value.
*/
int calculateStep(int prevValue, int endValue) {
  int step = endValue - prevValue; // What's the overall gap?
  if (step) {                      // If its non-zero,
    step = 1020 / step;          //   divide by 1020
  }

  return step;
}

/* The next function is calculateVal. When the loop value, i,
   reaches the step size appropriate for one of the
   colors, it increases or decreases the value of that color by 1.
   (R, G, and B are each calculated separately.)
*/
int calculateVal(int step, int val, int i) {
  if ((step) && i % step == 0) { // If step is non-zero and its time to change a value,
    if (step > 0) {              //   increment the value if step is positive...
      val += 1;
    }
    else if (step < 0) {         //   ...or decrement it if step is negative
      val -= 1;
    }
  }

  // Defensive driving: make sure val stays in the range 0-255
  if (val > 255) {
    val = 255;
  }
  else if (val < 0) {
    val = 0;
  }

  return val;
}

void blinkBuiltInLed(int blinks) {
  int blinksAlready = 0;

  while (blinksAlready < (blinks * 2)) {
    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED)); // Built in LED ON
    blinksAlready++;
  }
}
