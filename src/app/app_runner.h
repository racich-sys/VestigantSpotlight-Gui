#pragma once
#include "app/models.h"
#include <atomic>

namespace vestigant::spotlight {
RunResult runApplication(const RunOptions& opt, const std::atomic_bool* cancelToken = nullptr);
bool validateRunOptions(const RunOptions& opt, std::string& error);
}
