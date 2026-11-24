#include <Arduino.h>

#include <ESP32Time.h>
ESP32Time rtc; // pass tz offset ins seconds to constructor

#include "FS.h"
#include "SD.h" // if using SD_MMC supported esp32 then use SD_MMC.h, will require some changes to the code
#include "SPI.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include "ESPAsyncWebServer.h"

extern "C"
{
#include "miniz.h"
}
bool gzipFile(const char *input_path, const char *output_path);

#include <OneWire.h>
#include <DallasTemperature.h>

// #include <display.h> // TODO: abstract display interface. WriteText(), Clear(), StatusEmoji(),Off() ect.
#include <Wire.h>
#include <Adafruit_GFX.h>
// #include <Adafruit_SSD1306.h>
#include <GxEPD2_BW.h>

#include <VictronTelemetry.h>
const auto testing = true;

const auto SCREEN_WIDTH = 128; // OLED display width, in pixels
const auto SCREEN_HEIGHT = 32; // OLED display height, in pixels
const auto OLED_RESET = -1;    // Reset pin # (or -1 if sharing Arduino reset pin)
const auto refreshRate_ms = 1000 / 10;
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define EPD_CS 5   // Actually define these, this is just copy pasted
#define EPD_DC 17  // Actually define these, this is just copy pasted
#define EPD_RST 16 // Actually define these, this is just copy pasted
#define EPD_BUSY 4 // Actually define these, this is just copy pasted
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

const char *ssid = "Thanksgiving-Cabin";
const char *password = "stuandstuinc";
// WiFiServer server(80);
void handleClient(WiFiClient client);
AsyncWebServer server(80);
const auto WiFi_Hold_Time_ms = 16 * 60 * 60 * 1000; // 16 hours

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

const auto SD_CS = GPIO_NUM_7;

constexpr uint64_t BUTTON_PIN_BITMASK(uint64_t pin);
void handleClient(WiFiClient client);
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
    digitalWrite(ChargeCtrl_Select1_GPIO, LOW);
    digitalWrite(ChargeCtrl_Select2_GPIO, LOW);

    // serial
    Serial.begin(19200, SERIAL_8N1); // Dedicated to VE.Direct frames
    Serial.flush();

    Wire.begin();
}

void loop()
{
    static bool active = false || testing == true;
    static auto inactiveTimer = millis();
    if (digitalRead(WAKEUP_GPIO) == HIGH)
    {
        inactiveTimer = millis();
        active = true;
    }
    else if (millis() - inactiveTimer > 10 * 1000)
    {
        active = false;
    }

    static auto wifiTimer = 0; // off by default
    if (digitalRead(WiFi_GPIO) == HIGH || testing == true)
    {
        wifiTimer = millis();
    }
    if (wifiTimer && WiFi.getMode() == WIFI_OFF) // init Wifi
    {
        if (!WiFi.softAP(ssid, password))
        {
            while (1)
            {
            }
        }
        WiFi.setHostname("Cabin Control");
        WiFi.softAPIP();

        SD.begin(SD_CS, SPI, 20000000);
        server.serveStatic("/data/", SD, "/data/").setCacheControl("no-cache");
        server.serveStatic("/", SD, "/").setDefaultFile("index.html").setCacheControl("no-cache");

        server.on("/api/temperature", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send(200, "text/plain", "hello"); });
        server.on("/api/humidity", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send(200, "text/plain", "there"); });
        server.on("/api/setTime", HTTP_POST, [](AsyncWebServerRequest *request)
                  {
            if (!request->hasParam("epoch", true))
            {
                request->send(400);
                return;
            }
            const String epoch = request->getParam("epoch", true)->value();
            char *end;
            const unsigned long seconds = strtoul(epoch.c_str(), &end, 10);
            if (epoch.length() != 10 || *end || seconds < 1704067200UL || seconds > INT32_MAX)
            {
                request->send(400);
                return;
            }
            if (rtc.getEpoch() < 1704067200UL)
                rtc.setTime(seconds);
            request->send(204); });

        server.begin();
    }

    // Get data from sensors
    static auto SensorsTimer = millis();
    if (millis() - SensorsTimer > sleepPeriod_ms)
    {
        SensorsTimer = millis();

        // Temperature
        digitalWrite(OneWire_Power_GPIO, HIGH);
        sensors.begin();
        sensors.requestTemperatures();
        float temperature1 = sensors.getTempCByIndex(0);
        float temperature2 = sensors.getTempCByIndex(1);
        float temperature3 = sensors.getTempCByIndex(2);
        // float temperature4 = sensors.getTempCByIndex(3);
        // float temperature5 = sensors.getTempCByIndex(4);
        digitalWrite(OneWire_Power_GPIO, LOW);
        int64_t victron[victronCount];
        digitalWrite(ChargeCtrl_Select2_GPIO, LOW);
        digitalWrite(ChargeCtrl_Select1_GPIO, HIGH);
        pollVictron(Serial, victron);
        digitalWrite(ChargeCtrl_Select1_GPIO, LOW);

        if (SD.begin(SD_CS, SPI, 20000000) && SD.cardType() != CARD_NONE)
        {
            const auto sampleTime = rtc.getTimeStruct();
            char path[32];
            char clock[9];
            strftime(path, sizeof(path), "/data/%Y-%m-%d.csv", &sampleTime);
            strftime(clock, sizeof(clock), "%H:%M:%S", &sampleTime);
            if (!SD.exists("/data"))
                SD.mkdir("/data");
            File log = SD.open(path, FILE_APPEND);
            if (log)
            {
                String row;
                row.reserve(256);
                if (log.size() == 0)
                {
                    row = "Time,T1,T2,T3";
                    for (const auto field : victronFields)
                    {
                        row += ',';
                        row += field.column;
                    }
                    row += '\n';
                }
                row += clock;
                for (const float temperature : {temperature1, temperature2, temperature3})
                {
                    row += ',';
                    if (temperature != DEVICE_DISCONNECTED_C && isfinite(temperature))
                        row += String(temperature, 1);
                }
                for (size_t i = 0; i < victronCount; ++i)
                {
                    row += ',';
                    const auto value = victron[i];
                    if (value != missingVictron)
                    {
                        if (victronFields[i].address == panelPower)
                            row += String(value / 100.0, 2);
                        else
                            row += String(value);
                    }
                }
                row += '\n';
                log.print(row);
            }
        }
    }

    // OLED
    static auto refreshTimer = millis();
    if (active && millis() - refreshTimer > refreshRate_ms)
    {
        refreshTimer = millis();
        // digitalWrite(OLED_Power_GPIO, HIGH);
        // display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // configure display Voltage
        // display.clearDisplay();
        // // Write Charge status
        // display.setTextSize(2); // Draw 2X-scale text
        // display.setTextColor(WHITE);
        // display.setCursor(10, 0);
        // display.println(F("Text"));
        // display.display();
    }
    else if (!active) // turn off OLED
    {
        digitalWrite(OLED_Power_GPIO, LOW);
    }
    // TODO abstract above to use display interface
    //{
    // Init()
    // writeText(solar status)
    // writeText(wheel status)
    // writeText(battery status)
    // writeText(Load status)
    // summery emoticon
    // } else {
    // off()
    // }

    // Sleep
    if (!active && (!wifiTimer || millis() - wifiTimer > WiFi_Hold_Time_ms))
    {
        if (WiFi.getMode() != WIFI_OFF)
        {
            WiFi.softAPdisconnect(true);
        }

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
    // Serial.println("Client Disconnected.");
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
    // Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        // Serial.println("Failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        // Serial.println("File written");
    }
    else
    {
        // Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
    // Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        // Serial.println("Failed to open file for appending");
        return;
    }
    if (file.print(message))
    {
        // Serial.println("Message appended");
    }
    else
    {
        // Serial.println("Append failed");
    }
    file.close();
}
// --- GZIP Compression Function ---
// Compresses a file from SD card and writes the compressed data to another file on SD.
// Returns true on successful compression, false otherwise.
bool gzipFile(const char *input_path, const char *output_path)
{
    File inputFile = SD.open(input_path, FILE_READ);
    if (!inputFile)
    {
        // Serial.printf("Error: Failed to open input file for reading: %s\n", input_path);
        return false;
    }
    if (inputFile.isDirectory())
    {
        // Serial.printf("Error: Input path %s is a directory, not a file. Cannot compress directories.\n", input_path);
        inputFile.close();
        return false;
    }

    File outputFile = SD.open(output_path, FILE_WRITE);
    if (!outputFile)
    {
        // Serial.printf("Error: Failed to open output file for writing: %s\n", output_path);
        inputFile.close(); // Close input file if output fails
        return false;
    }

    // Serial.printf("Starting compression: '%s' (Size: %lu bytes) -> '%s'\n", input_path, inputFile.size(), output_path);

    // Initialize the miniz stream structure
    mz_stream stream;
    memset(&stream, 0, sizeof(stream)); // Important: Zero out the stream structure

    // Initialize the DEFLATE compressor
    // Parameters:
    // 1. mz_stream*: Pointer to the stream structure.
    // 2. int level: Compression level (MZ_DEFAULT_COMPRESSION, 0-9).
    // 3. int method: Compression method (MZ_DEFLATED).
    // 4. int window_bits: Controls window size and output format.
    //    15: Default zlib window size.
    //    + 16: Adds GZIP header and footer. (i.e., 15 + 16 = 31)
    // 5. int mem_level: Memory usage level (1-9). Higher uses more RAM, generally faster.
    // 6. int strategy: Compression strategy (MZ_DEFAULT_STRATEGY).
    int ret = mz_deflateInit2(&stream, MZ_DEFAULT_COMPRESSION, MZ_DEFLATED, MZ_DEFAULT_WINDOW_BITS + 16, 9, MZ_DEFAULT_STRATEGY);
    if (ret != MZ_OK)
    {
        // Serial.printf("Error: mz_deflateInit2 failed with code: %d\n", ret);
        inputFile.close();
        outputFile.close();
        return false;
    }

    // Define input and output buffers
    // CHUNK_SIZE affects performance and memory usage. 4KB is a good general choice.
    const int CHUNK_SIZE = 4096;
    uint8_t in_buffer[CHUNK_SIZE];
    uint8_t out_buffer[CHUNK_SIZE];

    int flush_state = MZ_NO_FLUSH; // Initial flush state for mz_deflate
    unsigned long bytesReadTotal = 0;
    unsigned long bytesWrittenTotal = 0;

    // Main compression loop
    do
    {
        // Read data from input file if input buffer is empty
        if (stream.avail_in == 0 && inputFile.available())
        {
            size_t bytes_read = inputFile.read(in_buffer, CHUNK_SIZE);
            if (bytes_read < 0)
            { // Check for read error (read returns -1 on error)
                // Serial.println("Error: Failed to read from input file.");
                mz_deflateEnd(&stream);
                inputFile.close();
                outputFile.close();
                return false;
            }
            stream.avail_in = bytes_read;
            stream.next_in = in_buffer;
            bytesReadTotal += bytes_read;
        }

        // If no more data to read from input file, signal the end of stream to compressor
        if (stream.avail_in == 0 && !inputFile.available())
        {
            flush_state = MZ_FINISH;
        }

        // Set output buffer for compressor to write into
        stream.avail_out = CHUNK_SIZE;
        stream.next_out = out_buffer;

        // Perform the compression step
        ret = mz_deflate(&stream, flush_state);

        // Check for errors during compression
        // MZ_BUF_ERROR means output buffer was too small (shouldn't happen with correct loop logic)
        // MZ_STREAM_END is the success state when MZ_FINISH is signaled
        if (ret != MZ_OK && ret != MZ_BUF_ERROR && ret != MZ_STREAM_END)
        {
            // Serial.printf("Error: mz_deflate failed with code: %d\n", ret);
            mz_deflateEnd(&stream);
            inputFile.close();
            outputFile.close();
            return false;
        }

        // Write compressed data from output buffer to the output file
        size_t bytes_produced = CHUNK_SIZE - stream.avail_out;
        if (bytes_produced > 0)
        {
            outputFile.write(out_buffer, bytes_produced);
            bytesWrittenTotal += bytes_produced;
        }

        // Continue loop until all input is processed (flush_state is MZ_FINISH)
        // AND the compressor has finished outputting everything (ret is MZ_STREAM_END).
    } while (flush_state != MZ_FINISH || ret != MZ_STREAM_END);

    // Clean up compressor resources
    mz_deflateEnd(&stream);

    inputFile.close();
    outputFile.close();

    // Final check for successful compression completion
    if (ret == MZ_STREAM_END)
    {
        // Serial.printf("Compression successful! Original size: %lu bytes, Compressed size: %lu bytes.\n", bytesReadTotal, bytesWrittenTotal);
        return true;
    }
    else
    {
        // This case indicates an unexpected termination or error not caught earlier
        // Serial.printf("Compression finished with unexpected status: %d (Expected MZ_STREAM_END).\n", ret);
        return false;
    }
}
