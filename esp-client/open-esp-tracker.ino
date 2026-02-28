/**
 * open-esp-tracker.ino
 *
 * Main firmware for the Open ESP Tracker
 * Hardware: Waveshare ESP32-S3 + A7670E 4G/GPS module
 *
 * Behaviour (each wake cycle):
 *   1. Boot LED white.
 *   2. If USB Serial becomes active within 3 s → enter Interactive Serial
 *      Interface (ISI) for configuration.
 *   3. Load config from NVS (falls back to compile-time defaults).
 *   4. Power up A7670E modem, attach to cellular network.
 *   5. Acquire GPS fix (timeout = config.gpsWaitSec).
 *   6. Read battery voltage/percentage.
 *   7. POST JSON payload to server via HTTPS.
 *   8. Power down modem, set LED off, enter deep sleep.
 *
 * RGB LED (WS2812B on GPIO 48) colour codes:
 *   White  – booting
 *   Yellow – searching for GPS fix
 *   Green  – GPS fix acquired
 *   Blue   – transmitting data
 *   Purple – ISI / configuration mode
 *   Red    – error
 */

// ==========================================================================
// 1. INCLUDES AND DEFINES
// ==========================================================================

#include "default_config.h"

// TinyGSM – must be defined before the header is included
#define TINY_GSM_MODEM_A7670          // Select A7670E modem driver
#define TINY_GSM_RX_BUFFER 1024       // Increase RX buffer for large responses
#include <TinyGsmClient.h>

#include <ArduinoJson.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

// --------------------------------------------------------------------------
// Pin assignments (Waveshare ESP32-S3 + A7670E carrier board)
// --------------------------------------------------------------------------
#define MODEM_TX_PIN   17   // ESP32-S3 TX → A7670E RX
#define MODEM_RX_PIN   18   // ESP32-S3 RX → A7670E TX
#define MODEM_BAUD     115200

#define VBAT_ADC_PIN   1    // Resistor-divider battery ADC input
#define LED_PIN        48   // Onboard WS2812B RGB LED
#define LED_COUNT      1

// ADC-to-voltage calibration constants.
// Adjust VBAT_DIVIDER_RATIO to match the actual resistor divider on the PCB.
#define ADC_REF_VOLTAGE   3.3f   // ESP32-S3 ADC reference (V)
#define ADC_RESOLUTION    4095.0f
#define VBAT_DIVIDER_RATIO 2.0f  // e.g. two equal resistors → ratio = 2
#define VBAT_FULL_VOLTAGE  4.2f  // LiPo fully-charged voltage
#define VBAT_EMPTY_VOLTAGE 3.3f  // LiPo cut-off voltage

// Serial port connected to the A7670E
#define SerialAT Serial1

// Convenience macro for conditional debug printing
#if DEBUG_MESSAGES
  #define DBG(x)   Serial.print(x)
  #define DBGLN(x) Serial.println(x)
#else
  #define DBG(x)
  #define DBGLN(x)
#endif

// ==========================================================================
// 2. GLOBAL VARIABLES AND STRUCTS
// ==========================================================================

/** Runtime configuration – loaded from NVS, falls back to compile-time defaults. */
struct Config {
    char    serverUrl[128];
    int     serverPort;
    char    apiToken[128];
    char    apn[32];
    int     wakeupIntervalSec;
    float   gpsAccuracyThreshold;
    float   speedThreshold;
    int     batteryLowThreshold;
    int     gpsWaitSec;
};

/** GPS fix data returned by the A7670E. */
struct GpsData {
    bool    valid;
    float   latitude;
    float   longitude;
    float   altitude;
    float   speed;       // km/h from GNSS; converted to m/s before sending
    float   accuracy;    // Estimated horizontal accuracy (m) – derived from HDOP
    int     satellites;
    float   hdop;
    char    timestamp[32]; // ISO-8601 UTC timestamp
};

Config      g_config;
Preferences g_prefs;
Adafruit_NeoPixel g_led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
TinyGsm     g_modem(SerialAT);

// ==========================================================================
// 3. FUNCTION PROTOTYPES
// ==========================================================================

// LED helpers
void ledColor(uint8_t r, uint8_t g, uint8_t b);
void ledOff();

// Battery
float   readBatteryVoltage();
uint8_t voltageToPercent(float voltage);

// NVS config
void loadConfig();
void saveConfig();

// ISI
bool    checkIsiRequested();
void    runIsi();
void    isiHelp();
void    isiStatus();
void    isiSet(const String& param, const String& value);
void    isiGet(const String& param);
String  readIsiLine(unsigned long timeoutMs = 30000UL);

// Modem / cellular
bool    modemInit();
bool    modemConnectGprs();
void    modemPowerOff();

// GPS
bool    getGpsFix(GpsData& data);

// HTTP transmission
bool    sendData(const GpsData& gps, float battVoltage, uint8_t battPercent);

// ==========================================================================
// 4. HELPER FUNCTIONS
// ==========================================================================

// --------------------------------------------------------------------------
// LED control
// --------------------------------------------------------------------------

/** Set the onboard WS2812B to the given RGB colour. */
void ledColor(uint8_t r, uint8_t g, uint8_t b) {
    g_led.setPixelColor(0, g_led.Color(r, g, b));
    g_led.show();
}

/** Turn the LED off. */
void ledOff() { ledColor(0, 0, 0); }

// Colour presets used throughout the firmware
#define LED_WHITE()   ledColor(30, 30, 30)
#define LED_YELLOW()  ledColor(30, 20,  0)
#define LED_GREEN()   ledColor( 0, 30,  0)
#define LED_BLUE()    ledColor( 0,  0, 30)
#define LED_PURPLE()  ledColor(20,  0, 20)
#define LED_RED()     ledColor(30,  0,  0)

// --------------------------------------------------------------------------
// Battery measurement
// --------------------------------------------------------------------------

/**
 * Read battery voltage via the resistor-divider on VBAT_ADC_PIN.
 * Returns the actual battery voltage (before the divider).
 */
float readBatteryVoltage() {
    // Average 8 samples to reduce noise
    uint32_t raw = 0;
    for (int i = 0; i < 8; i++) {
        raw += analogRead(VBAT_ADC_PIN);
        delay(5);
    }
    float adcVoltage = (raw / 8.0f) * (ADC_REF_VOLTAGE / ADC_RESOLUTION);
    return adcVoltage * VBAT_DIVIDER_RATIO;
}

/**
 * Map battery voltage linearly to a 0–100 % estimate.
 * Values outside the [empty, full] range are clamped.
 */
uint8_t voltageToPercent(float voltage) {
    if (voltage >= VBAT_FULL_VOLTAGE)  return 100;
    if (voltage <= VBAT_EMPTY_VOLTAGE) return 0;
    float pct = (voltage - VBAT_EMPTY_VOLTAGE) /
                (VBAT_FULL_VOLTAGE - VBAT_EMPTY_VOLTAGE) * 100.0f;
    return (uint8_t)pct;
}

// --------------------------------------------------------------------------
// NVS configuration load / save
// --------------------------------------------------------------------------

/** Load configuration from NVS.  Missing keys fall back to compile-time defaults. */
void loadConfig() {
    g_prefs.begin(NVS_NAMESPACE, true); // read-only

    strncpy(g_config.serverUrl, g_prefs.getString("server_url",
            DEFAULT_SERVER_URL).c_str(), sizeof(g_config.serverUrl) - 1);

    g_config.serverPort = g_prefs.getInt("server_port", DEFAULT_SERVER_PORT);

    strncpy(g_config.apiToken, g_prefs.getString("api_token",
            DEFAULT_API_TOKEN).c_str(), sizeof(g_config.apiToken) - 1);

    strncpy(g_config.apn, g_prefs.getString("apn",
            DEFAULT_APN).c_str(), sizeof(g_config.apn) - 1);

    g_config.wakeupIntervalSec    = g_prefs.getInt  ("interval",    DEFAULT_WAKEUP_INTERVAL_SEC);
    g_config.gpsAccuracyThreshold = g_prefs.getFloat("accuracy",    DEFAULT_GPS_ACCURACY_THRESHOLD);
    g_config.speedThreshold       = g_prefs.getFloat("speed_thr",   DEFAULT_SPEED_THRESHOLD);
    g_config.batteryLowThreshold  = g_prefs.getInt  ("batt_low",    DEFAULT_BATTERY_LOW_THRESHOLD);
    g_config.gpsWaitSec           = g_prefs.getInt  ("gps_wait",    DEFAULT_GPS_WAIT_SEC);

    g_prefs.end();
}

/** Persist the current in-memory configuration to NVS. */
void saveConfig() {
    g_prefs.begin(NVS_NAMESPACE, false); // read-write

    g_prefs.putString("server_url",  g_config.serverUrl);
    g_prefs.putInt   ("server_port", g_config.serverPort);
    g_prefs.putString("api_token",   g_config.apiToken);
    g_prefs.putString("apn",         g_config.apn);
    g_prefs.putInt   ("interval",    g_config.wakeupIntervalSec);
    g_prefs.putFloat ("accuracy",    g_config.gpsAccuracyThreshold);
    g_prefs.putFloat ("speed_thr",   g_config.speedThreshold);
    g_prefs.putInt   ("batt_low",    g_config.batteryLowThreshold);
    g_prefs.putInt   ("gps_wait",    g_config.gpsWaitSec);

    g_prefs.end();
}

// ==========================================================================
// 5. INTERACTIVE SERIAL INTERFACE (ISI)
// ==========================================================================

/**
 * Check whether the user has activated the ISI.
 * Watches USB Serial for any incoming byte during the first 3 seconds of boot.
 * Returns true if activity is detected.
 */
bool checkIsiRequested() {
    unsigned long deadline = millis() + 3000UL;
    while (millis() < deadline) {
        if (Serial.available()) return true;
        delay(10);
    }
    return false;
}

/**
 * Read a line from USB Serial, echoing each character back.
 * Blocks until '\n' / '\r' is received or timeoutMs elapses.
 */
String readIsiLine(unsigned long timeoutMs) {
    String line;
    unsigned long deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        if (Serial.available()) {
            char c = (char)Serial.read();
            if (c == '\n' || c == '\r') {
                if (line.length() > 0) {
                    Serial.println(); // move to new line after Enter
                    return line;
                }
            } else if (c == 127 || c == '\b') { // backspace
                if (line.length() > 0) {
                    line.remove(line.length() - 1);
                    Serial.print("\b \b");
                }
            } else {
                line += c;
                Serial.print(c); // echo
            }
        }
        delay(5);
    }
    return line;
}

/** Read a line from USB Serial with characters masked as '*' (for passwords). */
String readIsiPassword(unsigned long timeoutMs = 30000UL) {
    String pwd;
    unsigned long deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        if (Serial.available()) {
            char c = (char)Serial.read();
            if (c == '\n' || c == '\r') {
                if (pwd.length() > 0) {
                    Serial.println();
                    return pwd;
                }
            } else if (c == 127 || c == '\b') {
                if (pwd.length() > 0) {
                    pwd.remove(pwd.length() - 1);
                    Serial.print("\b \b");
                }
            } else {
                pwd += c;
                Serial.print('*');
            }
        }
        delay(5);
    }
    return pwd;
}

/** Print the ISI help text. */
void isiHelp() {
    Serial.println(F("\r\n--- Open ESP Tracker ISI Help ---"));
    Serial.println(F("Commands:"));
    Serial.println(F("  help                    Show this help"));
    Serial.println(F("  status                  Show current configuration and device status"));
    Serial.println(F("  get <param>             Get a configuration value"));
    Serial.println(F("  set <param> <value>     Set a configuration value (saved to NVS)"));
    Serial.println(F("  reset                   Reset all settings to compile-time defaults"));
    Serial.println(F("  reboot                  Reboot the device"));
    Serial.println(F("  exit                    Exit ISI and continue normal operation"));
    Serial.println(F("\r\nSettable parameters:"));
    Serial.println(F("  server_url         HTTPS server base URL"));
    Serial.println(F("  server_port        Server port (integer)"));
    Serial.println(F("  api_token          Bearer token for authentication"));
    Serial.println(F("  apn                Cellular APN"));
    Serial.println(F("  interval           Wake-up interval in seconds"));
    Serial.println(F("  accuracy           GPS accuracy threshold in metres"));
    Serial.println(F("  speed_threshold    Speed threshold for movement detection (m/s)"));
    Serial.println(F("  battery_low        Battery low threshold in percent"));
    Serial.println(F("  gps_wait           Max seconds to wait for a GPS fix (not settable via set)"));
}

/** Print current config and live device status. */
void isiStatus() {
    float bv  = readBatteryVoltage();
    uint8_t bp = voltageToPercent(bv);

    Serial.println(F("\r\n--- Device Status ---"));
    Serial.printf (  "  Firmware version   : %s\r\n", FIRMWARE_VERSION);
    Serial.printf (  "  Battery voltage    : %.2f V\r\n", bv);
    Serial.printf (  "  Battery level      : %d %%\r\n",  bp);
    Serial.println(F("\r\n--- Configuration ---"));
    Serial.printf (  "  server_url         : %s\r\n", g_config.serverUrl);
    Serial.printf (  "  server_port        : %d\r\n", g_config.serverPort);
    Serial.printf (  "  api_token          : %s\r\n",
                     strlen(g_config.apiToken) > 0 ? "****" : "(not set)");
    Serial.printf (  "  apn                : %s\r\n", g_config.apn);
    Serial.printf (  "  interval           : %d s\r\n", g_config.wakeupIntervalSec);
    Serial.printf (  "  accuracy           : %.1f m\r\n", g_config.gpsAccuracyThreshold);
    Serial.printf (  "  speed_threshold    : %.2f m/s\r\n", g_config.speedThreshold);
    Serial.printf (  "  battery_low        : %d %%\r\n", g_config.batteryLowThreshold);
    Serial.printf (  "  gps_wait           : %d s\r\n", g_config.gpsWaitSec);
}

/** Apply a `set` command: update g_config and persist to NVS. */
void isiSet(const String& param, const String& value) {
    if (param == "server_url") {
        strncpy(g_config.serverUrl, value.c_str(), sizeof(g_config.serverUrl) - 1);
    } else if (param == "server_port") {
        g_config.serverPort = value.toInt();
    } else if (param == "api_token") {
        strncpy(g_config.apiToken, value.c_str(), sizeof(g_config.apiToken) - 1);
    } else if (param == "apn") {
        strncpy(g_config.apn, value.c_str(), sizeof(g_config.apn) - 1);
    } else if (param == "interval") {
        g_config.wakeupIntervalSec = value.toInt();
    } else if (param == "accuracy") {
        g_config.gpsAccuracyThreshold = value.toFloat();
    } else if (param == "speed_threshold") {
        g_config.speedThreshold = value.toFloat();
    } else if (param == "battery_low") {
        g_config.batteryLowThreshold = value.toInt();
    } else {
        Serial.printf("Unknown parameter: %s\r\n", param.c_str());
        return;
    }
    saveConfig();
    Serial.printf("  %s = %s  (saved)\r\n", param.c_str(), value.c_str());
}

/** Print the current value of a single configuration parameter. */
void isiGet(const String& param) {
    if      (param == "server_url")      Serial.printf("  server_url       = %s\r\n", g_config.serverUrl);
    else if (param == "server_port")     Serial.printf("  server_port      = %d\r\n", g_config.serverPort);
    else if (param == "api_token")       Serial.printf("  api_token        = %s\r\n",
                                                        strlen(g_config.apiToken) > 0 ? "****" : "(not set)");
    else if (param == "apn")             Serial.printf("  apn              = %s\r\n", g_config.apn);
    else if (param == "interval")        Serial.printf("  interval         = %d s\r\n", g_config.wakeupIntervalSec);
    else if (param == "accuracy")        Serial.printf("  accuracy         = %.1f m\r\n", g_config.gpsAccuracyThreshold);
    else if (param == "speed_threshold") Serial.printf("  speed_threshold  = %.2f m/s\r\n", g_config.speedThreshold);
    else if (param == "battery_low")     Serial.printf("  battery_low      = %d %%\r\n", g_config.batteryLowThreshold);
    else                                 Serial.printf("Unknown parameter: %s\r\n", param.c_str());
}

/** Reset all NVS keys to compile-time defaults and reload g_config. */
void isiResetDefaults() {
    g_prefs.begin(NVS_NAMESPACE, false);
    g_prefs.clear();
    g_prefs.end();
    loadConfig(); // repopulates g_config from defaults
    Serial.println(F("  All settings reset to defaults."));
}

/**
 * Full ISI session.
 * Handles password setup/verification, then enters command loop.
 */
void runIsi() {
    LED_PURPLE();

    Serial.println(F("\r\n\r\n============================================="));
    Serial.println(F("  Open ESP Tracker  –  Interactive Serial Interface"));
    Serial.printf (  "  Firmware: %s\r\n", FIRMWARE_VERSION);
    Serial.println(F("============================================="));

    // ---- Password handling ----
#if !SKIP_PASSWORD_SETUP
    g_prefs.begin(NVS_NAMESPACE, true);
    String storedPwd = g_prefs.getString("isi_pwd", "");
    g_prefs.end();

    if (storedPwd.length() == 0) {
        // No password set yet – prompt the user to create one
        Serial.println(F("\r\nNo ISI password is set.  Please create one now."));
        Serial.print(F("New password: "));
        String pwd1 = readIsiPassword();
        Serial.print(F("Confirm password: "));
        String pwd2 = readIsiPassword();

        if (pwd1 != pwd2 || pwd1.length() == 0) {
            Serial.println(F("Passwords do not match or are empty.  Aborting ISI."));
            return;
        }
        g_prefs.begin(NVS_NAMESPACE, false);
        g_prefs.putString("isi_pwd", pwd1);
        g_prefs.end();
        Serial.println(F("Password saved."));
    } else {
        // Verify password (3 attempts)
        bool authenticated = false;
        for (int attempt = 0; attempt < 3; attempt++) {
            Serial.print(F("Password: "));
            String entered = readIsiPassword();
            if (entered == storedPwd) {
                authenticated = true;
                break;
            }
            Serial.println(F("Incorrect password."));
        }
        if (!authenticated) {
            Serial.println(F("Authentication failed.  Continuing normal operation."));
            return;
        }
    }
#endif // SKIP_PASSWORD_SETUP

    Serial.println(F("\r\nAuthenticated.  Type 'help' for available commands.\r\n"));

    // Pre-load config so ISI can display/modify it
    loadConfig();

    // ---- Command loop ----
    while (true) {
        Serial.print(F("tracker> "));
        String line = readIsiLine();
        line.trim();
        if (line.length() == 0) continue;

        // Split into tokens: cmd [arg1 [rest...]]
        int firstSpace  = line.indexOf(' ');
        String cmd      = (firstSpace < 0) ? line : line.substring(0, firstSpace);
        String args     = (firstSpace < 0) ? String() : line.substring(firstSpace + 1);
        args.trim();

        int secondSpace = args.indexOf(' ');
        String arg1     = (secondSpace < 0) ? args : args.substring(0, secondSpace);
        String arg2     = (secondSpace < 0) ? String() : args.substring(secondSpace + 1);
        arg2.trim();

        cmd.toLowerCase();

        if (cmd == "help") {
            isiHelp();
        } else if (cmd == "status") {
            isiStatus();
        } else if (cmd == "set") {
            if (arg1.length() == 0 || arg2.length() == 0) {
                Serial.println(F("Usage: set <param> <value>"));
            } else {
                isiSet(arg1, arg2);
            }
        } else if (cmd == "get") {
            if (arg1.length() == 0) {
                Serial.println(F("Usage: get <param>"));
            } else {
                isiGet(arg1);
            }
        } else if (cmd == "reset") {
            isiResetDefaults();
        } else if (cmd == "reboot") {
            Serial.println(F("Rebooting..."));
            delay(500);
            esp_restart();
        } else if (cmd == "exit") {
            Serial.println(F("Exiting ISI.  Continuing normal operation.\r\n"));
            break;
        } else {
            Serial.printf("Unknown command: %s  (type 'help' for a list)\r\n", cmd.c_str());
        }
    }
}

// ==========================================================================
// 6. CELLULAR / MODEM FUNCTIONS
// ==========================================================================

/**
 * Initialise UART to the A7670E and perform a basic modem AT handshake.
 * Returns true if the modem responds within the timeout.
 */
bool modemInit() {
    DBGLN(F("[Modem] Initialising UART..."));
    SerialAT.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    delay(1000);

    DBGLN(F("[Modem] Waiting for AT response..."));
    for (int attempt = 0; attempt < 10; attempt++) {
        if (g_modem.testAT(1000)) {
            DBGLN(F("[Modem] AT OK"));
            return true;
        }
        delay(500);
    }
    DBGLN(F("[Modem] No AT response!"));
    return false;
}

/**
 * Connect to the cellular data network using the configured APN.
 * Returns true on successful GPRS/LTE attach.
 */
bool modemConnectGprs() {
    DBGLN(F("[Modem] Waiting for network registration..."));
    if (!g_modem.waitForNetwork(60000L)) {
        DBGLN(F("[Modem] Network registration failed"));
        return false;
    }
    DBGLN(F("[Modem] Registered.  Connecting GPRS..."));
    if (!g_modem.gprsConnect(g_config.apn, "", "")) {
        DBGLN(F("[Modem] GPRS connect failed"));
        return false;
    }
    DBGLN(F("[Modem] GPRS connected"));
    return true;
}

/** Gracefully disconnect from GPRS and power down the modem. */
void modemPowerOff() {
    g_modem.gprsDisconnect();
    g_modem.poweroff();
    DBGLN(F("[Modem] Powered off"));
}

// ==========================================================================
// 7. GPS FUNCTIONS
// ==========================================================================

/**
 * Enable the A7670E built-in GNSS engine, then poll for a valid fix.
 * Populates `data` and returns true when a fix meeting the accuracy
 * threshold is obtained, or false if the timeout expires first.
 */
bool getGpsFix(GpsData& data) {
    data.valid = false;

    DBGLN(F("[GPS] Enabling GNSS..."));
    g_modem.enableGPS();
    delay(2000);

    unsigned long deadline = millis() + (unsigned long)g_config.gpsWaitSec * 1000UL;

    while (millis() < deadline) {
        float lat = 0, lon = 0, alt = 0, spd = 0, hdop = 0, accuracy = 0;
        int   vsat = 0, usat = 0;

        // TinyGSM returns speed in km/h and HDOP (not raw accuracy).
        // We derive an approximate accuracy in metres from HDOP * CEP_CONSTANT.
        // The extended getGPS() overload also fills in UTC date/time directly.
        int yr = 0, mo = 0, dy = 0, hr = 0, mn = 0, sc = 0;
        if (g_modem.getGPS(&lat, &lon, &spd, &alt, &vsat, &usat, &hdop,
                           &yr, &mo, &dy, &hr, &mn, &sc)) {
            // Typical Urban CEP constant ≈ 5 m; accuracy = hdop * 5
            accuracy = hdop * 5.0f;

            if (accuracy <= g_config.gpsAccuracyThreshold) {
                // Build ISO-8601 UTC timestamp from values returned by getGPS()
                if (yr > 0) {
                    snprintf(data.timestamp, sizeof(data.timestamp),
                             "%04d-%02d-%02dT%02d:%02d:%02dZ",
                             yr, mo, dy, hr, mn, sc);
                } else {
                    strncpy(data.timestamp, "1970-01-01T00:00:00Z",
                            sizeof(data.timestamp));
                }

                data.valid      = true;
                data.latitude   = lat;
                data.longitude  = lon;
                data.altitude   = alt;
                data.speed      = spd / 3.6f; // km/h → m/s
                data.accuracy   = accuracy;
                data.satellites = usat;
                data.hdop       = hdop;

                DBGLN(F("[GPS] Fix acquired"));
                g_modem.disableGPS();
                return true;
            }
        }
        delay(2000);
    }

    DBGLN(F("[GPS] Timeout – no fix"));
    g_modem.disableGPS();
    return false;
}

// ==========================================================================
// 8. HTTP TRANSMISSION FUNCTION
// ==========================================================================

/**
 * Serialise the GPS fix and device telemetry into a JSON object and POST
 * it to the configured server endpoint via HTTPS.
 *
 * Returns true if the server responded with HTTP 2xx.
 */
bool sendData(const GpsData& gps, float battVoltage, uint8_t battPercent) {
    // ---- Build JSON payload ----
    JsonDocument doc;

    // Derive a unique device ID from the ESP32 MAC address
    uint64_t mac = ESP.getEfuseMac();
    char deviceId[18];
    snprintf(deviceId, sizeof(deviceId), "%02X%02X%02X%02X%02X%02X",
             (uint8_t)(mac >> 40), (uint8_t)(mac >> 32), (uint8_t)(mac >> 24),
             (uint8_t)(mac >> 16), (uint8_t)(mac >>  8), (uint8_t)(mac));

    doc["device_id"]        = deviceId;
    doc["recorded_at"]      = gps.timestamp;
    doc["latitude"]         = gps.latitude;
    doc["longitude"]        = gps.longitude;
    doc["altitude"]         = gps.altitude;
    doc["speed"]            = gps.speed;
    doc["accuracy"]         = gps.accuracy;
    doc["battery_level"]    = battPercent;
    doc["battery_voltage"]  = battVoltage;
    doc["battery_low"]      = (battPercent <= (uint8_t)g_config.batteryLowThreshold);
    doc["satellites"]       = gps.satellites;
    doc["hdop"]             = gps.hdop;
    doc["firmware_version"] = FIRMWARE_VERSION;

    String payload;
    serializeJson(doc, payload);

    DBG(F("[HTTP] Payload: ")); DBGLN(payload);

    // ---- Set up TinyGSM HTTPS client ----
    TinyGsmClientSecure secureClient(g_modem);
    if (!secureClient.connect(g_config.serverUrl, g_config.serverPort)) {
        DBGLN(F("[HTTP] Connection failed"));
        return false;
    }

    // Build the endpoint path – POST to /api/v1/locations
    String path = F("/api/v1/locations");

    // ---- Send HTTP POST ----
    secureClient.printf("POST %s HTTP/1.1\r\n", path.c_str());
    secureClient.printf("Host: %s\r\n", g_config.serverUrl);
    secureClient.print (F("Content-Type: application/json\r\n"));
    if (strlen(g_config.apiToken) > 0) {
        secureClient.printf("Authorization: Bearer %s\r\n", g_config.apiToken);
    }
    secureClient.printf("Content-Length: %d\r\n", (int)payload.length());
    secureClient.print (F("Connection: close\r\n\r\n"));
    secureClient.print (payload);

    // ---- Read response status line ----
    unsigned long httpDeadline = millis() + 10000UL;
    String statusLine;
    while (secureClient.connected() && millis() < httpDeadline) {
        if (secureClient.available()) {
            statusLine = secureClient.readStringUntil('\n');
            break;
        }
        delay(50);
    }
    secureClient.stop();

    DBG(F("[HTTP] Response: ")); DBGLN(statusLine);

    // HTTP/1.1 2xx means success
    return statusLine.indexOf("200") >= 0 ||
           statusLine.indexOf("201") >= 0 ||
           statusLine.indexOf("204") >= 0;
}

// ==========================================================================
// 9. SETUP AND LOOP
// ==========================================================================

void setup() {
    // ---- Serial init ----
    Serial.begin(115200);
    // Brief wait so USB CDC enumerates (ESP32-S3 native USB)
    delay(300);

    // ---- LED init ----
    g_led.begin();
    g_led.setBrightness(50);
    LED_WHITE();  // Booting

    // ---- ADC init ----
    analogReadResolution(12);

    // ---- Check for ISI request ----
    // If any byte arrives on USB Serial within 3 s, enter config mode.
    if (checkIsiRequested()) {
        runIsi();
    }

    // ---- Load runtime configuration from NVS ----
    loadConfig();

    // ---- Modem initialisation ----
    LED_YELLOW();  // Searching GPS / setting up
    bool modemOk = modemInit();
    if (!modemOk) {
        LED_RED();
        DBGLN(F("[Main] Modem init failed – sleeping"));
        delay(1000);
        // Go to sleep and try again next cycle
        esp_deep_sleep((uint64_t)g_config.wakeupIntervalSec * 1000000ULL);
        return;
    }

    // ---- Connect to cellular data network ----
    if (!modemConnectGprs()) {
        LED_RED();
        DBGLN(F("[Main] GPRS failed – sleeping"));
        modemPowerOff();
        delay(500);
        esp_deep_sleep((uint64_t)g_config.wakeupIntervalSec * 1000000ULL);
        return;
    }

    // ---- Acquire GPS fix ----
    LED_YELLOW();
    GpsData gps;
    bool fixOk = getGpsFix(gps);

    if (fixOk) {
        LED_GREEN();
        DBGLN(F("[Main] GPS fix OK"));
    } else {
        // Transmit a "no fix" report so the server knows the device is alive
        DBGLN(F("[Main] No GPS fix – sending heartbeat"));
        memset(&gps, 0, sizeof(gps));
        gps.valid = false;
        strncpy(gps.timestamp, "1970-01-01T00:00:00Z", sizeof(gps.timestamp));
    }

    // ---- Read battery ----
    float battV   = readBatteryVoltage();
    uint8_t battP = voltageToPercent(battV);
    if (battP <= (uint8_t)g_config.batteryLowThreshold) {
        DBG(F("[Main] Low battery: ")); DBGLN(battP);
    }

    // ---- Transmit data ----
    LED_BLUE();
    bool sent = sendData(gps, battV, battP);
    if (sent) {
        LED_GREEN();
        DBGLN(F("[Main] Data sent OK"));
    } else {
        LED_RED();
        DBGLN(F("[Main] Data send failed"));
    }

    // ---- Clean up ----
    delay(500); // Let LED be visible briefly
    modemPowerOff();
    ledOff();

    // ---- Deep sleep until next reporting interval ----
    DBGLN(F("[Main] Entering deep sleep"));
    esp_deep_sleep((uint64_t)g_config.wakeupIntervalSec * 1000000ULL);
}

/**
 * loop() is never reached: setup() always ends with esp_deep_sleep() which
 * resets the chip.  The function exists only to satisfy the Arduino framework.
 */
void loop() {}
