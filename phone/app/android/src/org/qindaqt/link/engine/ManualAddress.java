package org.qindaqt.link.engine;

import java.util.Locale;
import java.util.regex.Pattern;

/**
 * Parses the manual desktop address typed in Setup: "host", "host:port", "[v6]:port" or a bare IPv6
 * literal; the port defaults to 47160. A DNS name (MagicDNS) is accepted but never resolved here.
 * Android-free.
 *
 * AGENT-GUARD: loopback is refused. 127.0.0.1:47160 is the USB listener, and a "manual" loopback
 * endpoint would be reported as LAN while really being USB.
 */
public final class ManualAddress {
    private static final Pattern LABEL = Pattern.compile("[A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?");
    private static final Pattern IPV4 = Pattern.compile("(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})");
    private static final Pattern IPV6 = Pattern.compile("[0-9A-Fa-f:.]{2,45}");

    private ManualAddress() {}

    /** The endpoint, or throws {@link IllegalArgumentException} whose message is fit to show the user. */
    public static Endpoint parse(String raw) {
        if (raw == null) throw new IllegalArgumentException("Enter an address such as 100.64.0.1:47160");
        String s = raw.trim();
        if (s.isEmpty()) throw new IllegalArgumentException("Enter an address such as 100.64.0.1:47160");
        if (s.length() > 260) throw new IllegalArgumentException("That address is too long");
        String host;
        int port = Endpoint.DEFAULT_PORT;
        if (s.startsWith("[")) {
            int close = s.indexOf(']');
            if (close < 0) throw new IllegalArgumentException("Missing ] in the IPv6 address");
            host = s.substring(1, close);
            String rest = s.substring(close + 1);
            if (!rest.isEmpty()) port = parsePort(rest.startsWith(":") ? rest.substring(1) : "x");
        } else {
            int colons = count(s, ':');
            if (colons == 1) {
                int i = s.indexOf(':');
                host = s.substring(0, i);
                port = parsePort(s.substring(i + 1));
            } else {
                host = s; // no port, or a bare IPv6 literal
            }
        }
        if (!validHost(host)) throw new IllegalArgumentException("That is not a valid host name or IP address");
        if (isLoopback(host)) throw new IllegalArgumentException("Use USB for this computer; enter the desktop's network address");
        return new Endpoint(host, port, Endpoint.Kind.LAN_MANUAL);
    }

    /** Null instead of an exception, for stored values. */
    public static Endpoint parseOrNull(String raw) {
        try { return parse(raw); } catch (IllegalArgumentException e) { return null; }
    }

    private static int parsePort(String p) {
        if (!p.matches("\\d{1,5}")) throw new IllegalArgumentException("The port must be a number from 1 to 65535");
        int v = Integer.parseInt(p);
        if (v < 1 || v > 65535) throw new IllegalArgumentException("The port must be a number from 1 to 65535");
        return v;
    }

    private static boolean validHost(String h) {
        if (h.isEmpty() || h.length() > 253) return false;
        if (h.indexOf(':') >= 0) return count(h, ':') >= 2 && IPV6.matcher(h).matches();
        java.util.regex.Matcher m = IPV4.matcher(h);
        if (m.matches()) {
            for (int i = 1; i <= 4; i++) if (Integer.parseInt(m.group(i)) > 255) return false;
            return true;
        }
        if (h.matches("[0-9.]+")) return false; // looks numeric but is not a valid IPv4
        for (String label : h.split("\\.", -1)) if (!LABEL.matcher(label).matches()) return false;
        return true;
    }

    private static boolean isLoopback(String h) {
        String l = h.toLowerCase(Locale.ROOT);
        return l.equals("localhost") || l.startsWith("127.") || l.equals("::1") || l.equals("0:0:0:0:0:0:0:1")
                || l.equals("0.0.0.0") || l.equals("::");
    }

    private static int count(String s, char c) {
        int n = 0;
        for (int i = 0; i < s.length(); i++) if (s.charAt(i) == c) n++;
        return n;
    }
}
