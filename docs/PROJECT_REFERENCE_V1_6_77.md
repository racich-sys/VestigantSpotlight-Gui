# Project Reference V1.6.77

## V1.6.77 current changes

- Preserves the V1.6.76 AFF4/APFS CoreFields uncapped workflow and the prior GUI/build fixes.
- Adds a full local APFS directory-record name index for unresolved Spotlight object resolution:
  - `aff4_apfs_directory_record_name_index.csv` is written locally for resolver use.
  - `aff4_apfs_directory_record_name_index_sample.csv` is upload-safe.
  - `aff4_apfs_directory_record_name_index_summary.json` records row counts/provenance.
  - `AFF4_APFS_DIRECTORY_RECORD_NAME_INDEX.md` explains the output.
- Updates the unresolved Spotlight object resolver to prefer the full local APFS directory-record name index instead of relying only on the bounded `aff4_apfs_spotlight_name_scan_sample.csv`.
- Keeps parent-only matches as review context only; they are not applied as current file/folder paths without a direct child-file-ID match.
- Keeps the one-click PowerShell workflow as the default package entry point.

## Copy/paste command

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_77-AfterDownload.ps1
```

## Expected uploads after AFF4 run

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_77.zip
D:\Downloads\V1_6_77_build.log
D:\Downloads\V1_6_77_AFF4_WRAPPER_RUN_SUMMARY.txt
```

## V1.6.76 result reviewed before this version

- Wrapper completed with `RunnerExitCode: 0`, `FullNoGuardrails: True`, `FullNativeValues: False`, and `MaxNativeRecords: 0`.
- Build log reported `Vestigant Spotlight v1.6.76` and completed the smoke/self-test.
- Case summary showed `raw_record_count=102,170`, `artifact_count=101,326`, and `timeline_event_count=102,170`.
- Resolver summary showed `unresolved_before=71,402`, `artifacts_path_updated=1,281`, `artifacts_name_updated=59`, and `name_scan_paths_reconstructed=1,281`.
- Next validation target is whether V1.6.77 materially increases direct child-file-ID name/path resolution by using the full local APFS directory-record index.
