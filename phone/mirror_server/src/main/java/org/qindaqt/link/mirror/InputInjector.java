// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.os.SystemClock;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.MotionEvent;
import java.util.HashMap;
import java.util.Map;

/**
 * Injects desktop input into the mirror's virtual display through IInputManager.
 *
 * <p>Coordinates arrive already in virtual-display pixels (the desktop InputMapper owns the
 * window-to-display transform). Every event is stamped with the display id before injection,
 * so input never reaches the phone's own screen.
 *
 * <p>Threading: called only from the session thread. Privacy: text is never logged.
 */
final class InputInjector {
    private static final int INJECT_ASYNC = 0;
    private static final int MAX_POINTERS = 10;

    private final HiddenApi api;
    private int displayId = -1;

    private final int[] pointerIds = new int[MAX_POINTERS];
    private final float[] pointerX = new float[MAX_POINTERS];
    private final float[] pointerY = new float[MAX_POINTERS];
    private final float[] pointerPressure = new float[MAX_POINTERS];
    private int pointerCount;
    private long touchDownTime;

    private long mouseDownTime;
    private int mouseButtons;
    private final Map<Integer, Long> keyDownTimes = new HashMap<>();
    private KeyCharacterMap charMap;

    InputInjector(HiddenApi api) {
        this.api = api;
    }

    void setDisplayId(int id) {
        displayId = id;
    }

    /** Applies one touch transition for {@code id}; returns false for an inconsistent sequence. */
    boolean touch(String action, int id, float x, float y, float pressure) {
        long now = SystemClock.uptimeMillis();
        int index = indexOf(id);
        switch (action) {
            case "down":
                if (index >= 0 || pointerCount == MAX_POINTERS) {
                    return false;
                }
                index = pointerCount++;
                pointerIds[index] = id;
                store(index, x, y, pressure);
                if (pointerCount == 1) {
                    touchDownTime = now;
                    return injectTouch(MotionEvent.ACTION_DOWN, now);
                }
                return injectTouch(MotionEvent.ACTION_POINTER_DOWN
                        | (index << MotionEvent.ACTION_POINTER_INDEX_SHIFT), now);
            case "move":
                if (index < 0) {
                    return false;
                }
                store(index, x, y, pressure);
                return injectTouch(MotionEvent.ACTION_MOVE, now);
            case "up":
                if (index < 0) {
                    return false;
                }
                store(index, x, y, pressure);
                boolean ok = injectTouch(pointerCount == 1 ? MotionEvent.ACTION_UP
                        : MotionEvent.ACTION_POINTER_UP | (index << MotionEvent.ACTION_POINTER_INDEX_SHIFT), now);
                removePointer(index);
                return ok;
            case "cancel":
                if (pointerCount == 0) {
                    return true;
                }
                ok = injectTouch(MotionEvent.ACTION_CANCEL, now);
                pointerCount = 0;
                return ok;
            default:
                return false;
        }
    }

    /** Mouse with buttons (Android BUTTON_* bits); "hover" moves without buttons. */
    boolean mouse(String action, float x, float y, int buttons) {
        long now = SystemClock.uptimeMillis();
        switch (action) {
            case "hover":
                return injectMouse(MotionEvent.ACTION_HOVER_MOVE, now, now, x, y, 0, 0);
            case "down":
                // AGENT-GUARD: only the first pressed button opens the gesture with ACTION_DOWN;
                // a second button while one is held is a BUTTON_PRESS inside the same gesture.
                int pressed = buttons & ~mouseButtons;
                boolean first = mouseButtons == 0;
                mouseButtons = buttons;
                boolean ok = true;
                if (first) {
                    mouseDownTime = now;
                    ok = injectMouse(MotionEvent.ACTION_DOWN, mouseDownTime, now, x, y, buttons, 0);
                }
                return injectMouse(MotionEvent.ACTION_BUTTON_PRESS, mouseDownTime, now, x, y, buttons, pressed) && ok;
            case "move":
                return injectMouse(MotionEvent.ACTION_MOVE, mouseDownTime, now, x, y, mouseButtons, 0);
            case "up":
                int released = mouseButtons & ~buttons;
                mouseButtons = buttons;
                ok = injectMouse(MotionEvent.ACTION_BUTTON_RELEASE, mouseDownTime, now, x, y, buttons, released);
                if (buttons != 0) {
                    return ok;
                }
                return injectMouse(MotionEvent.ACTION_UP, mouseDownTime, now, x, y, 0, 0) && ok;
            default:
                return false;
        }
    }

    boolean scroll(float x, float y, float horizontal, float vertical) {
        long now = SystemClock.uptimeMillis();
        MotionEvent.PointerProperties[] props = {pointerProperties(0, MotionEvent.TOOL_TYPE_MOUSE)};
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = x;
        coords.y = y;
        coords.setAxisValue(MotionEvent.AXIS_HSCROLL, horizontal);
        coords.setAxisValue(MotionEvent.AXIS_VSCROLL, vertical);
        MotionEvent event = MotionEvent.obtain(now, now, MotionEvent.ACTION_SCROLL, 1, props,
                new MotionEvent.PointerCoords[] {coords}, 0, 0, 1f, 1f, 0, 0, InputDevice.SOURCE_MOUSE, 0);
        return inject(event);
    }

    boolean key(String action, int keycode, int meta, int repeat) {
        long now = SystemClock.uptimeMillis();
        int keyAction;
        long downTime;
        if ("down".equals(action)) {
            keyAction = KeyEvent.ACTION_DOWN;
            if (repeat == 0 || !keyDownTimes.containsKey(keycode)) {
                keyDownTimes.put(keycode, now);
            }
            downTime = keyDownTimes.get(keycode);
        } else if ("up".equals(action)) {
            keyAction = KeyEvent.ACTION_UP;
            Long stored = keyDownTimes.remove(keycode);
            downTime = stored == null ? now : stored;
        } else {
            return false;
        }
        KeyEvent event = new KeyEvent(downTime, now, keyAction, keycode, repeat, meta,
                KeyCharacterMap.VIRTUAL_KEYBOARD, 0, 0, InputDevice.SOURCE_KEYBOARD);
        return inject(event);
    }

    /** Types text through the virtual key map; returns the number of characters it could not map. */
    int text(String text) {
        if (charMap == null) {
            charMap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
        }
        int unmapped = 0;
        char[] one = new char[1];
        for (int i = 0; i < text.length(); i++) {
            one[0] = text.charAt(i);
            KeyEvent[] events = charMap.getEvents(one);
            if (events == null) {
                unmapped++;
                continue;
            }
            for (KeyEvent event : events) {
                inject(KeyEvent.changeTimeRepeat(event, SystemClock.uptimeMillis(), 0));
            }
        }
        return unmapped;
    }

    private boolean injectTouch(int action, long now) {
        MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[pointerCount];
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[pointerCount];
        for (int i = 0; i < pointerCount; i++) {
            props[i] = pointerProperties(pointerIds[i], MotionEvent.TOOL_TYPE_FINGER);
            coords[i] = new MotionEvent.PointerCoords();
            coords[i].x = pointerX[i];
            coords[i].y = pointerY[i];
            coords[i].pressure = pointerPressure[i];
            coords[i].size = 1f;
        }
        MotionEvent event = MotionEvent.obtain(touchDownTime, now, action, pointerCount, props, coords,
                0, 0, 1f, 1f, 0, 0, InputDevice.SOURCE_TOUCHSCREEN, 0);
        return inject(event);
    }

    private boolean injectMouse(int action, long downTime, long now, float x, float y, int buttons,
            int actionButton) {
        boolean buttonAction = action == MotionEvent.ACTION_BUTTON_PRESS
                || action == MotionEvent.ACTION_BUTTON_RELEASE;
        if (buttonAction && (api.setActionButton == null || actionButton == 0)) {
            return true; // Older shapes accept plain DOWN/UP; nothing else to send.
        }
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = x;
        coords.y = y;
        coords.pressure = buttons == 0 ? 0f : 1f;
        MotionEvent event = MotionEvent.obtain(downTime, now, action, 1,
                new MotionEvent.PointerProperties[] {pointerProperties(0, MotionEvent.TOOL_TYPE_MOUSE)},
                new MotionEvent.PointerCoords[] {coords}, 0, buttons, 1f, 1f, 0, 0,
                InputDevice.SOURCE_MOUSE, 0);
        if (buttonAction) {
            try {
                HiddenApi.call(api.setActionButton, event, actionButton);
            } catch (Exception e) {
                event.recycle();
                return false;
            }
        }
        return inject(event);
    }

    private boolean inject(InputEvent event) {
        if (displayId < 0) {
            return false;
        }
        try {
            HiddenApi.call(api.setDisplayId, event, displayId);
            boolean ok = (Boolean) HiddenApi.call(api.injectInputEvent, api.inputManager, event, INJECT_ASYNC);
            Ln.d("inject " + event.getClass().getSimpleName() + " display " + displayId + " -> " + ok);
            return ok;
        } catch (Exception e) {
            Ln.w("input injection failed: " + Ln.describe(e));
            return false;
        } finally {
            if (event instanceof MotionEvent) {
                ((MotionEvent) event).recycle();
            }
        }
    }

    private static MotionEvent.PointerProperties pointerProperties(int id, int toolType) {
        MotionEvent.PointerProperties props = new MotionEvent.PointerProperties();
        props.id = id;
        props.toolType = toolType;
        return props;
    }

    private int indexOf(int id) {
        for (int i = 0; i < pointerCount; i++) {
            if (pointerIds[i] == id) {
                return i;
            }
        }
        return -1;
    }

    private void store(int index, float x, float y, float pressure) {
        pointerX[index] = x;
        pointerY[index] = y;
        pointerPressure[index] = pressure;
    }

    private void removePointer(int index) {
        for (int i = index; i < pointerCount - 1; i++) {
            pointerIds[i] = pointerIds[i + 1];
            pointerX[i] = pointerX[i + 1];
            pointerY[i] = pointerY[i + 1];
            pointerPressure[i] = pointerPressure[i + 1];
        }
        pointerCount--;
    }
}
