// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.function.LongSupplier;
import javax.crypto.AEADBadTagException;

/**
 * An unlocked vault: the DEK in memory, the decrypted list index and the still sealed entries.
 * Locking zeroes the DEK, drops the index and closes every typing source.
 *
 * AGENT-GUARD: the DEK array never leaves this class and is zeroed by {@link #lock()}. All public
 * methods are synchronized and throw {@code LOCKED} after lock(); do not cache a record or the DEK
 * outside. Records returned by {@link #get} belong to the caller, who must wipe() them.
 * AGENT-GUARD: only {@link AEADBadTagException} from the unwrap means "tampered"; every other failure
 * to use the key is KEY_UNAVAILABLE (retryable) or KEY_INVALIDATED, see {@link VaultLifecycle}.
 * Threading: any thread; the Android layer calls from the UI thread and a timer.
 */
public final class VaultSession {
    /** Idle lock timeout (brief: five minutes). */
    public static final long IDLE_LOCK_MILLIS = 5 * 60 * 1000L;
    public static final int FLAG_STRONGBOX = VaultFormat.FLAG_STRONGBOX, FLAG_KEY_B = VaultFormat.FLAG_KEY_B;

    private final LongSupplier clock;
    private final byte[] wrapped;
    private final int flags;
    private long generation;
    private byte[] dek;
    private Map<String, VaultEntry> index = new LinkedHashMap<>();
    private Map<String, VaultCrypto.Sealed> sealed = new LinkedHashMap<>();
    private final List<VaultTypingSource> sources = new ArrayList<>();
    private long lastActivity;

    private VaultSession(byte[] dek, byte[] wrapped, int flags, long generation, LongSupplier clock) {
        this.dek = dek; this.wrapped = wrapped; this.flags = flags; this.generation = generation; this.clock = clock;
        this.lastActivity = clock.getAsLong();
    }

    private static VaultException keyFailure(GeneralSecurityException e) {
        if (e instanceof DekWrapper.KeyInvalidatedException) return new VaultException(VaultException.Reason.KEY_INVALIDATED, "key invalidated");
        if (e instanceof AEADBadTagException) return new VaultException(VaultException.Reason.TAMPERED, "wrapped key failed authentication");
        return new VaultException(VaultException.Reason.KEY_UNAVAILABLE, "key could not be used");
    }

    /** Creates an empty vault (generation 0 until first serialised) with a fresh random DEK wrapped by {@code w}. */
    public static VaultSession create(DekWrapper w, int flags, LongSupplier clock) throws VaultException {
        byte[] dek = VaultCrypto.random(VaultCrypto.KEY_LEN);
        try {
            return new VaultSession(dek, w.wrap(dek), flags & (FLAG_STRONGBOX | FLAG_KEY_B), 0, clock);
        } catch (GeneralSecurityException e) {
            Bin.wipe(dek);
            throw keyFailure(e);
        } catch (RuntimeException e) {
            Bin.wipe(dek);
            throw new VaultException(VaultException.Reason.KEY_UNAVAILABLE, "key could not be used");
        }
    }

    /** The wrapped DEK from a file's header, needed to prepare an authenticated unwrap. Unauthenticated. */
    public static byte[] wrappedKeyOf(byte[] file) throws VaultException { return VaultFormat.parseHeader(file).wrapped; }

    /** Header flags (unauthenticated until open() succeeds); used to pick the Keystore slot before the prompt. */
    public static int flagsOf(byte[] file) throws VaultException { return VaultFormat.parseHeader(file).flags; }

    /** Header generation (unauthenticated until open() succeeds). */
    public static long generationOf(byte[] file) throws VaultException { return VaultFormat.parseHeader(file).generation; }

    /**
     * Opens a vault file; any altered byte fails closed with TAMPERED, CORRUPT or UNSUPPORTED_VERSION,
     * and an authenticated generation below {@code minGeneration} fails with ROLLBACK.
     */
    public static VaultSession open(byte[] file, DekWrapper w, LongSupplier clock, long minGeneration) throws VaultException {
        VaultFormat.Header h = VaultFormat.parseHeader(file);
        byte[] dek;
        try {
            dek = w.unwrap(h.wrapped);
        } catch (GeneralSecurityException e) {
            throw keyFailure(e);
        } catch (RuntimeException e) {
            throw new VaultException(VaultException.Reason.KEY_UNAVAILABLE, "key could not be used");
        }
        VaultSession s = new VaultSession(dek, h.wrapped, h.flags, h.generation, clock);
        try {
            if (dek.length != VaultCrypto.KEY_LEN) throw new VaultException(VaultException.Reason.CORRUPT, "key size");
            s.load(file, h);
            if (h.generation < minGeneration) throw new VaultException(VaultException.Reason.ROLLBACK, "older than a generation already opened");
            return s;
        } catch (VaultException | RuntimeException e) {
            s.lock();
            throw e instanceof VaultException ? (VaultException) e : new VaultException(VaultException.Reason.CORRUPT, "parse");
        }
    }

    private void load(byte[] file, VaultFormat.Header h) throws VaultException {
        Bin.In in = new Bin.In(file);
        in.raw(h.bodyOffset);
        byte[] iv = in.raw(VaultCrypto.IV_LEN);
        byte[] ct = in.bytes(VaultFormat.MAX_CT);
        byte[] plain = VaultCrypto.open(dek, VaultFormat.indexAad(h.bytes), iv, ct);
        try { index = VaultFormat.decodeIndex(plain); } finally { Bin.wipe(plain); }
        long n = in.u32();
        if (n != index.size()) throw new VaultException(VaultException.Reason.TAMPERED, "entry count");
        for (long i = 0; i < n; i++) {
            String id = new String(in.raw(in.u8()), StandardCharsets.UTF_8);
            byte[] eiv = in.raw(VaultCrypto.IV_LEN);
            byte[] ect = in.bytes(VaultFormat.MAX_CT);
            if (!index.containsKey(id) || sealed.put(id, new VaultCrypto.Sealed(eiv, ect)) != null)
                throw new VaultException(VaultException.Reason.TAMPERED, "entry set");
        }
        if (in.remaining() != 0) throw new VaultException(VaultException.Reason.CORRUPT, "trailing bytes");
        // Authenticate every entry now so a tampered file fails at unlock, not when first viewed.
        for (String id : new ArrayList<>(index.keySet())) get(id).wipe();
    }

    private void requireUnlocked() throws VaultException {
        if (dek == null) throw new VaultException(VaultException.Reason.LOCKED, "vault is locked");
    }

    public synchronized boolean isUnlocked() { return dek != null; }

    /** True when the Keystore key that wraps this vault is StrongBox backed. */
    public synchronized boolean strongBox() { return (flags & FLAG_STRONGBOX) != 0; }

    /** Which Keystore alias slot (0 or 1) wraps this vault. */
    public synchronized int keySlot() { return (flags & FLAG_KEY_B) != 0 ? 1 : 0; }

    /** Generation of the file this session was opened from, or of the last serialisation. */
    public synchronized long generation() { return generation; }

    /** Records user activity for the idle timer. */
    public synchronized void touch() { lastActivity = clock.getAsLong(); }

    /** Locks if idle for {@link #IDLE_LOCK_MILLIS}; returns whether it locked. */
    public synchronized boolean lockIfIdle() {
        if (dek != null && clock.getAsLong() - lastActivity >= IDLE_LOCK_MILLIS) { lock(); return true; }
        return false;
    }

    /** Zeroes the DEK, forgets the index and closes typing sources. Idempotent. */
    public synchronized void lock() {
        Bin.wipe(dek);
        dek = null;
        index = new LinkedHashMap<>();
        sealed = new LinkedHashMap<>();
        for (VaultTypingSource s : new ArrayList<>(sources)) s.close();
        sources.clear();
    }

    public synchronized List<VaultEntry> list() throws VaultException {
        requireUnlocked(); touch();
        return new ArrayList<>(index.values());
    }

    /** Matches the query against title, username and URL only (never the secrets). */
    public synchronized List<VaultEntry> search(String query) throws VaultException {
        requireUnlocked(); touch();
        String q = query.trim().toLowerCase(Locale.ROOT);
        List<VaultEntry> r = new ArrayList<>();
        for (VaultEntry e : index.values()) if (q.isEmpty() || e.matches(q)) r.add(e);
        return r;
    }

    public synchronized int size() throws VaultException { requireUnlocked(); return index.size(); }

    /** Decrypts one entry on demand. The caller wipes the record. */
    public synchronized VaultRecord get(String id) throws VaultException {
        requireUnlocked(); touch();
        VaultCrypto.Sealed s = sealed.get(id);
        if (s == null) throw new VaultException(VaultException.Reason.NOT_FOUND, "no such entry");
        byte[] plain = VaultCrypto.open(dek, VaultFormat.entryAad(id), s.iv, s.ct);
        try {
            VaultRecord r = VaultRecord.fromBytes(plain);
            if (!id.equals(r.id) || !r.listFields().sameListFields(index.get(id))) {
                r.wipe();
                throw new VaultException(VaultException.Reason.TAMPERED, "entry does not match index");
            }
            return r;
        } finally { Bin.wipe(plain); }
    }

    /** Seals {@code r} into the given maps (assigning id/time as needed). Does not touch session state. */
    private String sealInto(VaultRecord r, Map<String, VaultEntry> idx, Map<String, VaultCrypto.Sealed> sl) throws VaultException {
        if (r.title.isEmpty() && r.username.isEmpty() && r.url.isEmpty()) throw new VaultException(VaultException.Reason.INVALID, "empty entry");
        if (r.id == null) r.id = newId();
        else if (!idx.containsKey(r.id) && !isId(r.id)) throw new VaultException(VaultException.Reason.INVALID, "bad id");
        if (!idx.containsKey(r.id) && idx.size() >= VaultFormat.MAX_ENTRIES) throw new VaultException(VaultException.Reason.LIMIT, "vault full");
        if (r.updatedMillis == 0) r.updatedMillis = clock.getAsLong();
        byte[] plain = r.toBytes();
        try {
            sl.put(r.id, VaultCrypto.seal(dek, VaultFormat.entryAad(r.id), plain));
        } finally { Bin.wipe(plain); }
        idx.put(r.id, r.listFields());
        return r.id;
    }

    /** Inserts or replaces; assigns a random id when {@code r.id} is null. Does not wipe {@code r}. */
    public synchronized String put(VaultRecord r) throws VaultException {
        requireUnlocked(); touch();
        return sealInto(r, index, sealed);
    }

    public synchronized void delete(String id) throws VaultException {
        requireUnlocked(); touch();
        if (index.remove(id) == null) throw new VaultException(VaultException.Reason.NOT_FOUND, "no such entry");
        sealed.remove(id);
    }

    /** Decrypts every entry for a backup. The caller wipes each record. */
    public synchronized List<VaultRecord> exportRecords() throws VaultException {
        requireUnlocked();
        List<VaultRecord> out = new ArrayList<>();
        try {
            for (String id : index.keySet()) out.add(get(id));
        } catch (VaultException e) { for (VaultRecord r : out) r.wipe(); throw e; }
        return out;
    }

    /** A prepared import: new state built off to the side, applied only by {@link #commit}. */
    public static final class Staged {
        private final Map<String, VaultEntry> index;
        private final Map<String, VaultCrypto.Sealed> sealed;
        private final int count;
        private long generation;
        private Staged(Map<String, VaultEntry> i, Map<String, VaultCrypto.Sealed> s, int count) { index = i; sealed = s; this.count = count; }
        /** Number of entries the import writes. */
        public int count() { return count; }
    }

    /**
     * Builds the result of an import without changing this session (transactional step 1). Replace
     * starts empty; merge keeps the newer of two entries with the same id and adds the rest. Any
     * failure throws and leaves the session untouched. Wipes nothing.
     */
    public synchronized Staged stageImport(List<VaultRecord> records, boolean replace) throws VaultException {
        requireUnlocked(); touch();
        Map<String, VaultEntry> idx = replace ? new LinkedHashMap<>() : new LinkedHashMap<>(index);
        Map<String, VaultCrypto.Sealed> sl = replace ? new LinkedHashMap<>() : new LinkedHashMap<>(sealed);
        int n = 0;
        for (VaultRecord r : records) {
            VaultEntry have = idx.get(r.id);
            if (have != null && have.updatedMillis >= r.updatedMillis) continue;
            sealInto(r, idx, sl);
            n++;
        }
        return new Staged(idx, sl, n);
    }

    /** Serialises a staged state (next generation) without applying it, so the caller can write it first. */
    public synchronized byte[] serializeStaged(Staged st) throws VaultException {
        requireUnlocked();
        st.generation = generation + 1;
        return encode(st.index, st.sealed, st.generation);
    }

    /** Applies a staged state after its file was written (transactional step 2). */
    public synchronized void commit(Staged st) throws VaultException {
        requireUnlocked();
        index = st.index; sealed = st.sealed;
        if (st.generation > generation) generation = st.generation;
    }

    /** Convenience for in-memory use: stage and commit. Returns the number of entries written. */
    public synchronized int importRecords(List<VaultRecord> records, boolean replace) throws VaultException {
        Staged st = stageImport(records, replace);
        commit(st);
        return st.count;
    }

    /** The complete file image for an atomic write; advances the generation. Entries are not re-encrypted. */
    public synchronized byte[] serialize() throws VaultException {
        requireUnlocked();
        generation++;
        return encode(index, sealed, generation);
    }

    private byte[] encode(Map<String, VaultEntry> idx, Map<String, VaultCrypto.Sealed> sl, long gen) throws VaultException {
        byte[] header = VaultFormat.headerBytes(flags, gen, wrapped);
        byte[] plainIndex = VaultFormat.encodeIndex(idx);
        VaultCrypto.Sealed ix;
        try { ix = VaultCrypto.seal(dek, VaultFormat.indexAad(header), plainIndex); } finally { Bin.wipe(plainIndex); }
        Bin.Out o = new Bin.Out().raw(header).raw(ix.iv).bytes(ix.ct).u32(sl.size());
        for (Map.Entry<String, VaultCrypto.Sealed> e : sl.entrySet()) {
            byte[] id = e.getKey().getBytes(StandardCharsets.UTF_8);
            o.u8(id.length).raw(id).raw(e.getValue().iv).bytes(e.getValue().ct);
        }
        return o.toByteArray();
    }

    synchronized void register(VaultTypingSource s) throws VaultException { requireUnlocked(); sources.add(s); }
    synchronized void unregister(VaultTypingSource s) { sources.remove(s); }

    private static String newId() {
        StringBuilder sb = new StringBuilder();
        for (byte b : VaultCrypto.random(16)) sb.append(String.format("%02x", b));
        return sb.toString();
    }

    private static boolean isId(String s) { return s.length() == 32 && s.chars().allMatch(c -> Character.digit(c, 16) >= 0); }

    @Override public String toString() { return "VaultSession[" + (dek == null ? "locked" : "unlocked") + "]"; }
}
