package org.qindaqt.link.sms;

import java.util.List;

/**
 * The phone's SMS/MMS store, abstracted so the paging and bounds logic is JVM-testable.
 * The Android implementation is org.qindaqt.link.android.SmsProviderAndroid.
 *
 * AGENT-CONTRACT: every list method returns rows newest first, ordered by (date desc, id desc),
 * strictly older than the cursor (before, beforeId) when before >= 0, and at most {@code limit}
 * rows. Dates are milliseconds. Implementations never log row contents. On a provider failure they throw {@link ProviderException}; an empty list means no rows.
 */
public interface SmsProvider {
    /** Thread summary. {@code addrs} are raw addresses; names are resolved by {@link SmsModel}. */
    final class ThreadRow {
        public final long id, date;
        public final List<String> addrs;
        public final String snippet;
        public final int unread, count;
        public ThreadRow(long id, long date, List<String> addrs, String snippet, int unread, int count) {
            this.id = id; this.date = date; this.addrs = addrs; this.snippet = snippet; this.unread = unread; this.count = count;
        }
    }

    /** MMS part descriptor. */
    final class PartRow {
        public final long id;
        public final String mime, name;
        public final long size;
        public PartRow(long id, String mime, String name, long size) {
            this.id = id; this.mime = mime; this.name = name; this.size = size;
        }
    }

    /** One SMS or MMS. {@code box} is one of inbox, sent, outbox, failed, queued, draft. */
    final class MessageRow {
        public final long id, date;
        public final boolean mms, read;
        public final String box, addr, body;
        public final List<PartRow> parts;
        public MessageRow(long id, boolean mms, String box, String addr, long date, String body, boolean read, List<PartRow> parts) {
            this.id = id; this.mms = mms; this.box = box; this.addr = addr; this.date = date; this.body = body;
            this.read = read; this.parts = parts;
        }
    }

    /** Raw part bytes, or null when the part does not exist. */
    final class PartData {
        public final String mime;
        public final byte[] bytes;
        /** True when {@code bytes} were already downscaled to fit. */
        public final boolean scaled;
        public PartData(String mime, byte[] bytes, boolean scaled) { this.mime = mime; this.bytes = bytes; this.scaled = scaled; }
    }

    /**
     * A provider failure that must reach the desktop as an error, never as an empty list. The message is a
     * non-content reason (operation and exception class names only).
     */
    final class ProviderException extends RuntimeException {
        private static final long serialVersionUID = 1L;
        public ProviderException(String reason) { super(reason); }
    }

    /**
     * Fills in the expensive parts (MMS body, address, part descriptors) of a row returned by
     * smsMessages/mmsMessages. The model calls it only for rows that make the page. Default: identity.
     */
    default MessageRow hydrate(MessageRow header) { return header; }

    boolean canRead();
    boolean canSend();

    List<ThreadRow> threads(int limit, long before, long beforeId);
    List<MessageRow> smsMessages(long thread, int limit, long before, long beforeId);
    List<MessageRow> mmsMessages(long thread, int limit, long before, long beforeId);

    /** Thread id to a cheap change fingerprint (date, unread, count); used to find changed threads. */
    java.util.Map<Long, String> fingerprints();

    /** Total unread messages. */
    int unreadTotal();

    /**
     * Part bytes, downscaled (images only) to {@code maxEdge} and {@code maxBytes} when needed. Returns null when
     * missing; throws {@link TooLarge} for a non-image over {@code maxBytes}.
     */
    PartData part(long partId, int maxEdge, int maxBytes) throws TooLarge;

    /** Marks a thread read. Returns false when the platform refuses (not the default SMS app). */
    boolean markRead(long thread);

    /** Called (from any thread) when the provider changed; null clears. */
    void setChangeListener(Runnable listener);

    final class TooLarge extends Exception {
        private static final long serialVersionUID = 1L;
        public TooLarge() { super("part too large"); }
    }
}
