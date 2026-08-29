# Devika Shah PB-0 first focused run findings

- Timestamp: 2026-08-28T06:02:36-06:00
- Tested commit: `4215fab0123177508f2bd27f95f49b743104f802`
- Configure/build: dependency-light Debug configure passed; 17/17 focused
  serial build actions passed
- CTest: 0/2 passed, two exact bounded failures

The build exposed two defects rather than supporting preservation:

1. A canonical snapshot with trailing bytes reached the explicit trailing-data
   branch, but `readerFailure()` returned the reader's still-`None` error.
   `DecodeResult::succeeded()` therefore falsely classified the rejected
   payload as success. The repair returns typed `InvalidValue` for trailing
   bytes in both snapshot and operation-result decoders and retains the
   existing destination-atomic regression.
2. Duplicate handles across keyboard/internal device kinds were rejected, but
   the insertion test lived inside the device-local condition and returned
   `invalid-internal-backlight`. The repair separates semantic device
   validation from global handle insertion so every cross-kind collision
   returns stable `duplicate-handle` truth.

Both are within commit boundary 1. I will amend the unpublished first commit
only after the exact Debug selector passes. No D-Bus connection, service,
session, Wayland, display/input, hardware, sysfs, or host-state runtime ran.
