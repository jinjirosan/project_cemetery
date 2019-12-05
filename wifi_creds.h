/*
 * src/wifi_creds.h
 */

// See https://docs.particle.io/reference/firmware/photon/#setcredentials- for details
struct credentials { char *ssid; char *password; int authType; int cipher; };

const credentials wifiCreds[] = {
  // Set wifi creds here (last entry will be tried first when connecting). Max 5 for Photon.
  // {.ssid="SSID", .password="password", .authType=WPA2, .cipher=WLAN_CIPHER_AES},
  // {.ssid="SSID", .password="password", .authType=WPA2, .cipher=WLAN_CIPHER_AES}
};
