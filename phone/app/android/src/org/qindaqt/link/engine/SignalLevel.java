package org.qindaqt.link.engine;

/**
 * Maps radio readings to the protocol's 0-4 {@code status.network.signal} level. Pure so the JVM tests
 * cover it; the Android source feeds it dBm values.
 */
public final class SignalLevel {
    /** AOSP WifiManager's own bounds for the 5-step Wi-Fi level. */
    static final int MIN_RSSI = -100, MAX_RSSI = -55;

    private SignalLevel() {}

    /** Wi-Fi RSSI in dBm to 0-4; -127 and above 0 are Android's "unknown" markers and give -1. */
    public static int wifi(int rssi) {
        if (rssi >= 0 || rssi <= -127) return -1;
        if (rssi <= MIN_RSSI) return 0;
        if (rssi >= MAX_RSSI) return 4;
        return (rssi - MIN_RSSI) * 4 / (MAX_RSSI - MIN_RSSI);
    }

    /** A platform level (SignalStrength.getLevel()) to 0-4, or -1 when out of range. */
    public static int platform(int level) { return level < 0 || level > 4 ? -1 : level; }
}
