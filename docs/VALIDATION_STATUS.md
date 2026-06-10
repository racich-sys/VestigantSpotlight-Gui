# Current Validation Status - V1.6.6.5

- Linux CMake build: PASS.
- CLI version: `Vestigant Spotlight v1.6.6.5`.
- Self-test: PASS, including native CoreSpotlight probe text-context coverage.
- Uploaded V1.6.6.4 Windows build log: completed with `Vestigant Spotlight v1.6.6.4`.
- Uploaded V1.6.6.4 iOS thin bundle: `complete_success`.
- Windows/MSVC build for V1.6.6.5: not run in this environment.
- Required next validation: run `scripts\Build-V1_6_6_5.ps1`, then run iOS thin with `scripts\Run-V1_6_6_5-iOS-CoreSpotlight-AndZip.ps1 -CleanOut`.

AFF4/APFS thin/full is not required for V1.6.6.5 unless Windows build/shared schema initialization regresses.
