# Build Instructions — Vestigant Spotlight 1.2.1

## Scope

V1.2.1 is a coordinated GUI runtime hardening release on V1.1.11. It adds virtual/owner-data review-grid rendering for the main investigation ListView, preserves consolidated notes/validation logs, and keeps full extract/build/thin command blocks for new chats. No AFF4/APFS extraction, Store-V2 parser, iOS parser, or SQLite schema behavior was intentionally changed.

## Standard V1.2.1 build command block

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_2_1.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_2_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_2_1.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_2_1\scripts\Build-V1_2_1.ps1
```

## Standard V1.2.1 AFF4/APFS thin command

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_2_1\scripts\Run-V1_2_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```


Expected successful version output:

```text
Vestigant Spotlight v1.2.1
```

Build log output:

```text
D:\Downloads\V1_2_1_build.log
```

Thin output:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_2_1.zip
```

## TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build.
- iOS: not required.
- Reason: V1.2.1 changes Win32 GUI review-grid rendering behavior and documentation only. The V1.1.11 build and AFF4/APFS thin output were reviewed before this version; no extraction/traversal/copy-out/decompression/parser code changed.
- Trigger for escalating AFF4/APFS to full test: any next change to live APFS traversal, copy-out, decompression, extent handling, path reconstruction, external compare logic, or Store-V2 staging behavior.
- Trigger for iOS testing: any next change to iOS ZIP staging, CoreSpotlight parsing, FFS lookup, app DB parsing, bplist/NSKeyedArchiver handling, iOS schema, or iOS GUI views.
- Required next uploaded artifacts: `V1_2_1_build.log` and `Upload_Thin_MacOS_AFF4_V1_2_1.zip`.

