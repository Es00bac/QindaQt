# Media block replacement finding and repair

- Worker: everyday_media_delivery
- Prior source candidate: b493f3e1a29702078e242363b934aea8b0f7560c, unqualified.
- Finding: final removal could observe a replaced selected/sibling Block at a reused path before debounce while cached physical Drive identity remains unchanged.
- Repair: immediate revocation on known-path identity-interface addition, owning sibling/block identity replacement flag, no further removal step or final success across that flag. Added separate drive-object and block-attachment-same-drive final late-reply rows. Normalize missing final removal mode to Uncertain.
- Resources: source/static only; compiler lease remains Astra-owned.
- Next: superseding exact candidate, focused runtime/data-loss review and consumer gates.
