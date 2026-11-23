#include "launcher_summary.hpp"
#include "trophy_toast.hpp"
#include "../core/atomic_file.hpp"

#include <openrac/mod.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace {
using namespace achievements;
using Save = std::array<std::uint8_t, save_size>;

constexpr std::string_view mod_id = "rac2-hd-achievements";
constexpr openrac_gaddr card_records = 0x1393e0;
constexpr openrac_gaddr card_state = 0x13953c;
constexpr openrac_gaddr operation_card = 0x13952c;
constexpr openrac_gaddr card_error = 0x13954c;
constexpr openrac_gaddr bolt_wallet = 0x1a79f8;
constexpr openrac_gaddr level_bolt_scores = 0x18d208;
constexpr unsigned level_bolt_score_count = 28;
constexpr int yeedil = 20;
constexpr openrac_gaddr yeedil_request_queue = 0x1c01c0;
constexpr std::uint32_t protopet_defeat_request = 0xa1;

struct BoltSample {
    std::uint32_t wallet;
    std::uint64_t scored;
};

std::string checkpoint(int slot, const Save& data) {
    std::uint64_t digest = 14695981039346656037ull;
    for (auto byte : data) {
        digest ^= byte;
        digest *= 1099511628211ull;
    }
    return std::to_string(slot) + ':' + std::to_string(digest);
}

std::filesystem::path icons(const openrac::Mod& mod) {
    if (!mod.has_slot14(offsetof(openrac_api, plugins_folder)) || !mod.api().plugins_folder) return {};
    const auto plugins = mod.plugins_folder();
    return plugins.empty() ? std::filesystem::path{} :
        std::filesystem::u8path(plugins) / mod_id / "ui" / "icons";
}

class Adapter {
public:
    explicit Adapter(openrac::Mod& mod) : mod_(mod), api_(mod.api()), toast_(icons(mod)) {
        const auto dir = mod_.data_dir();
        if (dir.empty()) throw openrac::Error("RAC2 achievement data folder unavailable");
        path_ = std::filesystem::u8path(dir) / "profile.txt";
        summary_path_ = launcher_summary_path(path_.parent_path());
        std::string error;
        if (!load_profile(path_, profile_, error)) throw openrac::Error(error);
    }

    void start() {
        if (mod_.has_overlay() && api_.on_overlay) {
            toast_enabled_ = mod_.on_overlay([](void* user, const openrac_overlay* overlay) {
                auto& self = *static_cast<Adapter*>(user);
                if (overlay && self.toast_.draw(*overlay) && self.chime_ >= 0) self.mod_.sound_play(self.chime_);
            }, this);
        }
        if (toast_enabled_ && api_.setting_string) {
            preview_ = openrac::bind_from_name(mod_.setting<std::string>("preview_toast"));
        }
        if (toast_enabled_ && mod_.has_sounds() && api_.sound_load && api_.sound_play
            && (!api_.setting_bool || mod_.setting<bool>("toast_sound"))) {
            chime_ = mod_.sound_load("ui/audio/trophy-chime.wav");
        }
        mod_.on_frame([this] { frame(); });
    }

    void flush() {
        if (profile_dirty_) {
            std::string error;
            if (save_profile(path_, profile_, error)) profile_dirty_ = false;
            else mod_.warn("RAC2 achievement profile write failed. " + error);
        }
        if (!summary_dirty_ || summary_path_.empty()) return;
        const auto summary = trophy_summary(profile_, displayed_ ? &*displayed_ : nullptr, campaign_);
        if (summary != published_) {
            std::string error;
            if (!write_file_atomically(summary_path_, summary, error)) {
                mod_.warn("RAC2 trophy viewer update failed. " + error);
                return;
            }
            published_ = summary;
        }
        summary_dirty_ = false;
    }

private:
    template<class T>
    T read(openrac_gaddr address) const {
        T value;
        std::memcpy(&value, api_.memory + address, sizeof(value));
        return value;
    }

    bool read_slot(int slot, Save& save) const {
        if (slot < 0 || slot >= 4) return false;
        openrac_save_info info{};
        info.size = sizeof(info);
        return api_.save_slot(slot, &info) == 1
            && api_.save_read(slot, save.data(), save.size()) == save.size();
    }

    int native_slot(openrac_gaddr port_address) const {
        // Stable's port-zero card is the only one represented by save_read.
        if (read<std::int32_t>(port_address) != 0) return -1;
        const int slot = read<std::int16_t>(card_records + 0x18);
        return slot >= 0 && slot < 4 ? slot : -1;
    }

    void inspect_card(int state, bool in_game) {
        if (state == 13 || state == 14) {
            saw_load_ = true;
            const int slot = native_slot(operation_card);
            if (slot >= 0) load_slot_ = slot;
        } else if (saw_load_ && read<std::int32_t>(card_error) != 0) {
            saw_load_ = false;
            load_slot_ = -1;
        }
        const bool writing = state == 15 || state == 16 || state == 19 || state == 20;
        if (writing && !writing_ && in_game && !campaign_.empty()) {
            write_slot_ = native_slot(operation_card);
            Save old;
            old_checkpoint_ = read_slot(write_slot_, old) ? checkpoint(write_slot_, old) : "";
            write_snapshot_ = capture();
            write_campaign_ = campaign_;
            write_bolts_ = branch_bolts_;
        }
        if (!writing && writing_ && write_slot_ >= 0) {
            Save save;
            if (read<std::int32_t>(card_error) == 0 && read_slot(write_slot_, save)) {
                const auto key = checkpoint(write_slot_, save);
                const auto saved = decode_save(save.data(), save.size());
                if (key != old_checkpoint_ && saved && write_snapshot_
                    && write_snapshot_->play_ticks <= static_cast<std::uint64_t>(saved->play_ticks) + 120
                    && saved->play_ticks <= static_cast<std::uint64_t>(write_snapshot_->play_ticks) + 120
                    && saved->challenge == write_snapshot_->challenge
                    && saved->wallet == write_snapshot_->wallet
                    && saved->scored_bolts == write_snapshot_->scored_bolts
                    && saved->skills == write_snapshot_->skills
                    && saved->owned == write_snapshot_->owned) {
                    profile_dirty_ |= profile_.checkpoints.emplace(key, Checkpoint{
                        write_campaign_, std::max(write_bolts_, saved->bolt_floor())
                    }).second;
                }
            }
            write_slot_ = -1;
            write_snapshot_.reset();
        }
        writing_ = writing;
    }

    std::string fresh_campaign() {
        const auto now = std::chrono::system_clock::now().time_since_epoch().count();
        return "new:" + std::to_string(now) + ':' + std::to_string(++new_count_);
    }

    bool begin(const Snapshot& current) {
        Save save;
        if (load_slot_ >= 0 && read_slot(load_slot_, save)) {
            const auto stored = decode_save(save.data(), save.size());
            if (!stored || stored->challenge != current.challenge || stored->level != current.level
                || current.play_ticks < stored->play_ticks || current.play_ticks - stored->play_ticks > 900
                || stored->skills != current.skills || stored->owned != current.owned
                || stored->wallet != current.wallet || stored->scored_bolts != current.scored_bolts) return false;
            const auto key = checkpoint(load_slot_, save);
            const auto entry = profile_.checkpoints.emplace(key, Checkpoint{key, stored->bolt_floor()});
            campaign_ = entry.first->second.campaign;
            branch_bolts_ = entry.first->second.bolts;
            profile_dirty_ |= entry.second;
            observe(*stored, false, true);
            load_slot_ = -1;
            saw_load_ = false;
            return true;
        }
        if (saw_load_ || current.level != 0 || current.challenge != 0 || current.play_ticks > 900) {
            return false;
        }
        campaign_ = fresh_campaign();
        branch_bolts_ = current.bolt_floor();
        return true;
    }

    void record_bolts(std::uint64_t floor) {
        branch_bolts_ = std::max(branch_bolts_, floor);
        auto& best = profile_.campaigns[campaign_].bolts;
        if (branch_bolts_ > best) {
            best = branch_bolts_;
            profile_dirty_ = true;
            summary_dirty_ = true;
        }
    }

    void sample_bolts(bool continuous_play, std::uint32_t loads) {
        if (!continuous_play || campaign_.empty()) {
            previous_bolts_.reset();
            return;
        }

        BoltSample current{read<std::uint32_t>(bolt_wallet), 0};
        if (current.wallet > 0x7fffffff) {
            previous_bolts_.reset();
            return;
        }
        for (unsigned level = 0; level < level_bolt_score_count; ++level) {
            const auto scored = read<std::uint32_t>(level_bolt_scores + 4 * level);
            if (scored <= 0x7fffffff) current.scored += scored;
        }
        if (previous_bolts_ && loads == sampled_loads_) {
            const auto prev = *previous_bolts_;
            const auto wallet_gain = current.wallet > prev.wallet ? current.wallet - prev.wallet : 0;
            const auto score_gain = current.scored > prev.scored ? current.scored - prev.scored : 0;
            // These sources overlap. The largest increment proves gains even if another one
            // was reduced by spending, or if a reward bypassed the pickup function.
            const auto gain = std::max<std::uint64_t>(wallet_gain, score_gain);
            branch_bolts_ += std::min(gain, UINT64_MAX - branch_bolts_);
        }
        previous_bolts_ = current;
        sampled_loads_ = loads;
        record_bolts(std::max<std::uint64_t>(current.wallet, current.scored));
    }

    std::optional<Snapshot> capture() {
        if (api_.save_capture(buffer_.data(), buffer_.size()) != buffer_.size()) return std::nullopt;
        return decode_save(buffer_.data(), buffer_.size());
    }

    void check_protopet_defeat() {
        if (protopet_ending_observed_) return;
        const auto queue = api_.relocate_level_data(yeedil, yeedil_request_queue);
        if (!queue) return;
        // The queue initializes all five entries and keeps a request ID in its slot when popped.
        // Only the boss's defeat branch submits 0xA1 to this Yeedil queue.
        for (unsigned slot = 0; slot < 5; ++slot) {
            if (read<std::uint32_t>(queue + 8 + 8 * slot) != protopet_defeat_request) continue;
            auto proof = displayed_ ? displayed_ : capture();
            if (proof) {
                protopet_ending_observed_ = true;
                proof->protopet = true;
                observe(*proof, false, false);
            }
            return;
        }
    }

    void observe(const Snapshot& snapshot, bool museum, bool imported) {
        if (!campaign_.empty()) record_bolts(snapshot.bolt_floor());
        const auto unlocked = evaluate(profile_, campaign_, snapshot, museum);
        displayed_ = snapshot;
        profile_dirty_ |= !unlocked.empty();
        summary_dirty_ = true;
        for (auto index : unlocked) {
            if (toast_enabled_ && !imported) toast_.enqueue(index);
            mod_.info(std::string(imported ? "Imported achievement: " : "Achievement unlocked: ")
                      + catalog()[index].name);
        }
        if (imported || !unlocked.empty()) flush();
    }

    void preview() {
        if (!toast_enabled_ || preview_ == OPENRAC_BIND_NONE || !mod_.has_input() || !api_.input) return;
        const auto input = mod_.input();
        if (!input.focused() || input.overlay() || !input.pressed(preview_)) return;
        const auto& trophies = catalog();
        const auto demo = std::find_if(trophies.begin(), trophies.end(), [](const Trophy& trophy) {
            return std::string_view(trophy.id) == "more-vandalism";
        });
        if (demo != trophies.end()) toast_.enqueue(static_cast<std::size_t>(demo - trophies.begin()), true);
    }

    void frame() {
        openrac_play_status status{};
        status.size = sizeof(status);
        if (api_.play_status(&status) != OPENRAC_HOOK_OK) return;
        const bool playable = status.phase == OPENRAC_PLAY_PLAYING || status.phase == OPENRAC_PLAY_MENU
            || status.phase == OPENRAC_PLAY_SCENE;
        const auto state = read<std::int32_t>(card_state);
        const bool reading = state == 13 || state == 14;
        sample_bolts(playable && !reading && !saw_load_, status.loads);
        inspect_card(state, playable);
        if (status.phase == OPENRAC_PLAY_FRONT_END) {
            if (last_phase_ != OPENRAC_PLAY_FRONT_END) {
                flush();
                campaign_.clear();
                branch_bolts_ = 0;
                displayed_.reset();
                toast_.clear();
            }
            last_phase_ = status.phase;
            return;
        }
        last_phase_ = status.phase;
        if (status.phase == OPENRAC_PLAY_PLAYING) preview();
        if (!playable || reading) return;
        if (status.level == yeedil && !saw_load_) check_protopet_defeat();
        if ((campaign_.empty() || saw_load_) && status.phase != OPENRAC_PLAY_PLAYING) return;
        if (++ticks_ < 8 && status.loads == loads_) return;
        ticks_ = 0;
        loads_ = status.loads;
        const auto snapshot = capture();
        if (!snapshot) return;
        if (campaign_.empty() || saw_load_) {
            if (begin(*snapshot)) {
                previous_bolts_ = BoltSample{snapshot->wallet, snapshot->scored_bolts};
                sampled_loads_ = status.loads;
            } else if (saw_load_) {
                return;
            }
        }
        // The first observation can occur after a direct start or with an ambiguous card.
        // Import proof then, and reserve live notifications for later observations.
        observe(*snapshot, status.level == 30, !displayed_ && status.level != 30);
        if (++flush_cycles_ >= 8) {
            flush_cycles_ = 0;
            flush();
        }
    }

    openrac::Mod& mod_;
    const openrac_api& api_;
    runtime::TrophyToast toast_;
    Profile profile_;
    Save buffer_{};
    std::filesystem::path path_;
    std::filesystem::path summary_path_;
    std::optional<Snapshot> displayed_;
    std::optional<Snapshot> write_snapshot_;
    std::optional<BoltSample> previous_bolts_;
    std::string published_;
    std::string campaign_;
    std::string old_checkpoint_;
    std::string write_campaign_;
    std::uint64_t branch_bolts_ = 0;
    std::uint64_t write_bolts_ = 0;
    unsigned new_count_ = 0;
    unsigned ticks_ = 0;
    unsigned flush_cycles_ = 0;
    std::uint32_t loads_ = 0;
    std::uint32_t sampled_loads_ = 0;
    int load_slot_ = -1;
    int write_slot_ = -1;
    int chime_ = -1;
    int last_phase_ = OPENRAC_PLAY_NONE;
    int preview_ = OPENRAC_BIND_NONE;
    bool writing_ = false;
    bool saw_load_ = false;
    bool toast_enabled_ = false;
    bool protopet_ending_observed_ = false;
    bool profile_dirty_ = false;
    bool summary_dirty_ = false;
};

std::unique_ptr<Adapter> adapter;

void load(openrac::Mod& mod) {
    const auto& api = mod.api();
    if (mod.game() != "rac2" || mod.version() != "ntsc") {
        throw openrac::Error("RAC2 HD Achievements requires Going Commando USA v1.01");
    }
    if (!mod.has_api_11() || !mod.has_play() || !api.memory || !api.save_capture
        || !api.save_read || !api.save_slot || !api.on_frame || !api.play_status || !api.data_dir
        || !api.relocate_level_data) {
        throw openrac::Error("RAC2 HD Achievements requires the OpenRAC play and save interfaces");
    }
    adapter = std::make_unique<Adapter>(mod);
    adapter->start();
}
} // namespace

OPENRAC_MOD_CPP("rac2-hd-achievements", "0.1.3", load)

extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload() {
    if (adapter) adapter->flush();
    adapter.reset();
}
