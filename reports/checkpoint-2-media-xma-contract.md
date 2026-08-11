# Checkpoint 2 — XMA/ASF media boundary

Date: 2026-08-11

The native cache now has a streaming FFmpeg path for compressed media. FFmpeg
headers and `libavformat/libavcodec/libavutil` are linked through pkg-config;
the qualified runtime reports `av_version_info()`. `RetailMediaDecoder` uses a
custom seekable AVIO reader over a content-addressed cache blob, decodes audio
frames without loading the compressed pack into memory, and records the
pre-peripheral PCM SHA-256.

Qualified smoke evidence on the imported PAL cache:

```text
media_decode=pass decoder=8.0.1-3ubuntu2
pcm_sha256=4f8eafe7fede17fca8e8ea4c8badf5c26b1ca78d885860434a74e3aa4db4cf55
sample_rate=48000 channels=6 samples=71795328
```

The XMA2 stream is the actual `bgmpack.bin` blob, not a synthetic fixture.
The decoder refuses missing streams, unsupported sample formats, and invalid
custom-IO probes. ASF movie playback remains explicitly open: its retail
`moviepack.bin` is not accepted by generic FFmpeg probing and still needs the
bounded ASF resource/index contract before any decode is claimed.
