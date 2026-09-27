# Native icon fixture review: ACCEPT

Exact candidate `4071579d75f5e02956bcbe47fb815cd7d6d1180d` is test-only and has no blocking source finding. Fake Settings1 now returns the schema's empty-string Follow-theme default for requested `appearance.iconTheme` instead of a null QVariant, and includes the key in its invalidation signal. This restores a valid wire fixture without weakening assertions or changing production behavior. Manager owns native rerun before packaging.
