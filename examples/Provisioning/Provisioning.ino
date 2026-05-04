/**
 * WiFiManager – AP+STA Provisioning Flow Example
 * ================================================
 * Demonstrates the non-blocking AP+STA provisioning flow:
 *
 *   1. On first boot (or when stored credentials fail) the device starts a
 *      SoftAP so users can connect and reach the captive-portal setup page.
 *   2. The OS automatically opens the portal thanks to the built-in captive-
 *      portal probe handlers (Android /generate_204, iOS /hotspot-detect.html,
 *      Windows /ncsi.txt).
 *   3. When the user selects a network and submits a password the device starts
 *      a STA connection attempt WITHOUT tearing down the AP or rebooting.
 *   4. The browser polls /status (JSON) and displays real-time feedback
 *      (connecting → connected / failed).
 *   5. On success: credentials are saved, the IP address is shown, and the AP
 *      shuts down after a configurable delay (default 15 s).
 *   6. On failure: a descriptive error is shown and the user can retry – the AP
 *      stays up and no credentials are saved.
 *   7. On subsequent boots the stored credentials are tried first; if they fail
 *      the provisioning portal starts again automatically.
 *
 * Key new APIs used
 * -----------------
 *   wm.setKeepAPDuringSTAConnect(true)   – enable AP+STA provisioning mode
 *   wm.setAPShutdownDelay(15000)         – keep AP 15 s after STA connects
 *   wm.setDetailedFailureReasons(true)   – human-readable /status errors
 *   wm.setCaptivePortalCompatibility(true) – OS captive portal probes (default)
 *   wm.getProvisioningState()            – poll state from user code
 *
 * Works on ESP32 and ESP8266.
 * Library: https://github.com/tzapu/WiFiManager
 */

#include <WiFiManager.h>

WiFiManager wm;

// ---- user-configurable options ----
static const char AP_NAME[]  = "ESP-Setup";
static const char AP_PASS[]  = "";         // leave empty for open AP
static const unsigned long AP_SHUTDOWN_DELAY_MS = 15000; // 15 s
// -----------------------------------

bool provisioningDone = false;

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\n=== WiFiManager AP+STA Provisioning Example ==="));

  // Optional: uncomment to wipe saved credentials and force provisioning
  // wm.resetSettings();

  // ---- AP+STA provisioning options ----
  wm.setKeepAPDuringSTAConnect(true);         // Keep AP while STA connects
  wm.setAPShutdownDelay(AP_SHUTDOWN_DELAY_MS); // Shut down AP 15 s after success
  wm.setDetailedFailureReasons(true);          // Show "Wrong password" etc.
  wm.setCaptivePortalCompatibility(true);      // Android/iOS/Windows auto-open (default)

  // ---- general portal options ----
  wm.setConfigPortalBlocking(false);  // Non-blocking so loop() keeps running
  wm.setConfigPortalTimeout(0);       // 0 = never time out while user is present

  // Callback: called when SoftAP is started
  wm.setAPCallback([](WiFiManager *w) {
    Serial.print(F("AP started: "));
    Serial.println(w->getConfigPortalSSID());
    Serial.print(F("AP IP: "));
    Serial.println(WiFi.softAPIP());
  });

  // Callback: called when credentials are saved and STA is confirmed connected
  wm.setSaveConfigCallback([]() {
    Serial.println(F("Credentials saved! STA connected."));
  });

  // Try stored credentials; on failure start provisioning portal
  if (wm.autoConnect(AP_NAME, AP_PASS[0] ? AP_PASS : nullptr)) {
    // Stored credentials worked – device is already connected
    Serial.print(F("Auto-connected! IP: "));
    Serial.println(WiFi.localIP());
    provisioningDone = true;
  } else {
    Serial.println(F("No stored credentials (or they failed). Provisioning portal running."));
    Serial.println(F("Connect to the AP and open the captive portal to configure WiFi."));
  }
}

void loop() {
  // Drive the non-blocking portal (processes DNS, HTTP, and the provisioning
  // state machine).  Returns true once STA is connected AND the AP has been
  // shut down (i.e. provisioning is complete).
  if (!provisioningDone) {
    if (wm.process()) {
      provisioningDone = true;
      Serial.print(F("Provisioning complete! STA IP: "));
      Serial.println(WiFi.localIP());
    }

    // You can also inspect the state directly:
    wm_provstate_t state = wm.getProvisioningState();
    static wm_provstate_t lastState = WM_PROV_IDLE;
    if (state != lastState) {
      lastState = state;
      switch (state) {
        case WM_PROV_CONNECTING:
          Serial.println(F("[Provisioning] Connecting to STA..."));
          break;
        case WM_PROV_CONNECTED:
          Serial.print(F("[Provisioning] STA connected, IP: "));
          Serial.println(WiFi.localIP());
          Serial.println(F("AP will shut down shortly."));
          break;
        case WM_PROV_FAILED:
          Serial.println(F("[Provisioning] Connection failed. Portal still open."));
          break;
        default:
          break;
      }
    }
  }

  // ---- put your normal application code here ----
  // It runs concurrently while the provisioning portal is active.
  // Example: blink LED, read sensors, etc.
}
