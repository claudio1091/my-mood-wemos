/*
   ESP8266 MQTT Lights for Home Assistant.

   Created DIY lights for Home Assistant using MQTT and JSON.
   This project supports single-color, RGB, and RGBW lights.

   Copy the included `config-sample.h` file to `config.h` and update
   accordingly for your setup.

   See https://github.com/corbanmailloux/esp-mqtt-rgb-led for more information.
*/

// Set configuration options for LED type, pins, WiFi, and MQTT in the following file:
#include "MyMoodWemos-config.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <FirebaseArduino.h>

// https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>
// http://pubsubclient.knolleary.net/
#include <PubSubClient.h>

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
    {255, 255, 0, -1}};
const int numColors = 8;

//To make Arduino software autodetect OTA device
WiFiServer TelnetServer(8266);

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTimePublishFirebase = 0;
unsigned long lastTimePublishMqtt = 0;

void setup()
{
  if (CONFIG_DEBUG)
  {
    Serial.begin(115200);
  }

  //To make Arduino software autodetect OTA device
  TelnetServer.begin();
  
  pinMode(CONFIG_PIN_RED, OUTPUT);
  pinMode(CONFIG_PIN_GREEN, OUTPUT);
  pinMode(CONFIG_PIN_BLUE, OUTPUT);

  analogWriteRange(255);

  setup_wifi();

  // Arduino OTA
  ArduinoOTA.setHostname("my-mood-device");
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA End");
    Serial.println("Rebooting...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  client.setServer(CONFIG_MQTT_HOST, CONFIG_MQTT_PORT);
  client.setCallback(callback);

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);
}

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network

  if (CONFIG_DEBUG)
  {
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(CONFIG_WIFI_SSID);
  }

  WiFi.mode(WIFI_STA); // Disable the built-in WiFi access point.
  WiFi.begin(CONFIG_WIFI_SSID, CONFIG_WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  if (CONFIG_DEBUG)
  {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  blinkBuiltInLed(2);
}

void callback(char *topic, byte *payload, unsigned int length)
{
  if (CONFIG_DEBUG)
  {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.println("] ");
  }

  char message[length + 1];
  for (int i = 0; i < length; i++)
  {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';

  if (CONFIG_DEBUG)
  {
    Serial.println(message);
  }

  if (!processJson(message))
  {
    return;
  }

  if (stateOn)
  {
    // Update lights
    realRed = map(red, 0, 255, 0, brightness);
    realGreen = map(green, 0, 255, 0, brightness);
    realBlue = map(blue, 0, 255, 0, brightness);
  }
  else
  {
    realRed = 0;
    realGreen = 0;
    realBlue = 0;
  }

  startFade = true;
  inFade = false; // Kill the current fade

  sendState();
}

bool processJson(char *message)
{
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject &root = jsonBuffer.parseObject(message);

  if (!root.success())
  {
    Serial.println("parseObject() failed");
    return false;
  }

  if (root.containsKey("state"))
  {
    if (strcmp(root["state"], "true") == 0)
    {
      stateOn = true;
    }
    else
    {
      stateOn = false;
    }
  }

  if (strcmp(root["colorFade"], "true") == 0)
  {

    flash = false;
    colorfade = true;
    currentColor = 0;

    if (root.containsKey("transition"))
    {
      transitionTime = root["transition"];
    }
    else
    {
      transitionTime = 30;
    }
  }
  else if (colorfade && !root.containsKey("color") && root.containsKey("brightness"))
  {
    // Adjust brightness during colorfade
    // (will be applied when fading to the next color)
    brightness = root["brightness"];
  }
  else
  { //  ==== NO EFFECT ====
    flash = false;
    colorfade = false;

    if (rgb && root.containsKey("color"))
    {
      red = root["color"]["r"];
      green = root["color"]["g"];
      blue = root["color"]["b"];
    }

    if (root.containsKey("brightness"))
    {
      brightness = root["brightness"];
    }

    if (root.containsKey("transition"))
    {
      transitionTime = root["transition"];
    }
    else
    {
      transitionTime = 30;
    }
  }

  return true;
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // if (client.connect(CONFIG_MQTT_CLIENT_ID, CONFIG_MQTT_USER, CONFIG_MQTT_PASS)) {
    if (client.connect(CONFIG_MQTT_CLIENT_ID + ESP.getChipId()))
    {
      Serial.println("connected");
      client.subscribe(CONFIG_MQTT_TOPIC_SET);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void sendState()
{
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject &root = jsonBuffer.createObject();

  root["state"] = stateOn;

  JsonObject &color = root.createNestedObject("color");
  color["r"] = red;
  color["g"] = green;
  color["b"] = blue;

  root["brightness"] = brightness;
  root["colorFade"] = colorfade;

  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  client.publish(CONFIG_MQTT_TOPIC_STATE, buffer, true);
  blinkBuiltInLed(5);
}

void setColor(int inR, int inG, int inB)
{
  blinkBuiltInLed(3);

  if (CONFIG_INVERT_LED_LOGIC)
  {
    inR = (255 - inR);
    inG = (255 - inG);
    inB = (255 - inB);
  }

  analogWrite(CONFIG_PIN_RED, inR);
  analogWrite(CONFIG_PIN_GREEN, inG);
  analogWrite(CONFIG_PIN_BLUE, inB);

  if (CONFIG_DEBUG)
  {
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

void loop()
{
  unsigned long currentMillis = millis();

  if (!client.connected())
  {
    reconnect();
  }

  ArduinoOTA.handle();
  client.loop();

  // Publique Firebase
  if (lastTimePublishFirebase == 0 || (currentMillis - lastTimePublishFirebase > PUBLISH_DATA_FIREBASE))
  {
    lastTimePublishFirebase = currentMillis;
    DynamicJsonBuffer jsonBuffer;

    float analogTemperature = analogRead(CONFIG_PIN_LM35);
    float temperature = (analogTemperature * 0.00488);
    temperature = temperature * 100;

    if (!isnan(temperature))
    {
      // Push to Firebase
      JsonObject &temperatureObject = jsonBuffer.createObject();
      JsonObject &tempTime = temperatureObject.createNestedObject("timestamp");
      temperatureObject["temperature"] = temperature;
      tempTime[".sv"] = "timestamp";

      // Manda para o firebase
      Firebase.push("/temperature", temperatureObject);

      if (Firebase.failed())
      {
        Serial.print("pushing /temperature failed:");
        Serial.println(Firebase.error());
      }
    }
    else
    {
      Serial.println("Error Reading");
    }
  }

  // Publique MQTT
  currentMillis = millis();
  if (lastTimePublishMqtt == 0 || (currentMillis - lastTimePublishMqtt > PUBLISH_DATA_MQTT))
  {
    float analogTemperature = analogRead(CONFIG_PIN_LM35);
    float temperature = (analogTemperature * 0.00488);
    temperature = temperature * 100;

    if (!isnan(temperature))
    {
      // Push to Firebase
      JsonObject &temperatureObject = jsonBuffer.createObject();
      JsonObject &tempTime = temperatureObject.createNestedObject("timestamp");
      temperatureObject["temperature"] = temperature;
      tempTime[".sv"] = "timestamp";

      client.publish(CONFIG_MQTT_TOPIC_TEMP, temperatureObject);
    }
    else
    {
      Serial.println("Error Reading");
    }
  }

  if (colorfade && !inFade)
  {
    realRed = map(colors[currentColor][0], 0, 255, 0, brightness);
    realGreen = map(colors[currentColor][1], 0, 255, 0, brightness);
    realBlue = map(colors[currentColor][2], 0, 255, 0, brightness);
    currentColor = (currentColor + 1) % numColors;
    startFade = true;
  }

  if (startFade)
  {
    // If we don't want to fade, skip it.
    if (transitionTime == 0)
    {
      setColor(realRed, realGreen, realBlue);

      redVal = realRed;
      grnVal = realGreen;
      bluVal = realBlue;

      startFade = false;
    }
    else
    {
      loopCount = 0;
      stepR = calculateStep(redVal, realRed);
      stepG = calculateStep(grnVal, realGreen);
      stepB = calculateStep(bluVal, realBlue);

      inFade = true;
    }
  }

  if (inFade)
  {
    startFade = false;
    unsigned long now = millis();
    if (now - lastLoop > transitionTime)
    {
      if (loopCount <= 1020)
      {
        lastLoop = now;

        redVal = calculateVal(stepR, redVal, loopCount);
        grnVal = calculateVal(stepG, grnVal, loopCount);
        bluVal = calculateVal(stepB, bluVal, loopCount);

        setColor(redVal, grnVal, bluVal); // Write current values to LED pins

        loopCount++;
      }
      else
      {
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
int calculateStep(int prevValue, int endValue)
{
  int step = endValue - prevValue; // What's the overall gap?
  if (step)
  {                     // If its non-zero,
    step = 1020 / step; //   divide by 1020
  }

  return step;
}

/* The next function is calculateVal. When the loop value, i,
   reaches the step size appropriate for one of the
   colors, it increases or decreases the value of that color by 1.
   (R, G, and B are each calculated separately.)
*/
int calculateVal(int step, int val, int i)
{
  if ((step) && i % step == 0)
  { // If step is non-zero and its time to change a value,
    if (step > 0)
    { //   increment the value if step is positive...
      val += 1;
    }
    else if (step < 0)
    { //   ...or decrement it if step is negative
      val -= 1;
    }
  }

  // Defensive driving: make sure val stays in the range 0-255
  if (val > 255)
  {
    val = 255;
  }
  else if (val < 0)
  {
    val = 0;
  }

  return val;
}

void blinkBuiltInLed(int blinks)
{
  int blinksAlready = 0;

  while (blinksAlready < (blinks * 2))
  {
    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED)); // Built in LED ON
    blinksAlready++;
  }
}
