package org.qindaqt.link.engine;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

/**
 * App-private memory of LAN addresses: the manual address the user typed and the last LAN address that
 * connected. One small file in filesDir ("manual=host:port" / "last=host:port" lines); a missing or
 * unreadable file means none. Android-free.
 *
 * AGENT-GUARD: addresses are personal data. Never log them, never put them in EngineState or the
 * shared preferences backup (the app disables backup).
 */
public final class LanMemory {
    private final File file;

    public LanMemory(File file) { this.file = file; }

    public synchronized Endpoint manual() { return ManualAddress.parseOrNull(read("manual")); }

    public synchronized String manualText() { String m = read("manual"); return m == null ? "" : m; }

    /** Stores a validated manual address, or clears it for an empty string. Throws IllegalArgumentException when invalid. */
    public synchronized void setManual(String raw) throws IOException {
        if (raw == null || raw.trim().isEmpty()) { write("manual", null); return; }
        write("manual", ManualAddress.parse(raw).hostPort());
    }

    public synchronized Endpoint lastLan() { return ManualAddress.parseOrNull(read("last")); }

    public synchronized void setLastLan(Endpoint e) throws IOException { write("last", e == null ? null : e.hostPort()); }

    private String read(String key) {
        try {
            for (String line : new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8).split("\n")) {
                if (line.startsWith(key + "=")) return line.substring(key.length() + 1);
            }
        } catch (IOException e) {
            return null;
        }
        return null;
    }

    private void write(String key, String value) throws IOException {
        String manual = key.equals("manual") ? value : read("manual");
        String last = key.equals("last") ? value : read("last");
        StringBuilder sb = new StringBuilder();
        if (manual != null) sb.append("manual=").append(manual).append('\n');
        if (last != null) sb.append("last=").append(last).append('\n');
        File tmp = new File(file.getPath() + ".tmp");
        try (java.io.FileOutputStream o = new java.io.FileOutputStream(tmp)) {
            o.write(sb.toString().getBytes(StandardCharsets.UTF_8));
            o.getFD().sync();
        }
        Files.move(tmp.toPath(), file.toPath(), StandardCopyOption.REPLACE_EXISTING);
    }
}
