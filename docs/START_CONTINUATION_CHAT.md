# Start Continuation Chat - V1.6.29.4

Use `VestigantSpotlightInv_V1_6_29_4.zip` as the current source baseline.

## Why this hotfix exists

V1.6.29.3 failed MSVC compile in `aff4_probe_worker.cpp` at the new OMAP vertical-cycle note calls. V1.6.29.4 uses the existing `aff4ApfsAppendProbeNote` helper and hardens the build wrapper so a missing CLI executable stops the build before version probing.

## Required next uploads

- `D:\Downloads\V1_6_29_4_build.log`
- `D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_29_4.zip`
