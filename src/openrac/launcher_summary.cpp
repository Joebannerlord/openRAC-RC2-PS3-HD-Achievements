#include "launcher_summary.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>

namespace achievements {
namespace {
constexpr char mod_id[] = "rac2-hd-achievements";

const char* grade_name(Grade grade) {
    switch (grade) {
        case Grade::bronze: return "bronze";
        case Grade::silver: return "silver";
        case Grade::gold: return "gold";
        case Grade::platinum: return "platinum";
    }
    return "bronze";
}

void json_string(std::ostream& out, std::string_view text) {
    constexpr char digits[] = "0123456789abcdef";
    out.put('"');
    for (const unsigned char ch : text) {
        if (ch == '"' || ch == '\\') out.put('\\');
        if (ch < 0x20) {
            out << "\\u00" << digits[ch >> 4] << digits[ch & 15];
        } else {
            out.put(static_cast<char>(ch));
        }
    }
    out.put('"');
}
} // namespace

std::string trophy_summary(const Profile& profile, const Snapshot* snapshot, const std::string& campaign) {
    std::ostringstream out;
    out << "{\"version\":1,\"trophies\":[\n";
    const auto& trophies = catalog();
    for (std::size_t i = 0; i < trophies.size(); ++i) {
        const auto& trophy = trophies[i];
        if (i != 0) out << ",\n";
        out << "{\"id\":";
        json_string(out, trophy.id);
        out << ",\"name\":";
        json_string(out, trophy.name);
        out << ",\"description\":";
        json_string(out, trophy.description);
        out << ",\"grade\":";
        json_string(out, grade_name(trophy.grade));
        const bool earned = profile.unlocked[i];
        out << ",\"earned\":" << (earned ? "true" : "false") << ",\"progress\":";
        const auto measured = snapshot ? progress(profile, campaign, *snapshot, i) : std::nullopt;
        if (measured) {
            const auto current = earned ? measured->total : std::min(measured->current, measured->total);
            out << "{\"current\":" << current << ",\"total\":" << measured->total << '}';
        } else {
            out << "null";
        }
        out.put('}');
    }
    out << "\n]}\n";
    return out.str();
}

std::filesystem::path launcher_summary_path(const std::filesystem::path& mod_data) {
    const auto path = mod_data.lexically_normal();
    const auto store = path.parent_path();
    if (!path.is_absolute() || path.filename() != mod_id || store.filename() != "mod-data") return {};
    return store.parent_path() / "mod-tools" / mod_id / "trophies.json";
}

} // namespace achievements
