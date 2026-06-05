#pragma once

#include <string>

namespace vestigant::spotlight {

// Centralized policy for deciding whether structural APFS/AFF4 diagnostic CSVs
// should be written. Copy-out/staging evidence outputs are intentionally not
// governed by this helper and should remain enabled in normal runs.
bool shouldWriteAff4ApfsStructuralDiagnostics(bool verbose,
                                              bool diagnosticFullNativeDb,
                                              bool aff4ApfsDiagnosticOutputs);

std::string aff4ApfsStructuralDiagnosticsSuppressedStatus();
std::string aff4ApfsStructuralDiagnosticsSuppressedGuidance();

} // namespace vestigant::spotlight
