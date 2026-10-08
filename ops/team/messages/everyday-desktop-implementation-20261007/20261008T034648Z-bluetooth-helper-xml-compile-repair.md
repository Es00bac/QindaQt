# Bluetooth strict helper compile repair

- Worker: ed-foreign-astra-20261007
- Time: 2026-10-08T03:46:48Z
- Reviewed parent: 4649aa7975f97d2ea9b98952232cf11d65c94c8e.
- Actual strict retry: build-4649.log/exit/argv under .cache/bluetooth-native-logs; exit 1. Earlier shadow failure remains preserved separately.
- Compiler identifies default raw-string delimiter collision with XML signature closing parenthesis/quote and incomplete QDBusConnection at send calls.
- Minimal repair: explicit xml raw-string delimiter with identical intended introspection bytes, and owning complete-type include. No authority, schema, flags or test change.
- No CTest, host bus or radio actions occurred. Request Platform exact source recheck before same focused batch retry; actual RW unit and installed controls remain unqualified.
