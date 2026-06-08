# Current Continuation Handoff - V1.3.2

Current package: `VestigantSpotlightInv_V1_3_3.zip`

## Build command

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_3_3.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_3_3" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_3_3.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_3_3\scripts\Build-V1_3_3.ps1
```

## AFF4/APFS thin-create command

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_3_3\scripts\Run-V1_3_3-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

## Completed/worked sections

- A1: AFF4/APFS progress visibility advanced. Direct-map AFF4 reader now emits progress heartbeats during long operations.
- A2: GUI status display improved with percent-derived GB-of-GB source progress when possible.
- B1: iOS communication and identity analysis elevated to active work. Added communication identity provenance, KnowledgeC interaction streams, communication-frequency view, and export.
- C: Case tab stability remains in follow-through; prior V1.3.1 mutation-control safeguards remain in place.

## Still open

- Full NSKeyedArchiver UID graph reconstruction.
- Full APFS traversal unification through a shared BlockReader architecture.
- WhereFroms/quarantine/volume metadata extraction.
- Safe Live Ingest Preview panel.
- Any additional Case tab button issues observed during live GUI runs.

## Next uploads requested

- `V1_3_3_build.log`
- `Upload_Thin_MacOS_AFF4_V1_3_3.zip`
- iOS thin output if iOS communication-frequency validation is run.

## V1.3.5 Update
- Implemented cautious WhereFroms XATTR surfacing, bounded bplist/NSKeyedArchiver graph-sample output, safe iOS provenance markers, and tombstone/deleted review routing keywords.
- Preserved non-interpretive wording: no automatic exfiltration or destruction conclusions were added.
- Local validation: Linux CMake build PASS; CLI version reports v1.3.5; self-test PASS.
- Next required upload: V1.3.5 Windows build log and iOS thin output.
