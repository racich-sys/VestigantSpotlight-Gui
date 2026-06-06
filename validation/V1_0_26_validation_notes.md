# V1.0.26 Validation Notes

## Inputs reviewed

- `VestigantSpotlightInv_V1_0_25.zip`
- `V1_0_25_build.log`
- `Upload_Thin_MacOS_AFF4_V1_0_25.zip`
- User review file `Pasted markdown.md`

## Build-log review

The uploaded V1.0.25 MSVC build log completed successfully and reported `Vestigant Spotlight v1.0.25`. No `error` or `fatal` lines were found in the decoded build log.

## Thin-output review

The uploaded V1.0.25 macOS AFF4/APFS thin output reached `complete_source_probe`. The case summary retained the staged Store-V2 parser baseline:

- `raw_record_count=25000`
- `raw_key_value_count=2181`
- `raw_date_candidate_count=25000`
- `artifact_count=25000`

The thin ZIP still included `aff4_stream_inventory_raw.txt`, which means V1.0.25 did not fully close the raw-log thin-upload leak in the standalone upload-tool path. V1.0.26 addresses that path.

## Local validation performed

- Updated version fields to `1.0.26`.
- Ran C++20 syntax check for `src/app/app_runner.cpp` with `VESTIGANT_HAS_LZFSE=1`.
- Ran C++20 syntax checks for:
  - `src/core/app_info.cpp`
  - `src/gui/gui_view_helpers.cpp`
  - `src/gui/gui_export_worker.cpp`
- Checked V1.0.26 build/launch/AFF4 wrapper scripts for current version references.

## Not validated locally

- Windows/MSVC full build.
- Windows GUI runtime.
- V1.0.26 macOS AFF4/APFS thin run.
- V1.0.26 iOS CoreSpotlight thin run.
