#ifndef DEFAULT_CONFIG_H
#define DEFAULT_CONFIG_H

/**
 * default_config.h
 * Compile-time defaults for Open ESP Tracker firmware.
 *
 * These values are used when no configuration has been saved to NVS, or as
 * fallback values.  Override them at runtime through the Interactive Serial
 * Interface (ISI) – changes are persisted in NVS Preferences.
 */

// --------------------------------------------------------------------------
// Interactive Serial Interface (ISI)
// --------------------------------------------------------------------------

/** Set to true to bypass the password prompt on the ISI (development only). */
#define SKIP_PASSWORD_SETUP false

// --------------------------------------------------------------------------
// Debug
// --------------------------------------------------------------------------

/** Set to true to print verbose diagnostic messages over USB Serial. */
#define DEBUG_MESSAGES false

// --------------------------------------------------------------------------
// Server
// --------------------------------------------------------------------------

/** Base URL of the tracking back-end (no trailing slash). */
#define DEFAULT_SERVER_URL  "https://your-server.example.com"

/** HTTPS port of the tracking back-end. */
#define DEFAULT_SERVER_PORT 443

/** Bearer token sent in the Authorization header. */
#define DEFAULT_API_TOKEN   ""

// --------------------------------------------------------------------------
// Cellular
// --------------------------------------------------------------------------

/** APN of the SIM card's mobile data network. */
#define DEFAULT_APN "internet"

// --------------------------------------------------------------------------
// Tracking behaviour
// --------------------------------------------------------------------------

/** Seconds the device sleeps between location reports. */
#define DEFAULT_WAKEUP_INTERVAL_SEC 60

/**
 * Minimum GPS horizontal accuracy (metres) required before the fix is
 * accepted and transmitted.
 */
#define DEFAULT_GPS_ACCURACY_THRESHOLD 20.0f

/**
 * Speed (m/s) below which the device is considered stationary.
 * When stationary the report interval may be extended to save battery.
 */
#define DEFAULT_SPEED_THRESHOLD 0.5f

/** Battery percentage below which a "low battery" flag is set in the payload. */
#define DEFAULT_BATTERY_LOW_THRESHOLD 20

/** Maximum seconds to wait for a GPS fix before giving up and going to sleep. */
#define DEFAULT_GPS_WAIT_SEC 60

// --------------------------------------------------------------------------
// NVS
// --------------------------------------------------------------------------

/** NVS Preferences namespace used to persist all runtime configuration. */
#define NVS_NAMESPACE "tracker"

// --------------------------------------------------------------------------
// Firmware metadata
// --------------------------------------------------------------------------

#define FIRMWARE_VERSION "1.0.0"

#endif // DEFAULT_CONFIG_H
