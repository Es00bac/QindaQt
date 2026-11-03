# QindaLink phone-signal handoff

Cause (inferred): NetworkCapabilities.getSignalStrength() is redacted for ordinary apps (dumpsys shows the value, the app
got unspecified), so the Wi-Fi level stayed null. Fix: fall back to WifiManager.getConnectionInfo().getRssi() (not
location-gated; needs normal ACCESS_WIFI_STATE, added), mapped by pure engine/SignalLevel.wifi(); RSSI_CHANGED_ACTION
receiver refreshes it. Cellular unchanged.

Tests: phone/engine-tests/run.sh exit 0 (RingTest now 17 checks incl. level mapping); build-apk.sh exit 0; APK installed
on the phone, engine started, no exceptions in the app's logcat.
Caveat: the desktop link service was not running, so Devices() could not be read; the live Wi-Fi level is unconfirmed.
Next: PM reads Devices() with the service up and confirms network.signal is 0-4.
