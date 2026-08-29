# Program Manager — Font F0 ADR collision resolution

- Timestamp: 2026-08-28T19:58:20Z
- Candidate: `5d5df6a496d632a96e6d31bb04b233d2e0a0f06e`

The accepted Font candidate used ADR-0042 against its older base, while the
current manager tree already assigns ADR-0042 to the integrated Launcher model.
Integration preserves both decisions and assigns **ADR-0047** to Font F0. All
Font references are renamed together; Launcher remains ADR-0042. The temporary
candidate-local Faye profile is omitted from the product tree because the
canonical employee record and full handoff remain on this shared Team Board.
