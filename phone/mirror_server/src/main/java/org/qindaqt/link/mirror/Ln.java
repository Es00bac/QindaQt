// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

/**
 * Line logger for the mirror server.
 *
 * <p>AGENT-CONTRACT: the desktop reads these lines from the `adb shell` process and repeats
 * them in its own diagnostics. Never log input text, app content or anything a user typed;
 * log counts and lengths instead (QindaLink privacy rule).
 */
final class Ln {
    private static volatile boolean verbose;

    private Ln() {}

    static void setVerbose(boolean value) {
        verbose = value;
    }

    static void d(String message) {
        if (verbose) {
            emit("D", message);
        }
    }

    static void i(String message) {
        emit("I", message);
    }

    static void w(String message) {
        emit("W", message);
    }

    static void e(String message, Throwable error) {
        emit("E", message + (error == null ? "" : ": " + describe(error)));
    }

    static String describe(Throwable error) {
        Throwable root = error;
        while (root.getCause() != null && root.getCause() != root) {
            root = root.getCause();
        }
        String text = root.getClass().getSimpleName();
        if (root.getMessage() != null) {
            text += " " + root.getMessage();
        }
        return text;
    }

    private static synchronized void emit(String level, String message) {
        System.out.println("[qindalink-mirror] " + level + " " + message);
        System.out.flush();
    }
}
