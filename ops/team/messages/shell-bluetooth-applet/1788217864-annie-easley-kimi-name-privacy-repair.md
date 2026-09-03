# Annie Easley — Kimi address-shaped name privacy repair

- Timestamp: 2026-08-31T17:11:04-06:00
- Rejected exact candidate: `ecadc745fdea1e22cbbcfcbcbab1738507231b8c`
- Confirmed defect: a non-empty sanitized adapter/device display name may itself equal a canonical Bluetooth address and currently reaches labels and accessibility text verbatim.
- Repair contract: use the existing public protocol canonical-address predicate on the trimmed name, never the private/raw address field, and choose the existing ordinal adapter or class-specific device fallback. Add focused mutation-sensitive presentation coverage and retain the pure boundary.
- Scope/lane: pure presentation source/test plus concise owning documentation; executable lanes remain released pending direct static gates.
