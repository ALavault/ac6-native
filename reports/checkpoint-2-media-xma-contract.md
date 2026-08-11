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
`moviepack.bin` is not accepted by generic FFmpeg probing, so no video decode
is claimed. The bounded resource/index contract is nevertheless now
qualified by `RetailAsfIndex`: it scans the content-addressed blob in 1 MiB
windows, parses the BNK prefix plus the standard File Properties/Header
Extension objects, and validates the following little-endian monotone offset
table without loading the compressed pack into one buffer.

```text
asf_index=pass banks=2 entries=2630,2819
bank_0 offset=0 size=164638720 index_offset=4356 first=28142156 last=164634736
bank_1 offset=164638720 size=183668736 index_offset=164643076 first=28876056 last=183665040
```

Synthetic coverage rejects an out-of-bank offset and a truncated bank. The
remaining boundary is codec/container decoding and audio/video
synchronisation, not resource discovery.
