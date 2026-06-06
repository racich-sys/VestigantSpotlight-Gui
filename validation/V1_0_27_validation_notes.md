# V1.0.27 Validation Notes

## Inputs reviewed

- `V1_0_26_1_build.log`: Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26.1`.
- `Upload_Thin_MacOS_AFF4_V1_0_26_1.zip`: generated successfully and reviewed.
- `Pasted markdown (4).md`: reviewed suggestions for Job Objects, APFS path reconstruction, evidence intake modularization, NSKeyedArchiver parsing, and SQLite busy retry handling.

## Thin ZIP review

- Denied raw filenames were not found in the ZIP.
- `case_file_inventory.txt` and `additional_output_file_inventory.txt` did not contain full `Q:\`, `D:\`, or `T:\` absolute path prefixes.
- Run reached `complete_source_probe`.
- `case_summary.json`: `raw_record_count=25000`, `raw_key_value_count=2181`, `raw_date_candidate_count=25000`, `artifact_count=25000`.
- External compare summary: `external_file_count=4123`, `vestigant_file_count=8986`, `file_match_rows=2213`, `external_only_rows=1424`, `vestigant_only_rows=6710`, `hash_different_path_rows=431`.

## Local checks

Passed:

```text
g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_export_worker.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp
```

Not validated locally:

- Windows/MSVC full build.
- Windows Job Object runtime behavior.
- Windows PowerShell thin ZIP self-check execution.
- Windows GUI runtime and transient lock behavior.
