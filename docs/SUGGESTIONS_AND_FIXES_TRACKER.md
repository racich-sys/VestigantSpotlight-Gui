## V1.3.3 - iOS thin lightweight sample exports and cautious provenance

- Implemented lightweight base-table sample SQL for previously heavy iOS thin sample exports.
- Full investigator views remain available for explicit full/support diagnostics.
- Added cautious provenance markers for device-owner contact, Trash path components, and LSQuarantine string references.
- Deferred NSKeyedArchiver graph decoding, APFS tombstone GUI surfacing, and WhereFroms decoding to later milestone releases.

# Suggestions and Fixes Tracker - V1.3.2

## Completed in V1.3.2

- AFF4 direct-map reader emits progress heartbeats during long map/chunk scanning stages.
- GUI status heartbeat includes percent-derived `GB of GB processed` estimate when source size and progress percent are available.
- iOS communication frequency view added.
- KnowledgeC communication-intent stream coverage expanded cautiously.
- Communication/identity provenance markers added to parsed iOS app/KnowledgeC records when source text supports them.
- Browser download table categorization added as `WEB_DOWNLOADS` without exfiltration conclusions.

## Highest-priority iOS Spotlight investigation items

- CoreSpotlight communication keys: DomainIdentifier, author, recipient, phone/email hints.
- Frequency/volume grouping by thread/contact identifiers.
- KnowledgeC interaction/intent parsing for share/message activity.
- Deleted/expired Spotlight communication surfacing.
- NSKeyedArchiver graph reconstruction.

## Deferred / Not Yet Implemented

- Full NSKeyedArchiver unflattener.
- WhereFroms/quarantine APFS metadata extraction.
- Full APFS parser unification.
- Safe Live Ingest Preview panel.
- GUI SQLite connection pooling, pending measured need.

## V1.3.2.3 - iOS thin profile and export responsiveness

Completed:
- Default iOS thin runs no longer request diagnostic/support full-case exports.
- Bounded samples are generated for selected heavy iOS CSV surfaces.
- `-FullDiagnostics` is available for support runs that intentionally need full heavy exports.

Remaining:
- Add early/partial JSON summary generation during long runs.
- Add more granular cancellation handling inside the most expensive SQLite view exports.
- Add iOS keychain plist intake after V1.3.2.x thin stability is validated.

## V1.3.5 Update
- Implemented cautious WhereFroms XATTR surfacing, bounded bplist/NSKeyedArchiver graph-sample output, safe iOS provenance markers, and tombstone/deleted review routing keywords.
- Preserved non-interpretive wording: no automatic exfiltration or destruction conclusions were added.
- Local validation: Linux CMake build PASS; CLI version reports v1.3.5; self-test PASS.
- Next required upload: V1.3.5 Windows build log and iOS thin output.
