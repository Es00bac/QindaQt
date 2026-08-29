# Cora Vale midpoint: 25-row visual set reviewed and accepted for comparison

- **Timestamp:** 2026-08-28T02:11:49Z
- **Status:** generation/review clean; normal comparison starting

The serial process-isolated generator passed 25/25 after explicitly removing
the prior set. I inspected all five contact sheets and all 25 original-resolution
copies under the ignored directory
`build/controls-debug/controls-review-1787882700`; the manager independently
confirmed all five contact sheets. Complete DegradedNotice text, settled Slider,
required/error, disabled, busy, ordinary, checked, theme choice, and form states
are visible without clipping or overlap at every theme/profile/scale.

The image viewer caches repeated pixel content and sometimes displayed obsolete
Dusk/macOS previews for new paths. Direct evidence resolves that tool artifact:
all 25 review copies match their source baselines at ImageMagick AE=0, OCR reads
the complete provider message in every row (five compact rows wrap the last two
words across lines), and the contact sheets are freshly composed from the current
bytes. Diagnostic `/tmp` images were removed; review copies remain ignored.

Exact source/review SHA-256 values:

```text
7be12aea37d56ff72651263280dfe5f98dd41a8bc0bf74de543dc6f5143dd268  100/qinda-dark-compact.png
08a3bdd740566a6a4bf519e63f33d65d9b0db4a1398f0145748437a5bb6ee410  100/qinda-dark-large.png
deef9e16b7c0aa171a3a5fc8f7b3c818bbf9dba08227ce890098fd6e7ae561a0  100/qinda-dark-ordinary.png
9ff7f5758815fc2fb40c09d73f54b38a2c9cc15c7181eeccd4aa75e395ad1746  100/qinda-dusk-compact.png
caa43829d86608ecfacc1343207e783127ce9c145dd804c528a959ef8423c91c  100/qinda-dusk-large.png
32fe749cc2f52adafdd20e790537c6a9b6a6584dc3e68ae9f1d365427bdf1455  100/qinda-dusk-ordinary.png
7f9dec62c7952b6e6d7f505144af7ed9326376bd0634d4c6d717e08bfd6db72b  100/qinda-high-contrast-compact.png
d184918e1aea34fe7e1e5c966e60c4803fba0fa8267d181f6d689f3e69a513f9  100/qinda-high-contrast-large.png
c59242a77d726a64804da868f2431fda2240fa5b4f283ac4a49660bb26625e12  100/qinda-high-contrast-ordinary.png
2940edf15839d78419680aa5001b1261f029b688f9dccca6de2bb3e050e56651  100/qinda-light-compact.png
9c111b94bde3d7cf05e887433fcf6fdd7a1cf089ce63c912313aa6bd506bb812  100/qinda-light-large.png
671f8ec2a95643d2cbff1307c2d66b2e97c0d38937dd0ed0877172cef46cb486  100/qinda-light-ordinary.png
1bdce6241cb5e1e7fa409a269d67261294611aed6199da8dcdddd5db99462090  100/qinda-macos-compact.png
62ba8d9733b0162c02f6a16e04463d240bbecc8411e67e0aa43276727af3d96e  100/qinda-macos-large.png
ef82ef2f3de5b8098fc9516e2dc7181bf195e19f55cdf9a55054995fc3134f9b  100/qinda-macos-ordinary.png
74cbe8f1eaa16b23b9a837b24aaa893d306bde9cde4bcc04dd736ff3add10dd4  125/qinda-dark-ordinary.png
806a5f3072ab1a7fde36aedd5c4c4c1122aecfd0f73c7bdee5bb8281a903e372  125/qinda-dusk-ordinary.png
da27c2eaa4def64c6fcaa1bb8df9ad7c13d0f82ab772cab6ffbde105354020bf  125/qinda-high-contrast-ordinary.png
1ee18be39c9838c572c24010609634e05cfadf89a5fffd95680e5cfd2b9ccf89  125/qinda-light-ordinary.png
a688eb4c0f576bfa54716827d09539839b62553c940c40d1023488361d8fabb5  125/qinda-macos-ordinary.png
a2136c4e12efc1b8ed45de73913ad5e4f1c2b008cf8a031315c77e55566749ef  150/qinda-dark-ordinary.png
614da7b4c8d36781ffcdb4cd48264182112fa68bbefd861619bee7a5bf0c0da7  150/qinda-dusk-ordinary.png
43df162f6328b006c23de7e876fd2cfbdf3755300588e2b195d814357624625d  150/qinda-high-contrast-ordinary.png
3f5dbff5739d268503939393f010738e1107c045f4360ecdddbc5bafe22ca6ab  150/qinda-light-ordinary.png
dee9e317200ba27211a1b41247402f18069236f89f0a47ea19f11f123270bcf0  150/qinda-macos-ordinary.png
```

No normal comparison or broader gate is claimed in this midpoint; those start
now from this reviewed set.
