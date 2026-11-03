package org.qindaqt.link.engine;

import java.util.Random;

/**
 * Reconnect delay ladder with jitter: the n-th consecutive failure waits a random time in
 * [ladder/2, ladder], so a desktop that comes back is not hit by every retry at the same instant.
 * Android-free.
 */
public final class Backoff {
    private final long[] ladderMs;
    private final Random random;

    public Backoff(long[] ladderMs, Random random) {
        this.ladderMs = ladderMs.clone();
        this.random = random;
    }

    /** @param failures consecutive failures so far; 0 or 1 selects the first rung */
    public long delayMs(int failures) {
        long base = ladderMs[Math.min(Math.max(failures - 1, 0), ladderMs.length - 1)];
        return base / 2 + (long) (random.nextDouble() * (base - base / 2));
    }
}
