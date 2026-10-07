# ED08 late selected-device fence

Source-only final current-device audit adds private libnm managed/state recheck before add-and-activate, so an observed interface becoming unmanaged/unavailable cannot be dispatched merely because it still exists. New fake hidden-unmanaged scenario has valid RSN/CCMP but fails before capture. Model fixture also asserts unknown/unavailable device and missing existing profile-control capability refusal. Authored tests remain unrun awaiting lease; immutable wire and r16 unchanged.
