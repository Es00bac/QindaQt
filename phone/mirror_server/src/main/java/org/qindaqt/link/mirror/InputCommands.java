// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Applies desktop input control messages through InputPolicy before injection.
 *
 * <p>Coordinates are clamped to the current display, mouse buttons are masked and key events
 * pass the keycode allowlist; rejected keys are counted in the log, never their meaning.
 * Threading: session thread only. Privacy: text bodies are never logged.
 */
final class InputCommands {
    private final InputInjector injector;
    private int width;
    private int height;

    InputCommands(InputInjector injector) {
        this.injector = injector;
    }

    void setDisplay(int displayId, int displayWidth, int displayHeight) {
        injector.setDisplayId(displayId);
        width = displayWidth;
        height = displayHeight;
    }

    /** Returns false when {@code type} is not an input message. */
    boolean apply(int type, JSONObject body) throws JSONException {
        switch (type) {
            case Frames.C_TOUCH:
                injector.touch(body.getString("action"), body.optInt("pointerId", 0), x(body), y(body),
                        (float) Math.max(0.0, Math.min(1.0, body.optDouble("pressure", 1.0))));
                return true;
            case Frames.C_MOUSE:
                injector.mouse(body.getString("action"), x(body), y(body),
                        InputPolicy.buttons(body.optInt("buttons", 0)));
                return true;
            case Frames.C_SCROLL:
                injector.scroll(x(body), y(body), notches(body, "h"), notches(body, "v"));
                return true;
            case Frames.C_KEY:
                int keycode = body.getInt("keycode");
                if (!InputPolicy.keyAllowed(keycode)) {
                    Ln.w("rejected a key outside the mirror allowlist");
                    return true;
                }
                injector.key(body.getString("action"), keycode, body.optInt("meta", 0),
                        Math.max(0, body.optInt("repeat", 0)));
                return true;
            case Frames.C_TEXT:
                String text = body.getString("text");
                int unmapped = injector.text(text);
                if (unmapped > 0) {
                    Ln.d("text: " + unmapped + " of " + text.length() + " characters had no key mapping");
                }
                return true;
            default:
                return false;
        }
    }

    private float x(JSONObject body) throws JSONException {
        return InputPolicy.clamp((float) body.getDouble("x"), width);
    }

    private float y(JSONObject body) throws JSONException {
        return InputPolicy.clamp((float) body.getDouble("y"), height);
    }

    private static float notches(JSONObject body, String key) {
        double value = body.optDouble(key, 0);
        return (float) (Double.isNaN(value) ? 0 : Math.max(-100, Math.min(100, value)));
    }
}
