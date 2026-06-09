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

## V1.6.0 Update
- Implemented cautious WhereFroms XATTR surfacing, bounded bplist/NSKeyedArchiver graph-sample output, safe iOS provenance markers, and tombstone/deleted review routing keywords.
- Preserved non-interpretive wording: no automatic exfiltration or destruction conclusions were added.
- Local validation: Linux CMake build PASS; CLI version reports v1.6.0; self-test PASS.
- Next required upload: V1.6.0 Windows build log and iOS thin output.

## V1.6.0 handoff

Use V1.6.0 after V1.3.6 iOS thin failed with `database or disk is full` during `ios_ffs_inventory_csv_stream_import`. The corrected standard iOS thin path uses slim FFS lookup unless explicit full/support materialization is requested.


## V1.6.0 requested-fixes verification

- Verified and retained the requested GUI database pool deadlock fix, APFS guided traversal cycle detection, iOS bplist string extraction, Notes/Location routing, and widened table-column catchers.
- Added `tools/Verify-V1_6_0-RequestedFixes.ps1` for repeatable source-presence validation.
- Standard iOS thin should be run; AFF4/APFS thin is not required unless APFS cycle-guard behavior is specifically tested.

## V1.6.0 ai_context.md bootstrap

- Added root `ai_context.md` as the living project context file.
- Future source packages must carry forward and update `ai_context.md` with current state, roadmap, known bugs, and graveyard items.
- No parser, extraction, GUI, SQLite schema, or forensic interpretation behavior intentionally changed by this documentation-only package.
