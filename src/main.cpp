#include <Arduino.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <display.h> // TODO: abstract display interface. WriteText(), Clear(), StatusEmoji(),Off() ect.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
const auto SDA = GPIO_NUM_0;
const auto SCL = GPIO_NUM_0;

const auto SCREEN_WIDTH = 128; // OLED display width, in pixels
const auto SCREEN_HEIGHT = 32; // OLED display height, in pixels
const auto OLED_RESET = -1;    // Reset pin # (or -1 if sharing Arduino reset pin)
const auto refreshRate_ms = 1000 / 10;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

RTC_DATA_ATTR int64_t wifiTimer = 0; // decrement this while running, also decrement after sleep

const char *ssid = "hello";
const char *password = "12345678";
WiFiServer server(80);
void handleClient(WiFiClient client);

const auto sleepPeriod_ms = 10 * 60 * 1000; // 10 minutes
const auto WAKEUP_GPIO = GPIO_NUM_10;
const auto WiFi_GPIO = GPIO_NUM_0;
constexpr uint64_t BUTTON_PIN_BITMASK(uint64_t pin);

const auto OneWire_Power_GPIO = GPIO_NUM_0; // could be used for more than just OneWire, also might be negligible benefit
const auto ONE_WIRE_BUS = GPIO_NUM_0;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const auto ChargeCtrl_Select1_GPIO = GPIO_NUM_0;
const auto ChargeCtrl_Select2_GPIO = GPIO_NUM_0;

const auto OLED_Power_GPIO = GPIO_NUM_0;

void setup()
{
    // Pins
    pinMode(WAKEUP_GPIO, INPUT);
    pinMode(WiFi_GPIO, INPUT);
    pinMode(OneWire_Power_GPIO, OUTPUT);
    pinMode(ChargeCtrl_Select1_GPIO, OUTPUT);
    pinMode(ChargeCtrl_Select2_GPIO, OUTPUT);

    if (digitalRead(WiFi_GPIO) == HIGH)
    {
        wifiTimer = 16 * 60 * 60 * 1000; // 16 hours
    }
    else if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO)
    {
        wifiTimer = decrementTimer(wifiTimer, sleepPeriod_ms);
    }

    // serial
    Serial.begin(19200, SERIAL_8N1); // input serial port (VE device)
    Serial.flush();

    if (wifiTimer)
    {
        if (!WiFi.softAP(ssid, password))
        {
            Serial.println("Soft AP creation failed.");
            while (1)
            {
            }
        }
        WiFi.setHostname("Cabin Control");
        IPAddress myIP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(myIP);
        server.begin();
    }

    Wire.setPins(SDA, SCL);
}

void loop()
{
    static bool inactive = false;

    // Get data from sensors
    static auto SensorsTimer = millis();
    if (millis() - SensorsTimer > sleepPeriod_ms)
    {
        SensorsTimer = millis();

        // Temperature
        digitalWrite(OneWire_Power_GPIO, HIGH);
        sensors.requestTemperatures();
        float temperature = sensors.getTempC(0, 2);
        digitalWrite(OneWire_Power_GPIO, LOW);

	const auto interCharTimout =10; //uS
        // Charge controller stats
        digitalWrite(ChargeCtrl_Select1_GPIO, HIGH);
        // Read charge controller1 information
        // Serial.write():
	// TODO: group update commands into a updateAll function
	loadCurrent = GetValue(loadCurrent);
	solarCurrent = GetValue()

	digitalWrite(ChargeCtrl_Select1_GPIO, LOW);
        digitalWrite(ChargeCtrl_Select2_GPIO, HIGH);
        // Read charge controller2 information
        // Serial.write();
        digitalWrite(ChargeCtrl_Select2_GPIO, LOW);

	// TODO: get current off shunt sensor for AC load/generator
    }

    // Wifi Control
    if (wifiTimer)
    {
        WiFiClient client = server.accept();

        if (client)
        {
            handleClient(client);
        }
        // TODO: turn off wifi from user input
    }

    // OLED
    static auto refreshTimer = millis();
    if (digitalRead(WAKEUP_GPIO) == HIGH && millis() - refreshTimer > refreshRate_ms)
    {
        refreshTimer = millis();
        digitalWrite(OLED_Power_GPIO, HIGH);
        display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // configure display Voltage
        display.clearDisplay();
        // Write Charge status
        display.setTextSize(2); // Draw 2X-scale text
        display.setTextColor(WHITE);
        display.setCursor(10, 0);
        display.println(F("Text"));
        display.display();
    }
    else if (digitalRead(WAKEUP_GPIO) == LOW) // turn off OLED
    {
        digitalWrite(OLED_Power_GPIO, LOW);
    }
//TODO abstract abovr to use disaply interface
//{
//Init()
//writeText(powerStatus)
//writeText(battey status)
//} else {
//off()
//}


    // Sleep
    if (inactive)
    {
        // https://randomnerdtutorials.com/esp32-deep-sleep-arduino-ide-wake-up-sources/
        esp_sleep_enable_timer_wakeup(sleepPeriod_ms * 1000);
        esp_deep_sleep_enable_gpio_wakeup(BUTTON_PIN_BITMASK(WAKEUP_GPIO), ESP_GPIO_WAKEUP_GPIO_HIGH);
        esp_deep_sleep_start();
    }
}

constexpr uint64_t BUTTON_PIN_BITMASK(uint64_t pin)
{
    return (uint64_t)1 << (pin);
}

void handleClient(WiFiClient client)
{
    Serial.println("New Client.");
    while (client.connected())
    {
        if (client.available())
        {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            client.print("Hello World!");

            client.println();
            break;
        }
    }
    client.stop();
    Serial.println("Client Disconnected.");
}

int64_t decrementTimer(int64_t timer, int64_t change)
{
    auto newtime = timer - change;
    return newtime > 0 ? newtime : 0;
}
