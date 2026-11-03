// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.regex.Pattern;

/**
 * The per-session secret and the non-secret socket suffix.
 *
 * <p>AGENT-CONTRACT: the desktop (src/apps/link_mirror/src/adb_session.cpp) writes the secret
 * as one line on the server's stdin and puts only the random suffix in argv, the abstract
 * socket name and the adb forward spec. The secret must never appear in argv, a socket name,
 * a forward spec or a log line; see docs/wiki/reference/qindalink-mirror-v1.md#threat-model.
 *
 * <p>Pure Java (no android.*) so phone/mirror_server/tests can run it on the host JVM.
 */
final class SessionSecret {
    static final int MAX_LINE = 128;
    private static final Pattern SECRET = Pattern.compile("[0-9a-f]{32,64}");
    private static final Pattern SUFFIX = Pattern.compile("[0-9a-f]{8,32}");

    private SessionSecret() {}

    /** Reads the first stdin line (at most MAX_LINE bytes) and validates it as a secret. */
    static String read(InputStream in) throws IOException {
        byte[] line = new byte[MAX_LINE];
        int length = 0;
        while (true) {
            int b = in.read();
            if (b < 0 || b == '\n') {
                break;
            }
            if (length == MAX_LINE) {
                throw new IOException("secret line longer than " + MAX_LINE + " bytes");
            }
            line[length++] = (byte) b;
        }
        if (length > 0 && line[length - 1] == '\r') {
            length--;
        }
        String secret = new String(line, 0, length, StandardCharsets.US_ASCII);
        if (!SECRET.matcher(secret).matches()) {
            throw new IOException("stdin did not carry a 32-64 character lowercase hex secret");
        }
        return secret;
    }

    static boolean validSuffix(String suffix) {
        return suffix != null && SUFFIX.matcher(suffix).matches();
    }

    /** Constant-time comparison of the offered hello token with the session secret. */
    static boolean matches(String secret, String offered) {
        if (offered == null) {
            return false;
        }
        return MessageDigest.isEqual(secret.getBytes(StandardCharsets.UTF_8),
                offered.getBytes(StandardCharsets.UTF_8));
    }
}
