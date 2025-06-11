#include <Arduino.h>

#include <ESP32Time.h>
ESP32Time rtc; // pass tz offset ins seconds to constructor

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include "ESPAsyncWebServer.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// #include <display.h> // TODO: abstract display interface. WriteText(), Clear(), StatusEmoji(),Off() ect.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <VEDirectHex.h>
const auto testing = true;

const auto SCREEN_WIDTH = 128; // OLED display width, in pixels
const auto SCREEN_HEIGHT = 32; // OLED display height, in pixels
const auto OLED_RESET = -1;    // Reset pin # (or -1 if sharing Arduino reset pin)
const auto refreshRate_ms = 1000 / 10;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

RTC_DATA_ATTR int64_t wifiTimer = 0; // decrement this while running, also decrement after sleep

const char *ssid = "hello";
const char *password = "12345678";
// WiFiServer server(80);
void handleClient(WiFiClient client);
AsyncWebServer server(80);

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

const auto SD_CS = GPIO_NUM_0;

constexpr uint64_t BUTTON_PIN_BITMASK(uint64_t pin);
void handleClient(WiFiClient client);
int64_t decrementTimer(int64_t timer, int64_t change);
void appendFile(fs::FS &fs, const char *path, const char *message);
void writeFile(fs::FS &fs, const char *path, const char *message);

void setup()
{
    // Pins
    pinMode(WAKEUP_GPIO, INPUT);
    pinMode(WiFi_GPIO, INPUT);
    pinMode(OneWire_Power_GPIO, OUTPUT);
    pinMode(ChargeCtrl_Select1_GPIO, OUTPUT);
    pinMode(ChargeCtrl_Select2_GPIO, OUTPUT);

    if (digitalRead(WiFi_GPIO) == HIGH || testing == true)
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
        WiFi.softAPIP();

        server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send_P(200, "text/plain", "hello"); });
        server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send_P(200, "text/plain", "there"); });
        server.on("/setTime", HTTP_POST, [](AsyncWebServerRequest *request)
                  {
            size_t count = request->params(); // TODO use params to set date and time
            for (size_t i = 0; i < count; i++)
            {
                const AsyncWebParameter *p = request->getParam(i);
                Serial.printf("PARAM[%u]: %s = %s\n", i, p->name().c_str(), p->value().c_str());
            }

            String who;
            if (request->hasParam("who", true))
            {
                who = request->getParam("who", true)->value();
            }
            else
            {
                who = "No message sent";
            }
            request->send(200, "text/plain", "Time Set"); });

        server.begin();
    }

    Wire.begin();
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
        float temperature1 = sensors.getTempCByIndex(0);
        float temperature2 = sensors.getTempCByIndex(1);
        float temperature3 = sensors.getTempCByIndex(2);
        float temperature4 = sensors.getTempCByIndex(3);
        float temperature5 = sensors.getTempCByIndex(4);
        digitalWrite(OneWire_Power_GPIO, LOW);
        // TODO save values

        const auto interCharTimout = 10; // uS
        // Charge controller stats
        digitalWrite(ChargeCtrl_Select1_GPIO, HIGH);
        // Read charge controller1 information
        // Serial.write():
        // TODO: group update commands into a updateAll function
        auto loadCurrent = GetValue(Serial, VeDirectHexRegister::loadCurrent, valueSize::_32);
        auto solarCurrent = GetValue(Serial, VeDirectHexRegister::chargeCurrent, valueSize::_32);

        digitalWrite(ChargeCtrl_Select1_GPIO, LOW);
        digitalWrite(ChargeCtrl_Select2_GPIO, HIGH);
        // Read charge controller2 information
        // Serial.write();
        digitalWrite(ChargeCtrl_Select2_GPIO, LOW);

        // TODO: get current off shunt sensor for AC load/generator

        SD.begin(SD_CS);
        if (!SD.begin(SD_CS))
        {
            Serial.println("Card Mount Failed");
            return;
        }
        uint8_t cardType = SD.cardType();
        if (cardType == CARD_NONE)
        {
            Serial.println("No SD card attached");
            return;
        }
        Serial.println("Initializing SD card...");
        if (!SD.begin(SD_CS))
        {
            Serial.println("ERROR - SD card initialization failed!");
            return; // init failed
        }

        if (SD.usedBytes() > SD.totalBytes() * .9) // housekeeping, delete "oldest" file
        {
            auto timeData = rtc.getTimeStruct();
            char newDateBuffer[32];
            String fileName = ("/data/" + rtc.getTime("%Y-%m-%d") + ".csv");
            while (SD.exists(fileName))
            {
                timeData.tm_mday--;
                mktime(&timeData);                                                     // normalizes time data so day rolls over
                strftime(newDateBuffer, sizeof(newDateBuffer), "%Y-%m-%d", &timeData); // formats date into buffer

                fileName = "/data/";
                fileName += newDateBuffer;
                fileName += ".csv";
            }
            timeData.tm_mday++;
            mktime(&timeData);                                                     // normalizes time data so day rolls over
            strftime(newDateBuffer, sizeof(newDateBuffer), "%Y-%m-%d", &timeData); // formats date into buffer

            fileName = "/data/";
            fileName += newDateBuffer;
            fileName += ".csv";

            if (SD.exists(fileName))
            {
                SD.remove(fileName);
            }
        }

        auto fileName = ("/data/" + rtc.getTime("%Y-%m-%d") + ".csv").c_str();
        if (SD.exists(fileName))
        {
            writeFile(SD, fileName, "Time,temp1,temp2,ect\n"); // TODO finish header
        }

        auto line = rtc.getTime() + "," + temperature1 + "," + temperature2 + "," + temperature3 + "\n";
        appendFile(SD, fileName, line.c_str());
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
    // TODO abstract abovr to use disaply interface
    //{
    // Init()
    // writeText(powerStatus)
    // writeText(battey status)
    // } else {
    // off()
    // }

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

void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        Serial.println("File written");
    }
    else
    {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        Serial.println("Failed to open file for appending");
        return;
    }
    if (file.print(message))
    {
        Serial.println("Message appended");
    }
    else
    {
        Serial.println("Append failed");
    }
    file.close();
}