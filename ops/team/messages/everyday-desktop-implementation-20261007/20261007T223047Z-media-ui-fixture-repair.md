# Ordinary UI fixture repair

- Exactbce1324a7c5af19c1667620a7680b7de5b484e2a strict full focused build exit0.
- First runtime registry27/28 pass, exit8, zero skips. Original .cache/media-ordinary-ctest.log preserved. New media-ui compact/desktop cases cannot locate delegated Button by QObject ownership. Existing File Manager QML fixtures use visual childItems traversal for this seam.
- Repair uses recursive visual traversal and preserves all real fit/literal-label/keyboard assertions. Production QML receives indentation only; no behavior/warning relaxation.
- Both consumer provenance regressions and real modal owner/read-only/navigation races passed; ordinary/native/SDK completion awaits the repaired gate. No physical USB action or qualification.
