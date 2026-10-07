# Battery startup material finding

- Time: 2026-10-07T14:44:43Z
- Base: `46e6a74dc0de6b279ca8c2a24e50d3f86634d925`
- Cause: UPower adapter's synchronous missing-owner check returns unavailable without activating installed UPower.
- Change: GetNameOwner and StartServiceByName are asynchronous and bounded to 3 seconds per call; successful activation is followed by fresh unique-owner resolution. Superseded refresh/owner and stopped-generation replies are discarded before any truth publication.
- Evidence next: Seven private activation behaviors through the same production battery composition, plus existing adapter/production gates and documentation validation. Dedicated qinda build only.
