# Support and Diagnostic Tools Register

This register records support/diagnostic PowerShell tools retained in the active package. No support tool was removed in V1.1.11 because each either remains tied to AFF4/APFS validation, iOS support workflows, packaging, staging, or on-demand troubleshooting.

| Tool | Retention decision | Current-package references found | Reference examples |
|---|---|---:|---|
| `Build-Aff4CppLite-VS2022.ps1` | KEEP_ON_DEMAND_AFF4_SUPPORT | 4 | docs/BaselineVersionHistory.md; docs/FULL_VERSION_HISTORY.md; docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Collect-iOSCoreSpotlightQuickDiagnostics.ps1` | KEEP_ON_DEMAND_IOS_SUPPORT | 2 | docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Compare-ExternalSpotlightReference.ps1` | KEEP_ACTIVE_AFF4_APFS | 2 | docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Create-ApfsRemainingMismatchDiagnostics.ps1` | KEEP_ACTIVE_AFF4_APFS | 2 | docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Create-SourceProbeUploadZip.ps1` | KEEP_ACTIVE_AFF4_APFS | 15 | docs/BUILD_NOTES.md; docs/BaselineVersionHistory.md; docs/CONSOLIDATED_VERSION_HISTORY.md; docs/FULL_VERSION_HISTORY.md; docs/PROJECT_ROADMAP_AND_CONTINUATION.md; docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/SUGGESTIONS_AND_FIXES_TRACKER.md; docs/V1_0_18_GUI_EXPORT_AND_MODULARIZATION_CLEANUP.md |
| `Create-UploadZip.ps1` | KEEP_GENERAL_SUPPORT | 4 | docs/BaselineVersionHistory.md; docs/FULL_VERSION_HISTORY.md; docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Extract-IosCoreSpotlightFromZips.ps1` | KEEP_ON_DEMAND_IOS_SUPPORT | 2 | docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Extract-iOSCoreSpotlightFromFFSZips.ps1` | KEEP_ON_DEMAND_IOS_SUPPORT | 3 | docs/IOS_CORESPOTLIGHT_ROADMAP.md; docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Prepare-LzfseThirdParty.ps1` | KEEP_ACTIVE_AFF4_APFS | 6 | docs/BaselineVersionHistory.md; docs/CONSOLIDATED_VERSION_HISTORY.md; docs/FULL_VERSION_HISTORY.md; docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/V1_0_18_LZFSE_LZVN_OPTIONAL_VENDOR_INTEGRATION.md; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Probe-Aff4DirectMapScan.ps1` | KEEP_ON_DEMAND_AFF4_SUPPORT | 2 | docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Run-IosCoreSpotlightFocusedZip.ps1` | KEEP_ON_DEMAND_IOS_SUPPORT | 2 | docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Run-SingleAff4SourceProbeAndZip.ps1` | KEEP_ACTIVE_AFF4_APFS | 3 | docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md; scripts/Run-V1_1_11-macOS-AFF4-Probe-AndZip.ps1 |
| `Run-iOSCoreSpotlightFocusedAndZip.ps1` | KEEP_ON_DEMAND_IOS_SUPPORT | 3 | docs/IOS_CORESPOTLIGHT_ROADMAP.md; docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Search-SpotlightKeywordExports.ps1` | KEEP_GENERAL_SUPPORT | 2 | docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
| `Stage-EvidenceSource.ps1` | KEEP_GENERAL_SUPPORT | 4 | docs/BaselineVersionHistory.md; docs/FULL_VERSION_HISTORY.md; docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv; docs/CONSOLIDATED_DEVELOPMENT_NOTES.md |
