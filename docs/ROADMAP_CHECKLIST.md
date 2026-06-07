# Roadmap Checklist - V1.3.2

## Active priority order

1. AFF4/APFS progress visibility and status accuracy.
2. iOS communication/identity investigation.
3. Case tab/button stability.

## A. Stability / Architecture

- [x] Virtual ListView rendering.
- [x] Export thread lifecycle audit.
- [x] APFS guided buffer reuse.
- [x] AFF4 direct-map progress heartbeats.
- [x] GUI GB-of-GB progress estimate when source size and percent are available.
- [ ] Safe Live Ingest Preview panel.
- [ ] GUI SQLite access pooling only if measured need is confirmed.
- [ ] Full APFS BlockReader abstraction.

## B. iOS Communication & Identity Analysis

- [x] Communication-frequency SQL view and GUI route.
- [x] Communication-frequency CSV export.
- [x] Provenance markers for identity-bound communication hints.
- [x] Thread-volume provenance marker when kMDItemDomainIdentifier appears in parsed metadata.
- [x] KnowledgeC `/app/activity` and `/item/interactions` stream inclusion.
- [x] Cautious KnowledgeC communication-intent labeling.
- [ ] CoreSpotlight native parser field-level extraction for DomainIdentifier/Author/Recipient beyond existing text/context surfaces.
- [ ] NSKeyedArchiver UID graph reconstruction.
- [ ] iOS thin validation of new communication-frequency view.

## C. Case Tab Stability

- [x] Ingest-time mutation-control disabling/deferred autosave from V1.3.1.
- [ ] Review any remaining Case tab buttons reported from GUI runs.
- [ ] Improve running status display from APFS/AFF4 worker heartbeats.

## D. APFS Metadata Completeness

- [ ] WhereFroms extraction.
- [ ] Quarantine metadata extraction.
- [ ] Volume metadata extraction.
- [ ] Deleted/tombstone artifact surfacing with provenance-only language.

## E. Unified APFS Reader

- [ ] Shared B-tree leaf traversal helpers.
- [ ] Shared BlockReader abstraction.
- [ ] Direct-map and libaff4 APFS paths converged onto shared traversal math.
