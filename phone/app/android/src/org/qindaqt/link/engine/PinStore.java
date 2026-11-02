package org.qindaqt.link.engine;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

/**
 * Persists the one pinned desktop (hex SPKI digest + display name) in a single
 * app-private file. Android-free; the caller passes a file inside filesDir.
 * Format: "<64 hex>\n<name>\n". A missing or malformed file means "no pin".
 */
public final class PinStore {
    private final File file;

    public PinStore(File file) { this.file = file; }

    /** Pinned deviceId (64 lowercase hex) or null. */
    public synchronized String deviceId() {
        String[] p = read();
        return p == null ? null : p[0];
    }

    public synchronized String name() {
        String[] p = read();
        return p == null ? null : p[1];
    }

    public synchronized void set(String deviceId, String name) throws IOException {
        File tmp = new File(file.getPath() + ".tmp");
        try (java.io.FileOutputStream o = new java.io.FileOutputStream(tmp)) {
            o.write((deviceId + "\n" + name.replace('\n', ' ') + "\n").getBytes(StandardCharsets.UTF_8));
            o.getFD().sync();
        }
        Files.move(tmp.toPath(), file.toPath(), StandardCopyOption.REPLACE_EXISTING);
    }

    public synchronized void clear() { file.delete(); }

    private String[] read() {
        try {
            String[] l = new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8).split("\n", -1);
            if (l.length < 2 || !l[0].matches("[0-9a-f]{64}")) return null;
            return new String[] {l[0], l[1]};
        } catch (IOException e) {
            return null;
        }
    }
}
