#pragma once

#include "achievements/core.hpp"

namespace achievements {

// The same format is served as the bundled catalog and as the launcher's stored value.
std::string trophy_summary(const Profile& profile, const Snapshot* snapshot = nullptr,
                           const std::string& campaign = {});

// Normal launcher installs share one data home across mod-data and mod-tools.
std::filesystem::path launcher_summary_path(const std::filesystem::path& mod_data);

} // namespace achievements
