# Mail UI verification finding — input evidence under investigation

- Updated: 2026-10-04T17:42:58Z
- Repair UI/host/accounts build exits 0 (19 steps), configured MAKEOPTS unchanged.
- First focused CTest exits 8: 2/3 tests pass (accounts/host); UI reports 6 passed/8 failed/0 skips, including setup/cleanup. Existing inbox/ribbon/dialog/fixture behaviors pass; new cases fail at opened assertion without QML errors.
- The new test waited for visibility but lacked settled geometry and clicked-signal proof. Added a bounded actual-click spy and pre-click synthetic grab; checking one password-1280 case first. Initial baseline interpretation is pending this stronger input evidence.
- No candidate verdict/push and no live credentials/auth/network/UI or installation.
