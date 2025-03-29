#include <Arduino.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

#include <OneWire.h>
#include <DallasTemperature.h>

RTC_DATA_ATTR int64_t wifiTimer = 0; // decrement this while running, also decrement after sleep

const char *ssid = "hello";
const char *password = "12345678";
WiFiServer server(80);

const auto sleepPeriod_ms = 10 * 60 * 1000; // 10 minutes
const auto WAKEUP_GPIO = GPIO_NUM_10;
const auto WiFi_GPIO = GPIO_NUM_10;

const auto ONE_WIRE_BUS = GPIO_NUM_2;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void handleClient(WiFiClient client);
constexpr uint64_t BUTTON_PIN_BITMASK(uint64_t pin);

void setup()
{
    // Pins
    pinMode(WAKEUP_GPIO, INPUT);
    pinMode(WiFi_GPIO, INPUT);

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
        sensors.requestTemperatures();
        float temperature = sensors.getTempC(0, 2);

        // Charge controller stats
    }
    // Screen Control

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