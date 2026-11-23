#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace achievements {

bool write_file_atomically(const std::filesystem::path& path, std::string_view contents, std::string& error);

} // namespace achievements
