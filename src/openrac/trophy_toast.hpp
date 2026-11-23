#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>

struct openrac_overlay;

namespace achievements::runtime {

// A deliberately small, serial overlay for achievement unlock feedback.
class TrophyToast {
public:
    explicit TrophyToast(std::filesystem::path icon_directory);
    ~TrophyToast();

    TrophyToast(const TrophyToast&) = delete;
    TrophyToast& operator=(const TrophyToast&) = delete;
    TrophyToast(TrophyToast&&) noexcept;
    TrophyToast& operator=(TrophyToast&&) noexcept;

    void enqueue(std::size_t trophy_index, bool preview = false);
    void clear();
    // True on the first rendered frame of a toast.
    bool draw(const openrac_overlay& overlay,
              std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace achievements::runtime
