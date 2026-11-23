#include "achievements/core.hpp"
#include "atomic_file.hpp"
#include "rac2_save_format.h"
#include <algorithm>
#include <charconv>
#include <utility>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#undef max
#undef min
#endif

namespace achievements {
#include "rac2_rules.inc"
namespace {
constexpr auto profile_magic = "rac2-achievements 2";
constexpr std::size_t max_profile_bytes = 1024 * 1024;
constexpr std::size_t max_profile_rows = 4096;

bool valid_key(const std::string& key) {
    return !key.empty() && key.size() <= 256
        && std::all_of(key.begin(), key.end(), [](unsigned char c) { return c >= 0x21 && c <= 0x7e; });
}

bool integer(std::istream& row, std::uint64_t& value) {
    std::string word;
    if (!(row >> word)) return false;
    const auto result = std::from_chars(word.data(), word.data() + word.size(), value);
    return result.ec == std::errc{} && result.ptr == word.data() + word.size();
}

bool read_row(std::istringstream& row, Profile& profile) {
    std::string kind, key;
    if (!(row >> kind >> key) || !valid_key(key)) return false;
    if (kind == "U") {
        const auto found = std::find_if(trophies.begin(), trophies.end(),
            [&key](const auto& trophy) { return key == trophy.id; });
        if (found == trophies.end()) return false;
        const auto index = static_cast<std::size_t>(found - trophies.begin());
        if (profile.unlocked[index]) return false;
        profile.unlocked.set(index);
    } else if (kind == "C") {
        CampaignProgress campaign;
        if (!integer(row, campaign.bolts)
            || !profile.campaigns.emplace(key, campaign).second) return false;
    } else if (kind == "K") {
        Checkpoint checkpoint;
        if (!(row >> checkpoint.campaign) || !valid_key(checkpoint.campaign)
            || !integer(row, checkpoint.bolts)
            || !profile.checkpoints.emplace(key, checkpoint).second) return false;
    } else {
        return false;
    }
    row >> std::ws;
    return row.eof();
}
} // namespace

bool load_profile(const std::filesystem::path& path, Profile& output, std::string& error) {
    error.clear();
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        error = "Cannot inspect the achievement profile.";
        return false;
    }
    if (!exists) return true;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > max_profile_bytes) {
        error = "Achievement profile is too large or unreadable.";
        return false;
    }
    std::ifstream input(path);
    std::string line;
    if (!std::getline(input, line) || line != profile_magic) {
        error = "Invalid achievement profile header.";
        return false;
    }
    Profile profile;
    std::size_t rows = 0;
    while (std::getline(input, line)) {
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        std::istringstream row(line);
        if (++rows > max_profile_rows || !read_row(row, profile)) {
            error = "Invalid achievement profile record.";
            return false;
        }
    }
    if (!input.eof()) {
        error = "Cannot read the complete achievement profile.";
        return false;
    }
    output = std::move(profile);
    return true;
}

bool save_profile(const std::filesystem::path& path, const Profile& profile, std::string& error) {
    error.clear();
    if (profile.unlocked.count() + profile.campaigns.size() + profile.checkpoints.size() > max_profile_rows) {
        error = "Too many achievement profile records.";
        return false;
    }
    std::ostringstream rows;
    rows << profile_magic << '\n';
    for (std::size_t i = 0; i < trophies.size(); ++i) {
        if (profile.unlocked[i]) rows << "U " << trophies[i].id << '\n';
    }
    for (const auto& [key, campaign] : profile.campaigns) {
        if (!valid_key(key)) {
            error = "Invalid campaign key.";
            return false;
        }
        rows << "C " << key << ' ' << campaign.bolts << '\n';
    }
    for (const auto& [key, checkpoint] : profile.checkpoints) {
        if (!valid_key(key) || !valid_key(checkpoint.campaign)) {
            error = "Invalid checkpoint key.";
            return false;
        }
        rows << "K " << key << ' ' << checkpoint.campaign << ' ' << checkpoint.bolts << '\n';
    }
    const auto contents = rows.str();
    if (contents.size() > max_profile_bytes) {
        error = "Achievement profile is too large.";
        return false;
    }
    return write_file_atomically(path, contents, error);
}

bool write_file_atomically(const std::filesystem::path& path, std::string_view contents, std::string& error) {
    error.clear();
    auto temporary = path;
    temporary += ".tmp";
    std::error_code ec;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    if (!output) {
        error = "Cannot write the temporary file.";
        return false;
    }
#ifdef _WIN32
    if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
#else
    std::filesystem::rename(temporary, path, ec);
    if (!ec) return true;
#endif
    error = "Cannot replace the saved file.";
    std::filesystem::remove(temporary, ec);
    return false;
}
} // namespace achievements
