#pragma once

#include <filesystem>
#include <memory>

struct IDWriteFactory;
struct IDWriteFontCollection;

namespace achievements::runtime {

// A process-local DirectWrite collection for the bundled toast typeface.
class ToastFont {
public:
    explicit ToastFont(const std::filesystem::path& file);
    ~ToastFont();

    ToastFont(const ToastFont&) = delete;
    ToastFont& operator=(const ToastFont&) = delete;

    IDWriteFactory* factory() const;
    IDWriteFontCollection* collection() const;

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace achievements::runtime
