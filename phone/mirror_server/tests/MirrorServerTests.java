// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** Host-JVM tests for the mirror server's pure policies; see run.sh. */
public final class MirrorServerTests {
    private static int passed;
    private static int failed;

    private MirrorServerTests() {}

    public static void main(String[] args) {
        framing();
        secret();
        inputPolicy();
        listenPolicy();
        System.out.println("mirror server tests: " + passed + " passed, " + failed + " failed");
        System.exit(failed == 0 ? 0 : 1);
    }

    private static void check(String name, boolean condition) {
        if (condition) {
            passed++;
        } else {
            failed++;
            System.out.println("FAIL " + name);
        }
    }

    private static byte[] header(int channel, int type, long length, int reserved) {
        return new byte[] {(byte) channel, (byte) type, (byte) reserved, 0, (byte) (length >>> 24),
            (byte) (length >>> 16), (byte) (length >>> 8), (byte) length};
    }

    private static String read(byte[] bytes) {
        try {
            Frames.Frame frame = Frames.read(new DataInputStream(new ByteArrayInputStream(bytes)));
            return frame == null ? "eof" : "frame";
        } catch (Frames.ProtocolException e) {
            return "protocol";
        } catch (IOException e) {
            return "io";
        }
    }

    // Same classification as tests/apps/link_mirror/tst_mirror_framing.cpp.
    private static void framing() {
        check("unknown channel", read(header(4, 1, 2, 0)).equals("protocol"));
        check("channel zero", read(header(0, 1, 2, 0)).equals("protocol"));
        check("reserved bytes", read(header(1, 1, 2, 1)).equals("protocol"));
        check("zero length", read(header(1, 1, 0, 0)).equals("protocol"));
        check("control over 1 MiB", read(header(1, 1, (1 << 20) + 1, 0)).equals("protocol"));
        check("inventory over 1 MiB", read(header(3, 1, (1 << 20) + 1, 0)).equals("protocol"));
        check("video over 16 MiB", read(header(2, 2, (16 << 20) + 1, 0)).equals("protocol"));
        byte[] hello = new byte[10];
        System.arraycopy(header(1, 1, 2, 0), 0, hello, 0, 8);
        hello[8] = '{';
        hello[9] = '}';
        check("valid frame", read(hello).equals("frame"));
        check("truncated frame", read(java.util.Arrays.copyOf(hello, 9)).equals("protocol"));
        check("clean end", read(new byte[0]).equals("eof"));
    }

    private static String readSecret(String text) {
        try {
            return SessionSecret.read(new ByteArrayInputStream(text.getBytes(StandardCharsets.US_ASCII)));
        } catch (IOException e) {
            return null;
        }
    }

    private static void secret() {
        String good = "00112233445566778899aabbccddeeff";
        check("secret line", good.equals(readSecret(good + "\n")));
        check("secret CRLF", good.equals(readSecret(good + "\r\nrest")));
        check("secret without newline", good.equals(readSecret(good)));
        check("secret uppercase rejected", readSecret(good.toUpperCase() + "\n") == null);
        check("secret too short", readSecret("0011\n") == null);
        check("secret empty", readSecret("\n") == null);
        check("secret overlong line", readSecret(good + good + good + good + good + "\n") == null);
        check("matches equal", SessionSecret.matches(good, good));
        check("matches differ", !SessionSecret.matches(good, good.replace('f', 'e')));
        check("matches prefix", !SessionSecret.matches(good, good.substring(0, 31)));
        check("matches null", !SessionSecret.matches(good, null));
        check("suffix valid", SessionSecret.validSuffix("0123456789abcdef"));
        check("suffix not hex", !SessionSecret.validSuffix("0123456789abcdeg"));
        check("suffix null", !SessionSecret.validSuffix(null));
    }

    private static void inputPolicy() {
        int[] allowed = {4, 7, 16, 19, 23, 29, 54, 61, 62, 66, 67, 111, 112, 122, 123, 131, 142, 160};
        for (int key : allowed) {
            check("key allowed " + key, InputPolicy.keyAllowed(key));
        }
        // HOME, CALL, ENDCALL, VOLUME_UP/DOWN, POWER, CAMERA, SEARCH, NOTIFICATION, MUTE,
        // META_LEFT/RIGHT, VOLUME_MUTE, APP_SWITCH, ASSIST, BRIGHTNESS_DOWN/UP, SLEEP, WAKEUP,
        // VOICE_ASSIST, SOFT_SLEEP, ALL_APPS, SYSTEM_NAVIGATION_DOWN.
        int[] rejected = {3, 5, 6, 24, 25, 26, 27, 84, 83, 91, 117, 118, 164, 187, 219, 220, 221,
            223, 224, 231, 276, 284, 281, 0, -1};
        for (int key : rejected) {
            check("key rejected " + key, !InputPolicy.keyAllowed(key));
        }
        check("clamp negative", InputPolicy.clamp(-5f, 720) == 0f);
        check("clamp beyond", InputPolicy.clamp(5000f, 720) == 719f);
        check("clamp inside", InputPolicy.clamp(360.5f, 720) == 360.5f);
        check("clamp NaN", InputPolicy.clamp(Float.NaN, 720) == 0f);
        check("clamp no display", InputPolicy.clamp(10f, 0) == 0f);
        check("buttons masked", InputPolicy.buttons(0xff) == 0x1f);
    }

    private static void listenPolicy() {
        ListenPolicy policy = new ListenPolicy();
        check("first failure continues", !policy.recordFailedGreeting());
        check("second failure continues", !policy.recordFailedGreeting());
        check("third failure gives up", policy.recordFailedGreeting());
        check("hello timeout shorter than idle", ListenPolicy.HELLO_TIMEOUT_MS < ListenPolicy.IDLE_TIMEOUT_MS);
        check("idle exceeds three pings", ListenPolicy.IDLE_TIMEOUT_MS >= 3 * 5_000);
    }
}
