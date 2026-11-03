# ADR-0435: Decode QindaLink phone streams with FFmpeg in qindaqt-link-mirror

- **Status:** Accepted
- **Date:** 2026-11-02
- **Owners:** QindaLink (Platform workgroup)
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0433](0433-qindalink-phone-companion-architecture.md) sends each mirrored
phone app as an H.264 or HEVC stream from MediaCodec to one
`qindaqt-link-mirror` process per app. The process then needs a video
decoder.

- QindaQt had no media decoding dependency before this. KWin, Qt Quick and
  QindaTK do not decode H.264/HEVC elementary streams.
- Qt Multimedia expects containers or URLs. It does not give per-packet
  control over latency or the decoder path.
- Calling libva directly would mean writing an H.264/HEVC bitstream parser
  and a software fallback ourselves.
- The stream comes from a phone over ADB. The mirror server is our own code,
  but an attacker who controls the phone or the ADB link could feed it
  arbitrary bytes. Bitstream parsing is a classic attack surface.
- qinda decodes with radeonsi (RX 5700 XT). Other machines may lack VA-API.

The [mirror protocol reference](../reference/qindalink-mirror-v1.md) holds the
wire contract, and the [architecture page](../architecture/qindalink.md#phone-apps-on-the-desktop)
shows the modules.

## Decision

`qindaqt-link-mirror` decodes with FFmpeg's libavcodec, using libavutil and
libswscale for hardware frames and RGB conversion.

1. **Decoder path.** VA-API comes first, through libavutil's hardware device
   context. Software libavcodec is the automatic fallback:
   - when VA-API cannot open, or fails mid-stream (the rest of the session
     then stays on software and requests a keyframe);
   - never silently inside libavcodec, so the reported path is true.

   The policy is the pure `DecoderSelector` in `qindaqt_link_mirror_core`.
   Only `ffmpeg_decoder.cpp` touches FFmpeg.
2. **Untrusted input stays in the mirror process.** Only the per-app mirror
   process parses phone bitstreams. It holds no secrets beyond its own
   session token, and it never runs inside the shell, KWin, the link service
   or another QindaQt service. The following limits apply before FFmpeg
   allocates anything for a stream:
   - **Frame caps:** video frames are at most 16 MiB.
   - **Dimension caps:** at most 8192 pixels per side and 64 MP. They are
     checked in message parsing and again on every decoded picture.
   - **Backlog bound:** at most 8 packets wait for the decoder before non-key
     packets are dropped.
3. **Licence.** QindaQt is GPL-3.0-or-later, and it links the system FFmpeg
   dynamically.
   - LGPL-2.1+ and GPL-2+/3+ FFmpeg builds are compatible.
   - An FFmpeg built with non-free components is not. Portage marks the
     `fdk` and `gpl` combination `all-rights-reserved`. That build is fine for
     local builds and the user's own binhost, but binaries meant for
     publication must be built against an FFmpeg without non-free parts.
   - The mirror uses only the H.264 and HEVC decoders.
4. **Build switch.** The CMake option `QINDAQT_BUILD_LINK_MIRROR` (default
   ON) builds the executable. Configure fails with a message naming the
   option when libavcodec, libavutil or libswscale is missing. The pure core
   and its tests do not need FFmpeg.

## Consequences

- **Packaging obligation.** `gui-wm/qindaqt-desktop` must DEPEND and RDEPEND
  on `media-video/ffmpeg[vaapi]`, or configure with
  `-DQINDAQT_BUILD_LINK_MIRROR=OFF`. The overlay edit is a separate packaging
  change and is not made with this ADR.
- **Release tracking.** FFmpeg security releases now matter to QindaQt. They
  arrive through ordinary Portage updates, and the mirror picks them up
  without a rebuild unless the soname changes.
- **Copies.** VA-API pictures are downloaded to system memory and converted to
  RGB for a raster window. That costs one copy per frame at phone
  resolutions. A zero-copy dmabuf/EGL path is left for later.
- **Tests.** Decoder selection and backlog rules are unit-tested with fake
  decoders (`qindaqt.link-mirror-decoder-selection`). The FFmpeg backend is
  exercised by the `--grab` and `--probe` evidence runs.

## Revisit when

- QindaTK or Qt gains a low-latency elementary-stream decoder with VA-API
  that would remove the direct FFmpeg dependency.
- Mirror resolutions grow to where the per-frame download copy is visible as
  latency or CPU load.
- A sandbox (for example seccomp or a separate decoder process) becomes
  standard for QindaQt media parsing.
