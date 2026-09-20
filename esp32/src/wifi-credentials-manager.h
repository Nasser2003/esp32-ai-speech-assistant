#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include <string>

//----------------------------------------------------------
// STRUCT --------------------------------------------------
//----------------------------------------------------------

struct WifiCredential {
    std::string ssid;
    std::string password;
};

//----------------------------------------------------------
// CLASS ---------------------------------------------------
//----------------------------------------------------------

/**
 * WifiCredentialsManager
 *
 * Stores up to MAX_CREDENTIALS WiFi credentials in NVS (namespace "wifi_creds").
 * The list is ordered by recency: index 0 is the most recently used network.
 * - add()     : adds a new credential or updates an existing one (by SSID),
 *               then moves it to the front of the list (LRU).
 * - promote() : marks a SSID as successfully used, moving it to index 0.
 * - getAll()  : returns credentials ordered from most-recently-used to oldest.
 * - clear()   : wipes all stored credentials from NVS.
 *
 * NVS layout (namespace "wifi_creds"):
 *   "count"   -> uint8  : number of stored credentials
 *   "ssid_N"  -> string : SSID at position N
 *   "pass_N"  -> string : password at position N
 */
class WifiCredentialsManager {
public:
    static constexpr int MAX_CREDENTIALS = 10;

    /**
     * Loads credentials from NVS into memory.
     * Must be called once before using getAll() / add() / promote().
     * @return true if at least one credential was loaded.
     */
    bool load() {
        _credentials.clear();
        Preferences prefs;
        prefs.begin("wifi_creds", true); // read-only

        uint8_t count = prefs.getUChar("count", 0);
        Serial.printf("[WifiCreds] Loading %d credential(s) from NVS\n", count);

        for (uint8_t i = 0; i < count && i < MAX_CREDENTIALS; i++) {
            String ssidKey = "ssid_" + String(i);
            String passKey = "pass_" + String(i);

            String ssid = prefs.getString(ssidKey.c_str(), "");
            String pass = prefs.getString(passKey.c_str(), "");

            if (ssid.length() > 0) {
                _credentials.push_back({ ssid.c_str(), pass.c_str() });
                Serial.printf("[WifiCreds]   [%d] SSID: %s\n", i, ssid.c_str());
            }
        }

        prefs.end();
        return !_credentials.empty();
    }

    /**
     * Adds or updates a credential by SSID.
     * If the SSID already exists, its password is updated and it is moved to index 0.
     * If it is a new SSID and the list is full, the oldest entry (last index) is removed.
     * Persists the updated list to NVS.
     */
    void add(const std::string& ssid, const std::string& password) {
        if (ssid.empty()) return;

        // Remove existing entry with same SSID (case-sensitive)
        for (auto it = _credentials.begin(); it != _credentials.end(); ++it) {
            if (it->ssid == ssid) {
                _credentials.erase(it);
                Serial.printf("[WifiCreds] Updated existing credential: %s\n", ssid.c_str());
                break;
            }
        }

        // If at capacity, drop the oldest (last) entry
        if ((int)_credentials.size() >= MAX_CREDENTIALS) {
            Serial.printf("[WifiCreds] At capacity (%d), removing oldest: %s\n",
                MAX_CREDENTIALS, _credentials.back().ssid.c_str());
            _credentials.pop_back();
        }

        // Insert as most-recently-used (front)
        _credentials.insert(_credentials.begin(), { ssid, password });
        Serial.printf("[WifiCreds] Added credential at index 0: %s\n", ssid.c_str());

        _save();
    }

    /**
     * Moves the credential with the given SSID to index 0 (most-recently-used).
     * Call this after a successful WiFi connection.
     * No-op if SSID is not found.
     */
    void promote(const std::string& ssid) {
        for (auto it = _credentials.begin(); it != _credentials.end(); ++it) {
            if (it->ssid == ssid) {
                if (it == _credentials.begin()) return; // already at front
                WifiCredential cred = *it;
                _credentials.erase(it);
                _credentials.insert(_credentials.begin(), cred);
                Serial.printf("[WifiCreds] Promoted to index 0: %s\n", ssid.c_str());
                _save();
                return;
            }
        }
    }

    /**
     * Returns the list of credentials ordered from most-recently-used (index 0) to oldest.
     */
    const std::vector<WifiCredential>& getAll() const {
        return _credentials;
    }

    /** Returns the number of stored credentials. */
    int count() const {
        return (int)_credentials.size();
    }

    /** Clears all credentials from memory and NVS. */
    void clear() {
        _credentials.clear();
        Preferences prefs;
        prefs.begin("wifi_creds", false);
        prefs.clear();
        prefs.end();
        Serial.println("[WifiCreds] All credentials cleared");
    }

private:
    std::vector<WifiCredential> _credentials;

    /** Persists the current in-memory list to NVS. */
    void _save() {
        Preferences prefs;
        prefs.begin("wifi_creds", false); // read-write

        // Clear old keys to avoid stale data
        prefs.clear();

        uint8_t count = (uint8_t)_credentials.size();
        prefs.putUChar("count", count);

        for (uint8_t i = 0; i < count; i++) {
            String ssidKey = "ssid_" + String(i);
            String passKey = "pass_" + String(i);
            prefs.putString(ssidKey.c_str(), _credentials[i].ssid.c_str());
            prefs.putString(passKey.c_str(), _credentials[i].password.c_str());
        }

        prefs.end();
        Serial.printf("[WifiCreds] Saved %d credential(s) to NVS\n", count);
    }
};
