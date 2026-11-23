#pragma once
#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace achievements {
enum class Grade { bronze, silver, gold, platinum };
struct Trophy { const char* id; const char* name; const char* description; Grade grade; };
struct Snapshot {
    std::bitset<30> skills;
    std::bitset<56> owned;
    unsigned platinum_bolts = 0;
    unsigned nanotech = 0;
    unsigned ship_parts = 0;
    unsigned weapons_ready = 0;
    unsigned armor = 0;
    unsigned challenge = 0;
    std::uint32_t wallet = 0;
    std::uint32_t proven_bolt_spend = 0;
    std::uint64_t scored_bolts = 0;
    std::uint32_t play_ticks = 0;
    int level = -1;
    bool impossible = false;
    bool protopet = false;

    std::uint64_t bolt_floor() const {
        return std::max(static_cast<std::uint64_t>(wallet) + proven_bolt_spend, scored_bolts);
    }
};
struct CampaignProgress { std::uint64_t bolts = 0; };
struct Checkpoint { std::string campaign; std::uint64_t bolts = 0; };
struct Profile { std::bitset<35> unlocked; std::map<std::string, CampaignProgress> campaigns;
  std::map<std::string, Checkpoint> checkpoints; };
using Unlocks = std::vector<std::size_t>;
struct Progress { std::uint64_t current = 0, total = 0; };
const std::array<Trophy, 35>& catalog();
constexpr std::size_t save_size = 99264;
std::optional<Snapshot> decode_save(const std::uint8_t* bytes, std::size_t size);
Unlocks evaluate(Profile& profile, const std::string& campaign, const Snapshot& snapshot,
                 bool visited_museum = false);
std::optional<Progress> progress(const Profile&, const std::string& campaign,
                                 const Snapshot&, std::size_t catalog_index);
bool load_profile(const std::filesystem::path&, Profile&, std::string& error);
bool save_profile(const std::filesystem::path&, const Profile&, std::string& error);
} // namespace achievements
