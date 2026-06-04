#pragma once
#include "app/models.h"

namespace vestigant::spotlight {
RunResult runApplication(const RunOptions& opt);
RunResult runAutomatedSelfTest(const RunOptions& opt);
bool validateRunOptions(const RunOptions& opt, std::string& error);
}
