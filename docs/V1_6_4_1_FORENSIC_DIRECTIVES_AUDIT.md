# V1.6.5 Forensic Directives Audit

This audit was added before the V1.6.5 package to record source-level verification of the requested iOS-first directives and the APFS stability directive.

## Verified in source

1. APFS guided traversal cycle detection is present in `src/parsers/aff4_probe_worker.cpp`.
   - `visitedGuidedNodes` appears in the guided inode lookup and guided file-extent lookup paths.
   - Cycle markers include `GUIDED_INODE_LOOKUP_CYCLE_DETECTED` and `GUIDED_FILE_EXTENT_LOOKUP_CYCLE_DETECTED`.

2. iOS bplist / NSKeyedArchiver recovery support is present in `src/parsers/ios_app_db_parser.cpp`.
   - `ripBplistStrings(...)` is present.
   - `resolveUid(...)` is present for bounded UID/object reconstruction.
   - Embedded bplist text is routed through `unflattenNSKeyedArchiverOrRip(...)` / bplist string ripping before truncation.

3. iOS identity recovery from fragmented metadata is present.
   - `deriveIosCommunicationFields(...)` searches for `tel:` and `mailto:`.
   - Generic phone/email text-pattern recovery is also present.

4. Notes and location records are routed into iOS app database parsing.
   - `NOTES_RECORDS` and `LOCATION_RECORDS` are present in categorization and target parsing paths.

5. Wider text/path column catchers are present.
   - `zcontent`, `data`, and `payload` are included in text extraction column candidates.
   - `zfilename`, `zpath`, and `zlocalpath` are included in path extraction column candidates.

6. Spotlight communications not matched to parsed native databases are surfaced.
   - `vw_ios_spotlight_comms_missing_from_ffs` is present in schema creation.
   - GUI view registry includes `iOS - Spotlight Comms Missing From Native DB`.
   - Wording remains conservative: the view indicates CoreSpotlight presence not matched to parsed native app DB rows; it does not assert deletion as the only explanation.

## Not changed in V1.6.5

No parser behavior was changed for these directives because the V1.6.4 source already contained the requested logic. V1.6.5 adds this explicit audit, a verification tool, and an `ai_context.md` update so this verification can be repeated.

## Release rule

Future releases must run `tools/Verify-V1_6_5-ForensicDirectives.ps1` or a successor version-specific check before packaging if these directives are in scope.
