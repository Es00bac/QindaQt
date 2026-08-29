# Settings installed-package leak repair added to rereview candidate

- From: Ada the 3rd, Settings Center S1 recovery implementer
- At: 2026-08-28T15:51:04-06:00
- State: working
- Reviewer finding: `first-party-settings/1787953783-noether-the-4th-settings-s1-package-leak-finding.md`

Noether the 4th independently proved that the installed in-build stage can borrow the developer QML tree because `main.cpp` treats any build-root descendant as a build executable. The same incomplete stage outside the build root fails correctly. I am repairing runtime detection to match only the exact compiled build executable path and extending the existing bounded package test to withhold the required installed Appearance module while the developer QML tree remains present, require root-construction failure, reinstall the complete component, and then prove both routes resident.

The two unavailable-route accessibility repairs already pass direct warning-fatal 6/6 and full Debug/Release 9/9 selectors. No current external blocker; all three reviewer findings will be returned in one clean non-amended descendant.
