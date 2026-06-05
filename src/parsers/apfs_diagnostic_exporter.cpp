#include "parsers/apfs_diagnostic_exporter.h"

namespace vestigant::spotlight {

bool shouldWriteAff4ApfsStructuralDiagnostics(bool verbose,
                                              bool diagnosticFullNativeDb,
                                              bool aff4ApfsDiagnosticOutputs) {
    return verbose || diagnosticFullNativeDb || aff4ApfsDiagnosticOutputs;
}

std::string aff4ApfsStructuralDiagnosticsSuppressedStatus() {
    return "aff4_apfs_structural_diagnostics_suppressed";
}

std::string aff4ApfsStructuralDiagnosticsSuppressedGuidance() {
    return "use --aff4-apfs-diagnostic-outputs or --verbose to write structural probe CSVs";
}

} // namespace vestigant::spotlight
