package org.qindaqt.link.sms;

/**
 * Builds the provider selection for the older-than-cursor message pages (pure, so it is testable).
 *
 * AGENT-GUARD: ContentResolver binds every selection argument as TEXT. SQLite compares a column that has
 * INTEGER affinity with a TEXT value numerically, but an expression such as {@code date*1000} has no
 * affinity, so INTEGER < TEXT is always true and the page repeats forever. Therefore (1) compare plain
 * columns only, and (2) wrap every bound value in CAST(? AS INTEGER). SmsTest runs the clauses through
 * sqlite3 with TEXT-bound values when the CLI is present.
 */
public final class SmsQueries {
    private SmsQueries() { }

    /**
     * Returns {where, arg...} selecting rows of one thread strictly older than the cursor.
     * {@code mms} tables store seconds; the cursor is in milliseconds. {@code before < 0} means no cursor.
     */
    public static String[] messageSelection(boolean mms, long thread, long before, long beforeId) {
        if (before < 0) return new String[] {"thread_id=CAST(? AS INTEGER)", Long.toString(thread)};
        long lt = before, eq = before;
        if (mms) {
            lt = (before + 999) / 1000;                 // sec*1000 < before  <=>  sec < ceil(before/1000)
            eq = before % 1000 == 0 ? before / 1000 : -1; // sec*1000 == before only on a whole second
        }
        return new String[] {
            "thread_id=CAST(? AS INTEGER) AND (date < CAST(? AS INTEGER) OR (date = CAST(? AS INTEGER) AND _id < CAST(? AS INTEGER)))",
            Long.toString(thread), Long.toString(lt), Long.toString(eq), Long.toString(beforeId)};
    }
}
