package org.qindaqt.link.input;

/**
 * Turns raw touch frames into pointer, button and scroll actions. Pure Java: the caller supplies the
 * clock, so every rule is testable (phone/engine-tests/InputTest.java).
 *
 * Gestures: one finger moves the pointer; a quick tap is a left click; a finger held still for
 * {@link #HOLD_MS} presses the left button and the following movement drags until the finger lifts;
 * two fingers scroll; a quick two-finger tap is a right click. Movement is coalesced to at most one
 * action per {@link #EMIT_GAP_MS} (120 per second, the protocol page's phone-side bound).
 *
 * AGENT-GUARD: reset() must be called when the touch surface goes away or the screen is left; it releases
 * a button held by a drag so the desktop is never left with a stuck button.
 * Threading: one thread (the Qt GUI thread through the bridge).
 */
public final class TouchGestures {
    /** What a gesture produces; implemented by the input feature. */
    public interface Sink {
        void pointer(double dx, double dy);
        void button(String button, String state);
        /** Smooth scroll; {@code end} marks the end of a gesture. */
        void scroll(double dx, double dy, boolean end);
        void haptic();
    }

    public static final double SLOP_PX = 8;
    public static final long TAP_MS = 250;
    public static final long HOLD_MS = 350;
    public static final long EMIT_GAP_MS = 8;
    /** Scroll units per pixel of two-finger travel (one wheel detent is about 15 units). */
    public static final double SCROLL_PER_PX = 0.5;

    private enum Mode { IDLE, ONE, DRAG, TWO, WAIT_UP }

    private final Sink sink;
    private double sensitivity = 1.6;
    private boolean natural = true;
    private Mode mode = Mode.IDLE;
    private long downT, lastEmit = Long.MIN_VALUE / 2;
    private double startX, startY, lastX, lastY;
    private double pendX, pendY, pendScrollX, pendScrollY;
    private boolean moved, scrolled;

    public TouchGestures(Sink sink) { this.sink = sink; }

    /** Pointer speed multiplier, clamped to 0.3..4. */
    public void setSensitivity(double s) { sensitivity = Math.max(0.3, Math.min(4.0, Double.isNaN(s) ? 1.6 : s)); }
    public double sensitivity() { return sensitivity; }
    /** Natural scrolling: fingers moving up scroll the content up (toward its end). Default on. */
    public void setNaturalScroll(boolean on) { natural = on; }
    public boolean dragging() { return mode == Mode.DRAG; }

    /**
     * One touch frame: {@code n} fingers down (only the first two are used) and their positions in
     * pixels. Call again with the same points on a timer so a held finger is noticed.
     */
    public void update(long t, int n, double x0, double y0, double x1, double y1) {
        if (n > 2) n = 2;
        switch (mode) {
            case IDLE:
                if (n == 1) beginOne(t, x0, y0);
                else if (n == 2) beginTwo(t, x0, y0, x1, y1);
                break;
            case ONE: one(t, n, x0, y0, x1, y1); break;
            case DRAG: drag(t, n, x0, y0); break;
            case TWO: two(t, n, x0, y0, x1, y1); break;
            case WAIT_UP:
                if (n == 0) mode = Mode.IDLE;
                break;
        }
    }

    /** Releases anything held (a drag) and forgets the touch; safe at any time. */
    public void reset() {
        if (mode == Mode.DRAG) sink.button("left", "up");
        if (mode == Mode.TWO && scrolled) sink.scroll(0, 0, true);
        pendX = pendY = pendScrollX = pendScrollY = 0;
        mode = Mode.IDLE;
        moved = scrolled = false;
    }

    private void beginOne(long t, double x, double y) {
        mode = Mode.ONE;
        pendX = pendY = 0;
        downT = t;
        startX = lastX = x;
        startY = lastY = y;
        moved = false;
    }

    private void one(long t, int n, double x0, double y0, double x1, double y1) {
        if (n == 0) {
            flush(t);
            if (!moved && t - downT <= TAP_MS) sink.button("left", "click");
            mode = Mode.IDLE;
        } else if (n == 2) {
            flush(t);
            beginTwo(t, x0, y0, x1, y1);
        } else {
            if (!moved && Math.hypot(x0 - startX, y0 - startY) > SLOP_PX) moved = true;
            if (moved) accumulate(t, x0 - lastX, y0 - lastY);
            else if (t - downT >= HOLD_MS) {
                mode = Mode.DRAG;
                sink.button("left", "down");
                sink.haptic();
            }
            lastX = x0;
            lastY = y0;
        }
    }

    private void drag(long t, int n, double x0, double y0) {
        if (n == 0) {
            flush(t);
            sink.button("left", "up");
            mode = Mode.IDLE;
        } else {
            accumulate(t, x0 - lastX, y0 - lastY);
            lastX = x0;
            lastY = y0;
        }
    }

    private void beginTwo(long t, double x0, double y0, double x1, double y1) {
        mode = Mode.TWO;
        pendScrollX = pendScrollY = 0;
        downT = t;
        startX = lastX = (x0 + x1) / 2;
        startY = lastY = (y0 + y1) / 2;
        moved = scrolled = false;
    }

    private void two(long t, int n, double x0, double y0, double x1, double y1) {
        if (n < 2) {
            flushScroll(t, true);
            if (!moved && t - downT <= TAP_MS) {
                sink.button("right", "click");
                sink.haptic();
            } else if (scrolled) {
                sink.scroll(0, 0, true);
            }
            mode = n == 0 ? Mode.IDLE : Mode.WAIT_UP;
            return;
        }
        double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        if (!moved && Math.hypot(cx - startX, cy - startY) > SLOP_PX) moved = true;
        if (moved) {
            double dy = (cy - lastY) * SCROLL_PER_PX * (natural ? -1 : 1);
            double dx = (cx - lastX) * SCROLL_PER_PX * (natural ? -1 : 1);
            pendScrollX += dx;
            pendScrollY += dy;
            scrolled = true;
            flushScroll(t, false);
        }
        lastX = cx;
        lastY = cy;
    }

    private void accumulate(long t, double dx, double dy) {
        pendX += dx * sensitivity;
        pendY += dy * sensitivity;
        flush(t);
    }

    /**
     * Sends accumulated movement if the 120/s gap has passed. AGENT-GUARD: never forced, so the phone-side
     * rate bound holds; the sub-8 ms remainder when a finger lifts is dropped on purpose.
     */
    private void flush(long t) {
        if (pendX == 0 && pendY == 0) return;
        if (t < lastEmit) lastEmit = t - EMIT_GAP_MS; // the caller's clock restarted
        if (t - lastEmit < EMIT_GAP_MS) return;
        sink.pointer(pendX, pendY);
        pendX = pendY = 0;
        lastEmit = t;
    }

    private void flushScroll(long t, boolean force) {
        if (pendScrollX == 0 && pendScrollY == 0) return;
        if (!force && t - lastEmit < EMIT_GAP_MS) return;
        sink.scroll(pendScrollX, pendScrollY, false);
        pendScrollX = pendScrollY = 0;
        lastEmit = t;
    }
}
