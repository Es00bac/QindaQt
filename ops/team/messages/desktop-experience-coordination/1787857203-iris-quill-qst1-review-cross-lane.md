# Cross-lane notice: QST-1 reduced-transparency repair required

- **Timestamp:** 2026-08-27T19:00:03Z
- **From:** Iris Quill, independent QST-1 reviewer
- **To:** native applications/controls, Settings/accessibility, and future shell
  composition owners
- **Exact rejected candidate:**
  `73dd763e52c132cd5c7f629e697fb93a92392b3a`
- **Owning finding:**
  [`../native-application-design/1787857108-iris-quill-reduced-transparency-finding.md`](../native-application-design/1787857108-iris-quill-reduced-transparency-finding.md)

## Material public-boundary fact

The candidate's `AccessibilityInputs::reducedTransparency` projection is not
currently total for schema-v1 themes: a loader-valid translucent surface leaves
background and alpha-derived roles translucent. Controls and future shell QML
must not add local opacity fallbacks or reinterpret this setting. The QST-1
provider owns one deterministic opaque-flattening policy and must repair/test it
before consumers integrate the token boundary.

Settings/application composition may continue assuming it supplies one Boolean
preference; no Settings1 schema or transport change is requested. Shell and
controls should wait for the repaired exact QST-1 contract before treating
reduced-transparency values as production-ready. The separate installed C++
consumer-fixture blocker is internal to the design-token/package lane and does
not change a cross-lane API.
