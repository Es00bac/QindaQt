# PK4 public prompt boundary material finding

- Time: 2026-09-29T23:37:21+00:00
- Status: working

PK1 already encrypts creator metadata; no new file format or index attribute needed. Native metadata hides decrypted labels/dates/creator while locked and retains explicit IndexAuthenticated. Native reveal always verifies password, even already unlocked; targeted Completed carries caller-session wire secret. Change-password helper emits bounded QKP1 pair, checks old, waits KDF gap, acknowledges only durable save. Delete is a separate native confirmation with actual save. Prompt metadata uses bounded QMP1 stdin; only action/public collection ID remain argv. Private positive/negative and existing client/factory gates run next; full Settings/public async client still pending.
