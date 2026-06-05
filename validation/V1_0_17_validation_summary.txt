V1.0.17 validation summary

Reviewed inputs:
- V1_0_16_build.log
- Upload_Thin_MacOS_AFF4_V1_0_16.zip
- lzfse-master.zip
- Apple APFS reference and prior APFS compression notes

Input findings:
- V1.0.16 MSVC build succeeded but reported Apple lzfse source not vendored.
- V1.0.16 AFF4/APFS staged 8,986 normalized Store-V2 files and parsed 25,000 raw Store-V2 records.
- External comparison still had 486 relative-path size mismatches and 1,424 external-only rows.
- Candidate comparison showed 8,979 staged rows selected the best candidate, with seven rows still differing from the best candidate.

Implemented changes:
- Vendored uploaded Apple/lzfse source into third_party/lzfse.
- Recorded vendor manifest and uploaded ZIP SHA256.
- Added codec-enabled smoke test using a known Apple/lzfse-produced LZVN vector.
- Added copy-out summary fields for Apple/lzfse codec status and decmpfs LZVN/LZFSE rows.
- Added macOS investigative feature inventory and roadmap document.
- Updated scripts/version metadata to 1.0.17.

Validation performed in this environment:
- CMake configure detected Apple/lzfse and enabled VESTIGANT_HAS_LZFSE.
- Linux build progressed through lzfse_codec.cpp and Apple decoder sources before timing out during the existing large app_runner.cpp build; no compile error was observed before timeout.
- Syntax checks were run for changed C++ source where possible.

Still required:
- Windows/MSVC V1.0.17 build with Apple/lzfse enabled.
- VestigantSpotlightTests.exe smoke test confirming the codec vector passes under MSVC.
- Live AFF4/APFS run against the O: AFF4 image.
- Review of LZVN/LZFSE decmpfs copy-out statuses and external comparison impact.
