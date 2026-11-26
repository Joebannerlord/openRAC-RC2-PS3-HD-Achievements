#include "save_fixture.hpp"
#include <openrac/keys.h>
#include <openrac/openrac_mod.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif
#include <chrono>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using fixtures::check;
using fixtures::Save;

namespace {
struct {
    openrac_frame_fn frame{};
    void* frame_user{};
    openrac_overlay_fn overlay{};
    void* overlay_user{};
    openrac_play_status status{};
    openrac_input input{};
    std::vector<std::uint8_t> memory = std::vector<std::uint8_t>(32 << 20);
    std::array<std::optional<Save>, 4> slots;
    Save live;
    std::string data_dir;
    std::string plugins;
    bool chime = true;
    std::string preview = "Insert";
    unsigned chimes = 0;
    unsigned draw_calls = 0;
} host;

std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

void frame(int phase, int level, unsigned loads, unsigned repeat = 1) {
    host.status.phase = phase;
    host.status.level = level;
    host.status.loads = loads;
    if (const auto live = achievements::decode_save(host.live.data(), host.live.size())) {
        fixtures::word(host.memory.data() + 0x1a79f8, live->wallet);
        fixtures::word(host.memory.data() + 0x18d208, static_cast<std::uint32_t>(live->scored_bolts));
    }
    while (repeat--) host.frame(host.frame_user);
}

void card(unsigned state, unsigned slot) {
    fixtures::word(host.memory.data() + 0x13953c, state);
    const auto value = static_cast<std::int16_t>(slot);
    std::memcpy(host.memory.data() + 0x1393f8, &value, sizeof(value));
}

void draw() {
    openrac_overlay overlay{};
    overlay.size = sizeof overlay;
    overlay.rect = [](void*, float, float, float, float, std::uint32_t) { ++host.draw_calls; };
    overlay.text = [](void*, float, float, float, std::uint32_t, const char*) {};
    overlay.text_width = [](void*, float height, const char* value) {
        return height * .6F * static_cast<float>(std::strlen(value));
    };
    host.overlay(host.overlay_user, &overlay);
}

openrac_api api() {
    openrac_api value{};
    value.size = sizeof value;
    value.api_major = 1;
    value.api_minor = 20;
    value.game = "rac2";
    value.version = "ntsc";
    value.memory = host.memory.data();
    value.relocate_level_data = [](int level, openrac_gaddr address) -> openrac_gaddr {
        return level == 20 && address == 0x1c01c0 ? 0x201000 : 0;
    };
    value.fail = [](openrac_mod*, const char* reason) { std::cerr << reason << '\n'; };
    value.log = [](openrac_mod*, int, const char*) {};
    value.on_frame = [](openrac_mod*, openrac_frame_fn callback, void* user) {
        host.frame = callback;
        host.frame_user = user;
        return OPENRAC_HOOK_OK;
    };
    value.on_overlay = [](openrac_mod*, openrac_overlay_fn callback, void* user) {
        host.overlay = callback;
        host.overlay_user = user;
        return OPENRAC_HOOK_OK;
    };
    value.input = []() -> const openrac_input* { return &host.input; };
    value.data_dir = [](openrac_mod*) { return host.data_dir.c_str(); };
    value.plugins_folder = [](char* out, std::uint32_t size) {
        if (host.plugins.size() >= size) return 0;
        std::memcpy(out, host.plugins.c_str(), host.plugins.size() + 1);
        return static_cast<int>(host.plugins.size());
    };
    value.setting_string = [](openrac_mod*, const char*) { return host.preview.c_str(); };
    value.setting_bool = [](openrac_mod*, const char*) { return host.chime ? 1 : 0; };
    value.sound_load = [](openrac_mod*, const char*) { return 0; };
    value.sound_play = [](openrac_mod*, int, float, float, int) { return ++host.chimes; };
    value.play_status = [](openrac_play_status* out) {
        *out = host.status;
        return OPENRAC_HOOK_OK;
    };
    value.save_slot_current = [] { return -1; };
    value.save_slot = [](int slot, openrac_save_info*) {
        return slot >= 0 && slot < 4 && host.slots[slot] ? 1 : 0;
    };
    value.save_read = [](int slot, void* out, std::size_t size) -> std::size_t {
        if (slot < 0 || slot >= 4 || !host.slots[slot]) return 0;
        if (out && size >= host.slots[slot]->size()) std::memcpy(out, host.slots[slot]->data(), host.slots[slot]->size());
        return host.slots[slot]->size();
    };
    value.save_capture = [](void* out, std::size_t size) -> std::size_t {
        if (out && size >= host.live.size()) std::memcpy(out, host.live.data(), host.live.size());
        return host.live.size();
    };
    return value;
}
} // namespace

int main(int argc, char** argv) try {
    check(argc == 3, "pass the packaged library and Audiowide font");
#ifdef _WIN32
    const auto dll = LoadLibraryW(fs::absolute(argv[1]).c_str());
    check(dll != nullptr, "load packaged x64 DLL");
    const auto query = reinterpret_cast<openrac_mod_query_fn>(GetProcAddress(dll, OPENRAC_MOD_QUERY));
    const auto load = reinterpret_cast<openrac_mod_load_fn>(GetProcAddress(dll, OPENRAC_MOD_LOAD));
    const auto unload = reinterpret_cast<openrac_mod_unload_fn>(GetProcAddress(dll, OPENRAC_MOD_UNLOAD));
#else
    const auto dll = dlopen(fs::absolute(argv[1]).c_str(), RTLD_NOW | RTLD_LOCAL);
    check(dll != nullptr, "load packaged library");
    const auto query = reinterpret_cast<openrac_mod_query_fn>(dlsym(dll, OPENRAC_MOD_QUERY));
    const auto load = reinterpret_cast<openrac_mod_load_fn>(dlsym(dll, OPENRAC_MOD_LOAD));
    const auto unload = reinterpret_cast<openrac_mod_unload_fn>(dlsym(dll, OPENRAC_MOD_UNLOAD));
#endif
    check(query && load && unload, "plugin exports");
    openrac_mod_info info{};
    info.size = sizeof info;
    query(&info);
    check(info.api_major == 1 && info.api_minor == 14
          && std::string_view(info.id) == "rac2-hd-achievements", "packaged ABI or ID");

#ifdef _WIN32
    const auto pid = GetCurrentProcessId();
#else
    const auto pid = getpid();
#endif
    const auto root = fs::absolute("test-state-" + std::to_string(pid)
        + '-' + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto dir = root / "mod-data" / info.id;
    const auto profile = dir / "profile.txt";
    const auto summary = root / "mod-tools" / info.id / "trophies.json";
    const auto ui = root / "plugins" / info.id / "ui";
    host.data_dir = dir.u8string();
    host.plugins = (root / "plugins").u8string();
    fs::create_directories(ui);
    fs::copy_file(argv[2], ui / "audiowide-latin-400-normal.woff2");
    auto value = api();
    alignas(void*) std::array<std::uint8_t, 64> handle{};
    auto* mod = reinterpret_cast<openrac_mod*>(handle.data());
    value.version = "ntsc2";
    check(load(&value, mod) != 0, "accepted the wrong disc");
    value.version = "ntsc";
    check(load(&value, mod) == 0, "stable API mock rejected");
    check(host.frame && host.overlay, "callbacks missing");

    host.live = fixtures::save(0, 0, 0);
    card(0, 99);
    frame(OPENRAC_PLAY_FRONT_END, -1, 0);
    frame(OPENRAC_PLAY_PLAYING, 0, 1, 64);
    const auto before = read(profile);
    const auto viewer = read(summary);
    check(viewer.find("\"id\":\"more-vandalism\"") != std::string::npos, "no launcher collection");
    host.input.size = sizeof host.input;
    host.input.focused = 1;
    const auto insert = openrac_bind_from_name("Insert");
    host.input.keys[insert] = OPENRAC_KEY_PRESSED;
    frame(OPENRAC_PLAY_MENU, 0, 1);
    draw();
    check(host.draw_calls == 0, "preview drew in a menu");
    frame(OPENRAC_PLAY_PLAYING, 0, 1);
    host.input.keys[insert] = 0;
    draw();
    check(host.draw_calls > 0 && host.chimes == 1, "configured preview or chime didn't draw");
    check(read(profile) == before && read(summary) == viewer, "preview changed progress");

    frame(OPENRAC_PLAY_FRONT_END, -1, 1);
    host.live = fixtures::save(1, 100, 45'000);
    host.slots[0] = host.live;
    card(14, 0);
    frame(OPENRAC_PLAY_FRONT_END, -1, 1);
    card(0, 0);
    frame(OPENRAC_PLAY_PLAYING, 1, 2);
    check(read(summary).find("\"current\":45000,\"total\":50000") != std::string::npos, "slot restore progress");
    host.live = fixtures::save(1, 180, 52'000, 0, true);
    frame(OPENRAC_PLAY_PLAYING, 1, 2, 8);
    const auto updated = read(profile);
    check(updated.find("U more-vandalism\n") != std::string::npos && updated.find("U more-bolts\n") != std::string::npos,
          "live skill point and balance threshold");
    frame(OPENRAC_PLAY_PLAYING, 1, 2, 8);
    check(read(profile) == updated, "duplicate frame changed state");
    draw();
    check(host.chimes == 2, "live unlock did not chime");

    card(19, 0);
    frame(OPENRAC_PLAY_MENU, 1, 2);
    host.slots[0] = host.live;
    card(0, 0);
    frame(OPENRAC_PLAY_MENU, 1, 2);
    frame(OPENRAC_PLAY_FRONT_END, -1, 2);
    const auto saved = read(profile);
    check(std::count(saved.begin(), saved.end(), '\n') > std::count(updated.begin(), updated.end(), '\n'),
          "successful save did not link a checkpoint");
    host.slots[1] = fixtures::save(3, 200, 15'000, 1, true);
    host.live = *host.slots[1];
    card(14, 1);
    frame(OPENRAC_PLAY_FRONT_END, -1, 2);
    card(0, 1);
    frame(OPENRAC_PLAY_PLAYING, 3, 3, 64);
    check(read(summary).find("\"current\":15000,\"total\":100000") != std::string::npos,
          "slot switch mixed balances");
    unload();

    host.chime = false;
    host.preview = "F6";
    const auto chimes = host.chimes;
    check(load(&value, mod) == 0, "failed to reload profile");
    frame(OPENRAC_PLAY_FRONT_END, -1, 0);
    host.live = *host.slots[0];
    card(14, 0);
    frame(OPENRAC_PLAY_FRONT_END, -1, 0);
    card(0, 0);
    frame(OPENRAC_PLAY_PLAYING, 1, 1, 8);
    const auto id = read(profile);
    check(id.find("U more-vandalism\n") != std::string::npos, "profile did not survive restart");
    host.live = fixtures::save(0, 220, 40'000, 1, true);
    frame(OPENRAC_PLAY_PLAYING, 0, 2, 64);
    check(read(summary).find("\"current\":52000,\"total\":100000") != std::string::npos,
          "Challenge Mode transition lost progress");
    const auto calls = host.draw_calls;
    host.input.keys[openrac_bind_from_name("F6")] = OPENRAC_KEY_PRESSED;
    frame(OPENRAC_PLAY_PLAYING, 0, 2);
    draw();
    check(host.draw_calls > calls && host.chimes == chimes, "changed preview key or mute toggle failed");
    {
        host.live = fixtures::save(0, 230, 19'000, 1, true);
        frame(OPENRAC_PLAY_MENU, 0, 2);
        host.live = fixtures::save(0, 231, 37'500, 1, true);
        frame(OPENRAC_PLAY_SCENE, 0, 2);
        host.live = fixtures::save(0, 232, 16'000, 1, true, false, 5'000);
        frame(OPENRAC_PLAY_PLAYING, 0, 2, 64);
        check(read(summary).find("\"current\":75500,\"total\":100000") != std::string::npos,
              "direct reward or pickup offset by spending did not survive");

        card(19, 0);
        frame(OPENRAC_PLAY_MENU, 0, 2);
        host.slots[0] = host.live;
        card(0, 0);
        frame(OPENRAC_PLAY_MENU, 0, 2);
        frame(OPENRAC_PLAY_FRONT_END, -1, 2);
        card(14, 0);
        frame(OPENRAC_PLAY_FRONT_END, -1, 2);
        card(0, 0);
        frame(OPENRAC_PLAY_PLAYING, 0, 3);
        host.live = fixtures::save(0, 244, 40'500, 1, true, false, 5'000);
        frame(OPENRAC_PLAY_PLAYING, 0, 3, 8);
        check(read(profile).find("U bolt-scavenger-ii\n") != std::string::npos &&
              read(summary).find("\"current\":100000,\"total\":1000000") != std::string::npos,
              "cumulative threshold below wallet target did not survive save and reload");
        const auto awarded = read(profile);
        frame(OPENRAC_PLAY_FRONT_END, -1, 3);
        host.live = *host.slots[0];
        card(14, 0);
        frame(OPENRAC_PLAY_FRONT_END, -1, 3);
        card(0, 0);
        frame(OPENRAC_PLAY_PLAYING, 0, 4);
        host.live = fixtures::save(0, 244, 40'500, 1, true, false, 5'000);
        frame(OPENRAC_PLAY_PLAYING, 0, 4, 64);
        check(read(profile) == awarded, "replaying from the earlier save duplicated earned bolts");
    }
    unload();

    const auto boss_dir = root / "mod-data" / "boss-proof";
    host.data_dir = boss_dir.u8string();
    host.chime = true;
    achievements::Profile before_boss;
    before_boss.unlocked.set().reset(3).reset(34);
    std::string error;
    check(achievements::save_profile(boss_dir / "profile.txt", before_boss, error), "could not prepare boss fixture");
    check(load(&value, mod) == 0, "could not open boss fixture");
    host.live = fixtures::save(20, 4800, 0);
    fixtures::word(host.memory.data() + 0x201008, 0xa0);
    frame(OPENRAC_PLAY_PLAYING, 20, 1, 8);
    check(read(boss_dir / "profile.txt").find("U mutation-complete\n") == std::string::npos,
          "the other Yeedil ending request falsely awarded the boss");
    fixtures::word(host.memory.data() + 0x201010, 0xa1);
    frame(OPENRAC_PLAY_SCENE, 20, 1);
    const auto defeated = read(boss_dir / "profile.txt");
    check(defeated.find("U mutation-complete\n") != std::string::npos &&
          defeated.find("U he-went-commando\n") != std::string::npos,
          "native defeat request did not award gold and platinum");
    const auto before_popup = host.chimes;
    draw();
    check(host.chimes == before_popup + 1, "live boss defeat did not show an award");
    frame(OPENRAC_PLAY_SCENE, 20, 1, 8);
    check(read(boss_dir / "profile.txt") == defeated, "boss request was credited twice");
    unload();
#ifdef _WIN32
    FreeLibrary(dll);
#else
    dlclose(dll);
#endif
    std::cout << "Stable API fixture passed clean and existing starts, preview, chime, save, reload, slot switch, and Challenge Mode.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
