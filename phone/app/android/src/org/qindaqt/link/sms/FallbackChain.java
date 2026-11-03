package org.qindaqt.link.sms;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * Capability probing for vendor providers: tries strategies in order, remembers the one that worked so the
 * next call starts there, and fails loudly (never with an empty result) when all of them fail.
 *
 * AGENT-NOTE: exists because Samsung's mms-sms provider rejects columns that AOSP accepts and the reverse.
 * Failure reasons are exception class names only; never put row values in them.
 */
public final class FallbackChain<T> {
    private final List<Callable<T>> strategies;
    private int preferred;

    public FallbackChain(List<Callable<T>> strategies) { this.strategies = strategies; }

    public synchronized T call(String what) {
        StringBuilder why = new StringBuilder();
        for (int n = 0; n < strategies.size(); n++) {
            int i = (preferred + n) % strategies.size();
            try {
                T r = strategies.get(i).call();
                preferred = i;
                return r;
            } catch (Exception e) {
                why.append(why.length() == 0 ? "" : ",").append(e.getClass().getSimpleName());
            }
        }
        throw new SmsProvider.ProviderException(what + " failed: " + why);
    }
}
