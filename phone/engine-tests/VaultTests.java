import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Random;
import java.util.Set;
import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import org.qindaqt.link.vault.*;

/** Vault format, tamper detection, backup, generator, autotype and typing-source tests. Throwaway data only. */
public final class VaultTests {
    static int checks;

    static void check(boolean ok, String what) {
        checks++;
        if (!ok) { System.err.println("FAIL: " + what); System.exit(1); }
    }

    /** In-memory KEK standing in for the Keystore key. */
    static final class TestWrapper implements DekWrapper {
        final byte[] kek;
        TestWrapper(byte[] kek) { this.kek = kek; }
        public byte[] wrap(byte[] dek) throws GeneralSecurityException {
            byte[] iv = new byte[12]; new java.security.SecureRandom().nextBytes(iv);
            Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
            c.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(kek, "AES"), new GCMParameterSpec(128, iv));
            byte[] ct = c.doFinal(dek);
            byte[] r = Arrays.copyOf(iv, 12 + ct.length);
            System.arraycopy(ct, 0, r, 12, ct.length);
            return r;
        }
        public byte[] unwrap(byte[] w) throws GeneralSecurityException {
            Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
            c.init(Cipher.DECRYPT_MODE, new SecretKeySpec(kek, "AES"), new GCMParameterSpec(128, Arrays.copyOf(w, 12)));
            return c.doFinal(w, 12, w.length - 12);
        }
    }

    static final byte[] KEK = new byte[32];
    static long now = 1_000_000L;

    static VaultRecord rec(String title, String user, String pw) {
        VaultRecord r = new VaultRecord();
        r.title = title; r.username = user; r.url = "https://example.invalid/" + title;
        r.setPassword(pw).setNotes("note-" + title);
        return r;
    }

    static VaultSession fresh() throws Exception { return VaultSession.create(new TestWrapper(KEK), 0, () -> now); }

    static boolean fails(byte[] file, VaultException.Reason... ok) {
        try { VaultSession.open(file, new TestWrapper(KEK), () -> now, 0).lock(); return false; }
        catch (VaultException e) { return ok.length == 0 || Arrays.asList(ok).contains(e.reason()); }
        catch (Exception e) { return false; }
    }

    static void formatAndTamper() throws Exception {
        VaultSession s = fresh();
        String a = s.put(rec("alpha", "ann", "pw-A-1")), b = s.put(rec("beta", "bob", "pw-B-2"));
        check(s.size() == 2, "two entries");
        byte[] file = s.serialize();
        VaultSession t = VaultSession.open(file, new TestWrapper(KEK), () -> now, 0);
        check(t.size() == 2, "reopen count");
        VaultRecord r = t.get(a);
        check(r.passwordString().equals("pw-A-1") && r.notesString().equals("note-alpha") && r.username.equals("ann"), "round trip");
        r.wipe();
        check(r.password.length == 0, "wipe empties record");
        check(t.search("BOB").size() == 1 && t.search("example.invalid").size() == 2 && t.search("zzz").isEmpty(), "search list fields");
        check(t.search("pw-A").isEmpty() && t.search("note").isEmpty(), "search never reaches secrets");
        // Edit then delete.
        VaultRecord e = t.get(b); e.title = "beta2"; e.updatedMillis = 0; t.put(e); e.wipe();
        t.delete(a);
        VaultSession u = VaultSession.open(t.serialize(), new TestWrapper(KEK), () -> now, 0);
        check(u.size() == 1 && u.list().get(0).title.equals("beta2"), "edit+delete persisted");
        // The file must not contain any plaintext field.
        String raw = new String(file, StandardCharsets.ISO_8859_1);
        check(!raw.contains("alpha") && !raw.contains("ann") && !raw.contains("example.invalid") && !raw.contains("pw-A"), "no plaintext in file");

        // Flip every byte in turn (header, wrapped key, index, entries): each must fail closed.
        int bad = 0;
        for (int i = 0; i < file.length; i++) {
            byte[] f = file.clone(); f[i] ^= 0x01;
            if (!fails(f)) { bad++; System.err.println("byte " + i + " accepted"); }
        }
        check(bad == 0, "every single-byte flip is rejected");
        check(fails(Arrays.copyOf(file, file.length - 1)), "truncation rejected");
        check(fails(Arrays.copyOf(file, file.length + 1)), "trailing byte rejected");
        check(fails(new byte[0]) && fails("QLVT".getBytes(StandardCharsets.US_ASCII)), "empty/short rejected");
        byte[] wrongKek = new byte[32]; wrongKek[0] = 1;
        try { VaultSession.open(file, new TestWrapper(wrongKek), () -> now, 0); check(false, "wrong KEK opened"); }
        catch (VaultException ex) { check(ex.reason() == VaultException.Reason.TAMPERED, "wrong KEK is tampered"); }
        // Version byte: unsupported version is its own reason.
        byte[] v = file.clone(); v[5] = 2;
        check(fails(v, VaultException.Reason.UNSUPPORTED_VERSION), "version 2 unsupported");
    }

    /** Swaps the sealed bodies of the last two entries, keeping each id in place. */
    static byte[] swapEntries(byte[] f) {
        // Locate entries by scanning from the end: each = 1 + 32 (id) + 12 + 4 + len.
        int n = f.length;
        // walk forward instead: parse header + index to find entry start.
        int p = 4 + 2 + 1 + 8; int wl = ((f[p] & 0xff) << 8) | (f[p + 1] & 0xff); p += 2 + wl;
        p += 12; p += 4 + u32(f, p); p += 4;
        int s1 = p; int l1 = 1 + 32 + 12 + 4 + u32(f, p + 1 + 32 + 12);
        int s2 = s1 + l1; int l2 = 1 + 32 + 12 + 4 + u32(f, s2 + 1 + 32 + 12);
        byte[] o = f.clone();
        // Equal lengths in this test (same-sized entries); swap IV+len+ct but keep ids.
        int body = 12 + 4 + (l1 - 1 - 32 - 12 - 4);
        if (l1 != l2) throw new IllegalStateException("fixture entries differ in size");
        System.arraycopy(f, s2 + 33, o, s1 + 33, body);
        System.arraycopy(f, s1 + 33, o, s2 + 33, body);
        return o;
    }
    static int u32(byte[] b, int p) { return ((b[p] & 0xff) << 24) | ((b[p + 1] & 0xff) << 16) | ((b[p + 2] & 0xff) << 8) | (b[p + 3] & 0xff); }

    static void swapFixture() throws Exception {
        VaultSession s = fresh();
        s.put(rec("one", "u1", "same-length-1")); s.put(rec("two", "u2", "same-length-2"));
        check(fails(swapEntries(s.serialize())), "swap of equal-size entries rejected");
        // Drop an entry from the file but keep the count: index mismatch.
        s.lock();
    }


    /** Wrapper whose unwrap throws a chosen failure (stands in for Keystore/BiometricPrompt errors). */
    static final class FailingWrapper implements DekWrapper {
        final Exception failure;
        FailingWrapper(Exception f) { failure = f; }
        public byte[] wrap(byte[] d) throws GeneralSecurityException { throw new GeneralSecurityException("x"); }
        public byte[] unwrap(byte[] w) throws GeneralSecurityException {
            if (failure instanceof RuntimeException) throw (RuntimeException) failure;
            throw (GeneralSecurityException) failure;
        }
    }

    static VaultException.Reason openReason(byte[] file, DekWrapper w) {
        try { VaultSession.open(file, w, () -> now, 0).lock(); return null; } catch (VaultException e) { return e.reason(); }
    }

    static void lifecycle() throws Exception {
        VaultSession s = fresh();
        s.put(rec("x", "u", "p"));
        byte[] file = s.serialize();
        check(openReason(file, new FailingWrapper(new javax.crypto.AEADBadTagException("t"))) == VaultException.Reason.TAMPERED, "AEAD bad tag is tampered");
        check(openReason(file, new FailingWrapper(new DekWrapper.KeyInvalidatedException("k"))) == VaultException.Reason.KEY_INVALIDATED, "invalidated key");
        check(openReason(file, new FailingWrapper(new GeneralSecurityException("user not authenticated"))) == VaultException.Reason.KEY_UNAVAILABLE, "auth failure is retryable");
        check(openReason(file, new FailingWrapper(new java.security.ProviderException("keystore busy"))) == VaultException.Reason.KEY_UNAVAILABLE, "provider error is retryable");
        check(openReason(file, new FailingWrapper(new javax.crypto.BadPaddingException("p"))) == VaultException.Reason.KEY_UNAVAILABLE, "non-AEAD padding error is retryable");
        VaultLifecycle.State st;
        // The state machine: only integrity-type failures reach ERROR (the state that offers Restore).
        for (VaultException.Reason r : VaultException.Reason.values()) {
            st = VaultLifecycle.afterOpenFailure(r);
            boolean integrity = r == VaultException.Reason.TAMPERED || r == VaultException.Reason.CORRUPT
                || r == VaultException.Reason.UNSUPPORTED_VERSION || r == VaultException.Reason.ROLLBACK;
            check((st == VaultLifecycle.State.ERROR) == integrity, "ERROR only for integrity failures: " + r);
        }
        check(VaultLifecycle.afterOpenFailure(VaultException.Reason.KEY_UNAVAILABLE) == VaultLifecycle.State.LOCKED, "retry stays LOCKED");
        check(VaultLifecycle.afterOpenFailure(VaultException.Reason.KEY_INVALIDATED) == VaultLifecycle.State.INVALIDATED, "invalidated state");
        for (VaultLifecycle.State x : VaultLifecycle.State.values()) {
            boolean ok = x == VaultLifecycle.State.NONE || x == VaultLifecycle.State.INVALIDATED || x == VaultLifecycle.State.ERROR;
            check(VaultLifecycle.canRestore(x) == ok, "restore allowed only when absent or unusable: " + x);
        }
        check(!VaultLifecycle.canRestore(VaultLifecycle.State.LOCKED) && !VaultLifecycle.canRestore(VaultLifecycle.State.UNLOCKED), "no restore over a locked or open vault");
        check(VaultLifecycle.restoreNeedsReplaceConfirm(VaultLifecycle.State.ERROR) && !VaultLifecycle.restoreNeedsReplaceConfirm(VaultLifecycle.State.NONE), "replace confirmation");
        check(VaultLifecycle.canCreate(VaultLifecycle.State.NONE) && VaultLifecycle.canCreate(VaultLifecycle.State.RESTORE_READY) && !VaultLifecycle.canCreate(VaultLifecycle.State.LOCKED), "create gate");
        check(VaultLifecycle.nextKeySlot(0, true) == 1 && VaultLifecycle.nextKeySlot(1, true) == 0 && VaultLifecycle.nextKeySlot(-1, true) == 1 && VaultLifecycle.nextKeySlot(-1, false) == 0, "fresh key slot");
        // Slot and StrongBox flags round trip through the authenticated header.
        VaultSession b = VaultSession.create(new TestWrapper(KEK), VaultSession.FLAG_KEY_B | VaultSession.FLAG_STRONGBOX, () -> now);
        b.put(rec("y", "u", "p"));
        byte[] bf = b.serialize();
        check(VaultSession.flagsOf(bf) == 3, "flags in header");
        VaultSession bo = VaultSession.open(bf, new TestWrapper(KEK), () -> now, 0);
        check(bo.keySlot() == 1 && bo.strongBox(), "slot B and strongbox restored");
        // A failed wrap is retryable, not corrupt.
        try { VaultSession.create(new FailingWrapper(new GeneralSecurityException("x")), 0, () -> now); check(false, "create with failing wrapper"); }
        catch (VaultException e) { check(e.reason() == VaultException.Reason.KEY_UNAVAILABLE, "failed wrap is retryable"); }
    }

    static void rollbackAndTransactions() throws Exception {
        VaultSession s = fresh();
        s.put(rec("x", "u", "p"));
        byte[] g1 = s.serialize();
        byte[] g2 = s.serialize();
        check(VaultSession.generationOf(g1) == 1 && VaultSession.generationOf(g2) == 2 && s.generation() == 2, "generation advances per write");
        check(VaultSession.open(g2, new TestWrapper(KEK), () -> now, 2).isUnlocked(), "current generation opens");
        try { VaultSession.open(g1, new TestWrapper(KEK), () -> now, 2); check(false, "old generation opened"); }
        catch (VaultException e) { check(e.reason() == VaultException.Reason.ROLLBACK, "older generation refused"); }
        check(VaultLifecycle.afterOpenFailure(VaultException.Reason.ROLLBACK) == VaultLifecycle.State.ERROR, "rollback offers recovery");

        // Transactional import: a failing record leaves the session exactly as it was.
        VaultSession t = fresh();
        String keep = t.put(rec("keep", "k", "pw-keep"));
        List<VaultRecord> bad = new ArrayList<>();
        bad.add(rec("good", "g", "pw-g")); bad.get(0).id = "0123456789abcdef0123456789abcdef";
        VaultRecord empty = new VaultRecord(); empty.id = "fedcba9876543210fedcba9876543210"; bad.add(empty);
        try { t.stageImport(bad, true); check(false, "bad stage accepted"); } catch (VaultException e) { check(e.reason() == VaultException.Reason.INVALID, "bad record aborts stage"); }
        check(t.size() == 1 && t.get(keep).passwordString().equals("pw-keep"), "failed stage leaves the session untouched");
        List<VaultRecord> good = new ArrayList<>();
        VaultRecord r = rec("new", "n", "pw-new"); r.id = "0123456789abcdef0123456789abcdef"; good.add(r);
        VaultSession.Staged st = t.stageImport(good, true);
        check(t.size() == 1 && st.count() == 1, "staging does not apply");
        long before = t.generation();
        byte[] staged = t.serializeStaged(st);
        check(t.generation() == before && t.size() == 1, "serialising a stage does not apply it");
        VaultSession u = VaultSession.open(staged, new TestWrapper(KEK), () -> now, 0);
        check(u.size() == 1 && u.list().get(0).title.equals("new"), "staged bytes hold the new state");
        t.commit(st);
        check(t.size() == 1 && t.list().get(0).title.equals("new") && t.generation() == before + 1, "commit applies after the write");
        // A discarded stage changes nothing.
        t.stageImport(new ArrayList<>(), true);
        check(t.size() == 1, "discarded stage is a no-op");
    }


    static void keyPresence() throws Exception {
        check(KeyPresence.probe(() -> true) == KeyPresence.PRESENT, "present");
        check(KeyPresence.probe(() -> false) == KeyPresence.ABSENT, "absent");
        java.util.concurrent.Callable<Boolean>[] failures = new java.util.concurrent.Callable[] {
            (java.util.concurrent.Callable<Boolean>) () -> { throw new GeneralSecurityException("keystore"); },
            (java.util.concurrent.Callable<Boolean>) () -> { throw new java.io.IOException("io"); },
            (java.util.concurrent.Callable<Boolean>) () -> { throw new java.security.ProviderException("busy"); },
            (java.util.concurrent.Callable<Boolean>) () -> { throw new IllegalStateException("boot"); },
            (java.util.concurrent.Callable<Boolean>) () -> { throw new java.security.KeyStoreException("ks"); } };
        for (java.util.concurrent.Callable<Boolean> f : failures)
            check(KeyPresence.probe(f) == KeyPresence.ERROR, "any exception is ERROR, never ABSENT");
        check(KeyPresence.any(KeyPresence.ABSENT, KeyPresence.ERROR) == KeyPresence.ERROR, "unknown slot: error wins over absent");
        check(KeyPresence.any(KeyPresence.ERROR, KeyPresence.PRESENT) == KeyPresence.PRESENT, "unknown slot: present wins");
        check(KeyPresence.any(KeyPresence.ABSENT, KeyPresence.ABSENT) == KeyPresence.ABSENT, "unknown slot: both absent");

        int none = VaultLifecycle.NO_SLOT;
        for (int slot = 0; slot <= 1; slot++) {
            check(VaultLifecycle.decideLocked(slot, KeyPresence.ERROR, none) == VaultLifecycle.Decision.KEY_STORE_RETRY, "keystore error is a retry");
            check(VaultLifecycle.decideLocked(slot, KeyPresence.PRESENT, none) == VaultLifecycle.Decision.LOCKED, "present is locked");
            check(VaultLifecycle.decideLocked(slot, KeyPresence.ABSENT, none) == VaultLifecycle.Decision.INVALIDATED, "definitely absent is invalidated");
            check(VaultLifecycle.decideLocked(slot, KeyPresence.PRESENT, slot) == VaultLifecycle.Decision.INVALIDATED, "persisted loss for the file's slot holds");
            check(VaultLifecycle.decideLocked(slot, KeyPresence.ERROR, slot) == VaultLifecycle.Decision.INVALIDATED, "persisted definite loss survives a later keystore error");
            check(VaultLifecycle.decideLocked(slot, KeyPresence.PRESENT, 1 - slot) == VaultLifecycle.Decision.LOCKED, "stale marker for the other slot is ignored (crash during replace)");
            check(VaultLifecycle.decideLocked(slot, KeyPresence.ERROR, 1 - slot) == VaultLifecycle.Decision.KEY_STORE_RETRY, "stale marker plus error is still just a retry");
        }
        check(VaultLifecycle.decideLocked(-1, KeyPresence.ERROR, none) == VaultLifecycle.Decision.KEY_STORE_RETRY, "unreadable header plus error is a retry");
    }

    static void sessionRules() throws Exception {
        VaultSession s = fresh();
        String id = s.put(rec("x", "u", "p"));
        s.lock();
        check(!s.isUnlocked(), "locked");
        for (int i = 0; i < 4; i++) {
            try {
                switch (i) { case 0: s.list(); break; case 1: s.get(id); break; case 2: s.put(rec("y", "u", "p")); break; default: s.serialize(); }
                check(false, "locked op " + i + " succeeded");
            } catch (VaultException e) { check(e.reason() == VaultException.Reason.LOCKED, "locked reason " + i); }
        }
        // Idle lock.
        VaultSession t = fresh();
        t.put(rec("x", "u", "p"));
        now += VaultSession.IDLE_LOCK_MILLIS - 1;
        check(!t.lockIfIdle() && t.isUnlocked(), "not idle yet");
        t.touch();
        now += VaultSession.IDLE_LOCK_MILLIS;
        check(t.lockIfIdle() && !t.isUnlocked(), "idle lock");
        // Empty entries refused.
        try { fresh().put(new VaultRecord()); check(false, "empty entry accepted"); } catch (VaultException e) { check(e.reason() == VaultException.Reason.INVALID, "empty entry refused"); }
        check(!rec("t", "u", "secret").toString().contains("secret"), "toString redacts");
    }

    static void backup() throws Exception {
        VaultSession s = fresh();
        String a = s.put(rec("alpha", "ann", "pw-A")), b = s.put(rec("beta", "bob", "pw-B"));
        char[] pass = "correct horse battery staple".toCharArray();
        List<VaultRecord> recs = s.exportRecords();
        byte[] file = VaultBackup.export(recs, pass, VaultBackup.MIN_ITERATIONS);
        for (VaultRecord r : recs) r.wipe();
        check(VaultBackup.iterationsOf(file) == VaultBackup.MIN_ITERATIONS, "iterations in header");
        String raw = new String(file, StandardCharsets.ISO_8859_1);
        check(!raw.contains("alpha") && !raw.contains("pw-A"), "no plaintext in backup");
        List<VaultRecord> back = VaultBackup.read(file, pass);
        check(back.size() == 2 && back.get(0).passwordString().equals("pw-A"), "backup round trip");
        // Merge keeps newer; replace drops others.
        VaultSession t = fresh();
        t.put(rec("local", "l", "pw-L"));
        check(t.importRecords(back, false) == 2 && t.size() == 3, "merge adds");
        check(t.importRecords(back, false) == 0, "merge of identical is no-op");
        check(t.importRecords(back, true) == 2 && t.size() == 2, "replace");
        for (VaultRecord r : back) r.wipe();
        try { VaultBackup.read(file, "wrong passphrase here".toCharArray()); check(false, "wrong passphrase opened"); }
        catch (VaultException e) { check(e.reason() == VaultException.Reason.WRONG_PASSPHRASE, "wrong passphrase"); }
        // Tamper: flip bytes in header (iteration count, salt), iv, tag.
        for (int i : new int[]{8, 9, 10, 11, 12, 20, 30, 40, file.length - 1}) {
            byte[] f = file.clone(); f[i] ^= 0x01;
            try { VaultBackup.read(f, pass); check(false, "backup byte " + i + " accepted"); }
            catch (VaultException e) { check(e.reason() != null, "backup tamper " + i); }
        }
        // A downgraded iteration count (below minimum) is refused before any KDF work.
        byte[] low = file.clone(); low[8] = 0; low[9] = 0; low[10] = 0; low[11] = 100;
        try { VaultBackup.read(low, pass); check(false, "low iterations accepted"); }
        catch (VaultException e) { check(e.reason() == VaultException.Reason.CORRUPT, "low iterations refused"); }
        try { VaultBackup.export(new ArrayList<>(), "short".toCharArray(), VaultBackup.MIN_ITERATIONS); check(false, "weak passphrase exported"); }
        catch (VaultException e) { check(e.reason() == VaultException.Reason.WEAK_PASSPHRASE, "weak passphrase refused"); }
        try { VaultBackup.export(new ArrayList<>(), pass, 1000); check(false, "low iterations exported"); }
        catch (VaultException e) { check(e.reason() == VaultException.Reason.INVALID, "export iteration floor"); }
        int cal = VaultBackup.calibrate(50);
        check(cal >= VaultBackup.MIN_ITERATIONS && cal <= VaultBackup.MAX_ITERATIONS, "calibrate clamps");
    }

    static void strength() {
        check(!PassphraseStrength.check("short".toCharArray()).ok, "short refused");
        check(PassphraseStrength.check("abcdefghijkl".toCharArray()).ok, "12 chars ok");
        check(!PassphraseStrength.check("abcdefghijk".toCharArray()).ok, "11 chars refused");
        check(!PassphraseStrength.check("ab cd ef gh".toCharArray()).ok, "4 tiny words refused");
        check(PassphraseStrength.check("gold tree blue moon".toCharArray()).ok, "4 real words ok");
        check(!PassphraseStrength.check("gold tree".toCharArray()).ok, "2 short words refused");
        check(!PassphraseStrength.check("aaaaaaaaaaaa".toCharArray()).ok, "weak 12 chars refused");
        check(PassphraseStrength.check("Tr0ub4dor&3-xylophone".toCharArray()).bits > PassphraseStrength.check("aaaaaaaaaaaa".toCharArray()).bits, "repeats weaker");
    }

    static void generator() {
        PasswordGenerator g = new PasswordGenerator(new Random(7));
        for (int len = PasswordGenerator.MIN_LENGTH; len <= PasswordGenerator.MAX_LENGTH; len += 7) {
            char[] p = g.password(len, true, true, true, true, false);
            check(p.length == len, "length " + len);
            String s = new String(p);
            check(s.chars().anyMatch(Character::isLowerCase) && s.chars().anyMatch(Character::isUpperCase)
                && s.chars().anyMatch(Character::isDigit) && s.chars().anyMatch(c -> !Character.isLetterOrDigit(c)), "every class present at " + len);
        }
        String only = new String(g.password(40, false, false, true, false, false));
        check(only.chars().allMatch(Character::isDigit), "digits only");
        String amb = new String(g.password(128, true, true, true, false, true));
        check(amb.chars().noneMatch(c -> "0O1lI|".indexOf(c) >= 0), "ambiguous excluded");
        for (int[] bad : new int[][]{{7}, {129}}) { try { g.password(bad[0], true, true, true, true, false); check(false, "length accepted"); } catch (IllegalArgumentException e) { check(true, ""); } }
        try { g.password(12, false, false, false, false, false); check(false, "no classes"); } catch (IllegalArgumentException e) { check(true, ""); }
        // Distribution: 62 symbols, 62000 draws; each within a generous band.
        int[] hist = new int[128];
        for (int i = 0; i < 1000; i++) for (char c : new PasswordGenerator(new Random(i)).password(62, true, true, true, false, false)) hist[c]++;
        double mean = 62000.0 / 62;
        int lo = 0;
        for (char c = 'a'; c <= 'z'; c++) if (hist[c] < mean * 0.85 || hist[c] > mean * 1.15) lo++;
        check(lo == 0, "letter distribution within 15% of uniform");
        char[] pp = new PasswordGenerator().passphrase(6, "-", false, false);
        check(new String(pp).split("-").length == 6, "six words");
        Set<String> uniq = new HashSet<>();
        for (int i = 0; i < 50; i++) uniq.add(new String(new PasswordGenerator().passphrase(4, " ", true, true)));
        check(uniq.size() == 50, "passphrases differ");
        try { g.passphrase(2, "-", false, false); check(false, "2 words"); } catch (IllegalArgumentException e) { check(true, ""); }
    }

    static void autotype() throws Exception {
        List<AutotypeSequence.Item> it = AutotypeSequence.parse(VaultRecord.DEFAULT_AUTOTYPE).items();
        check(it.size() == 4 && it.get(0).kind == AutotypeSequence.Kind.USERNAME && it.get(1).text.equals("TAB")
            && it.get(2).kind == AutotypeSequence.Kind.PASSWORD && it.get(3).text.equals("ENTER"), "default parses");
        it = AutotypeSequence.parse("hi {{}x{}} {DELAY 250}{KEY ESC}{URL}").items();
        check(it.get(0).kind == AutotypeSequence.Kind.LITERAL && it.get(0).text.equals("hi {x} "), "escapes merge into literal");
        check(it.get(1).ms == 250 && it.get(2).text.equals("ESC") && it.get(3).kind == AutotypeSequence.Kind.URL, "tokens");
        check(AutotypeSequence.parse("").items().isEmpty(), "empty ok");
        String[] bad = {"{", "}", "{USERNAME", "{NOPE}", "{DELAY}", "{DELAY -1}", "{DELAY 10001}", "{DELAY 1x}", "{KEY}", "{KEY F13}",
            "{USERNAME x}", "{username}", "{}", "{{}}", "{DELAY 9000}{DELAY 9000}{DELAY 9000}{DELAY 9000}", "a".repeat(257),
            "{TAB}".repeat(65), "{DELAY 123456}"};
        for (String b : bad) {
            try { AutotypeSequence.parse(b); check(false, "accepted malformed: " + b.length()); }
            catch (AutotypeSequence.SyntaxException e) { check(e.position() >= 0, "position"); }
        }
    }

    static void typingSource() throws Exception {
        VaultSession s = fresh();
        VaultRecord r = rec("t", "user1", "pw-T");
        r.autotype = "{USERNAME}{TAB}{DELAY 100}{PASSWORD}{ENTER}";
        String id = s.put(r);
        VaultTypingSource src = new VaultTypingSource(s, id, VaultTypingSource.Field.AUTOTYPE);
        List<TypingStep> st = src.steps();
        check(st.size() == 5 && st.get(0).textString().equals("user1") && st.get(1).key.equals("TAB") && st.get(2).delayMs == 100
            && st.get(3).textString().equals("pw-T") && st.get(4).key.equals("ENTER"), "autotype steps");
        byte[] live = st.get(3).textBytes();
        src.close();
        check(Arrays.equals(live, new byte[live.length]), "close zeroes secret text");
        try { src.steps(); check(false, "steps after close"); } catch (VaultException e) { check(e.reason() == VaultException.Reason.LOCKED, "closed source locked"); }
        VaultTypingSource pw = new VaultTypingSource(s, id, VaultTypingSource.Field.PASSWORD);
        byte[] buf = pw.steps().get(0).textBytes();
        s.lock();   // locking zeroes live sources too
        check(Arrays.equals(buf, new byte[buf.length]), "lock zeroes open source");
        try { pw.steps(); check(false, "steps after lock"); } catch (VaultException e) { check(e.reason() == VaultException.Reason.LOCKED, "lock invalidates source"); }
        try { new VaultTypingSource(s, id, VaultTypingSource.Field.USERNAME); check(false, "source on locked"); }
        catch (VaultException e) { check(e.reason() == VaultException.Reason.LOCKED, "refuses while locked"); }
        VaultSession m = fresh();
        VaultRecord bad = rec("b", "u", "p"); bad.autotype = "{NOPE}";
        String bid = m.put(bad);
        try { new VaultTypingSource(m, bid, VaultTypingSource.Field.AUTOTYPE); check(false, "malformed autotype"); }
        catch (VaultException e) { check(e.reason() == VaultException.Reason.INVALID, "malformed autotype refused"); }
        VaultTypingSource un = new VaultTypingSource(m, bid, VaultTypingSource.Field.USERNAME);
        check(un.steps().size() == 1 && un.steps().get(0).textString().equals("u"), "username field");
        un.close();
    }

    public static void main(String[] a) throws Exception {
        formatAndTamper(); swapFixture(); lifecycle(); keyPresence(); rollbackAndTransactions(); sessionRules(); backup(); strength(); generator(); autotype(); typingSource();
        System.out.println("VaultTests ok: " + checks + " checks");
    }
}
