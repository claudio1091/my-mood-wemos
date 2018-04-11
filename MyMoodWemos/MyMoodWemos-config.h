// Pins
// In case of BRIGHTNESS: only WHITE is used
// In case of RGB(W): red, green, blue(, white) is used
// All values need to be present, if they are not needed, set to -1,
// it will be ignored.
#define CONFIG_PIN_RED   13  // For RGB(W)
#define CONFIG_PIN_GREEN 14  // For RGB(W)
#define CONFIG_PIN_BLUE  12  // For RGB(W)

// DHT
// Define the DHT type in the code file.
#define CONFIG_DHT_PIN 16
#define DHT_TYPE DHT11 // Update this to match your DHT type

// WiFi
#define CONFIG_WIFI_SSID "{WIFI-SSID}"
#define CONFIG_WIFI_PASS "{WIFI-PASSWORD}"

#define FIREBASE_HOST "https://my-mood-a16e2.firebaseio.com/"
#define FIREBASE_AUTH "XI16Hf2dp7Mxpr6j5CCXhziczX9AixXSkT9z2n3y"

// Publique a cada 5 min
#define PUBLISH_INTERVAL 1000*60*5
// Get a cada 30 segundos
#define GET_INTERVAL 1000*30

// Reverse the LED logic
// false: 0 (off) - 255 (bright)
// true: 255 (off) - 0 (bright)
#define CONFIG_INVERT_LED_LOGIC false

// Set the mode for the built-in LED on some boards.
// -1 = Do nothing. Leave the pin in its default state.
//  0 = Explicitly set the BUILTIN_LED to LOW.
//  1 = Explicitly set the BUILTIN_LED to HIGH. (Off for Wemos D1 Mini)
#define CONFIG_BUILTIN_LED_MODE 1

// Enables Serial and print statements
#define CONFIG_DEBUG false
