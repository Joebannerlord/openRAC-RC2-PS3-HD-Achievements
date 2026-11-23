namespace {
enum Index {
    platinum_power, super_skilled, destruction_time, mutation_complete, bolts_2m,
    wrench_joba, nanotech, impossible, nice_ride, bolts_1m, midtown, wrench_chainblade,
    ryno, zodiac, chi, deposit, vandalism, jackpot, rampage, bolts_100k, speed,
    prehistoric, museum, moon, brawler, machinery, shocking, bolts_50k, snow, birds,
    tourists, sheep, ships, carbonox, platinum
};

constexpr std::array<Trophy, 35> trophies{{
    {"platinum-power", "Platinum Power", "Find all 40 Platinum Bolts.", Grade::gold},
    {"super-skilled", "Super Skilled", "Earn all 30 skill points.", Grade::gold},
    {"destruction-time", "Destruction Time", "Own every weapon and reach its first experience upgrade where available.", Grade::gold},
    {"mutation-complete", "Mutation Complete", "Defeat the Mutated Protopet on Yeedil.", Grade::gold},
    {"i-got-bolts-to-trade", "I got bolts to trade", "Collect 2,000,000 bolts in one campaign.", Grade::gold},
    {"wrench-ninja-massacre", "Wrench Ninja II: Massacre", "Defeat Joba's enemies outside the arena with the wrench without dying.", Grade::silver},
    {"nano-to-the-max", "Nano To The Max", "Reach 80 maximum nanotech.", Grade::silver},
    {"thats-impossible", "That's Impossible!", "Win the Impossible Challenge at the Megacorp Games on Joba.", Grade::silver},
    {"nice-ride", "Nice Ride", "Purchase the ship's weapons, shields and performance upgrades.", Grade::silver},
    {"bolted-down", "Bolted Down", "Collect 1,000,000 bolts in one campaign.", Grade::silver},
    {"midtown-insanity", "Midtown Insanity", "Grind Damosel's train rails without taking damage.", Grade::silver},
    {"wrench-ninja-blade-to-blade", "Wrench Ninja: Blade to Blade", "Defeat Chainblade in the arena using only the wrench.", Grade::silver},
    {"now-its-time", "Now its time...", "Acquire the RYNO II.", Grade::silver},
    {"bfg", "BFG", "Acquire the Zodiac.", Grade::silver},
    {"heal-your-chi", "Heal Your Chi", "Gather all 100 desert crystals on Tabora.", Grade::bronze},
    {"safety-deposit", "Safety Deposit", "Save all four civilians in Damosel's bank from the robots.", Grade::bronze},
    {"more-vandalism", "More Vandalism?", "Smash every breakable in the Megacorp Store on Oozla.", Grade::bronze},
    {"clank-needs-new-shoes", "Clank Needs a New Pair of Shoes", "Win the 300-bolt slot-machine jackpot and its skill point.", Grade::bronze},
    {"robo-rampage", "Robo Rampage", "Destroy all the city buildings during the Giant Clank fight over Dobbo.", Grade::bronze},
    {"bolt-scavenger-ii", "Bolt Scavenger II", "Collect 100,000 bolts in one campaign.", Grade::bronze},
    {"speed-demon", "Speed Demon", "Finish a Barlow hoverbike race in 2:10 or less.", Grade::bronze},
    {"prehistoric-rampage", "Prehistoric Rampage", "Shoot down four pterodactyls on Oozla.", Grade::bronze},
    {"museum-tour", "Museum Tour", "Gain access to the Insomniac Museum.", Grade::bronze},
    {"be-a-moon-child", "Be a Moon Child", "Mine all 101 moonstones on Grelbin.", Grade::bronze},
    {"2b-hit-or-not", "2B Hit or Not 2B Hit", "Defeat B2 Brawler without taking damage.", Grade::bronze},
    {"operate-heavy-machinery", "Operate Heavy Machinery", "Use the cranes to destroy ten robots on Endako.", Grade::bronze},
    {"no-shocking-developments", "No Shocking Developments", "Grind Boldan's power cable through the lumber yard without taking damage.", Grade::bronze},
    {"more-bolts", "More Bolts?", "Collect 50,000 bolts in one campaign.", Grade::bronze},
    {"snow-dan", "You can Break a Snow Dan", "Destroy the hidden snowman on Siberius.", Grade::bronze},
    {"bye-bye-birdies", "Bye Bye Birdies", "Kill twelve birds on Joba.", Grade::bronze},
    {"youre-my-hero", "You're My Hero", "Save all of Todano's tourists from the squirrels.", Grade::bronze},
    {"try-to-sleep", "Try to Sleep", "Use the Sheepinator to turn sixteen Todano squirrels into sheep.", Grade::bronze},
    {"moving-violation", "Moving Violation", "Shoot down fourteen ships on Snivelak.", Grade::bronze},
    {"yay-carbon", "Yay Carbon", "Acquire Carbonox armor.", Grade::bronze},
    {"he-went-commando", "He Went Commando", "Earn the other 34 Going Commando HD trophies.", Grade::platinum},
}};

// Native awards in the USA v1.01 game's chunk 8, in trophy-catalog order.
constexpr std::pair<Index, unsigned> skill_trophies[] = {
    {destruction_time, 26}, {wrench_joba, 18}, {midtown, 7}, {wrench_chainblade, 1},
    {chi, 5}, {deposit, 27}, {vandalism, 15}, {jackpot, 25}, {rampage, 24},
    {speed, 2}, {prehistoric, 13}, {moon, 6}, {brawler, 19}, {machinery, 28},
    {shocking, 4}, {snow, 16}, {birds, 20}, {tourists, 10}, {sheep, 22}, {ships, 11},
};
constexpr std::pair<Index, unsigned> bolt_trophies[] = {
    {bolts_50k, 50000}, {bolts_100k, 100000}, {bolts_1m, 1000000}, {bolts_2m, 2000000},
};
// USA v1.01 vendor record prices for weapons only obtained through a paid stock sale.
constexpr std::pair<unsigned, unsigned> paid_weapons[] = {
    {9, 1'000'000},  // Clank Zapper
    {43, 1'500'000}, // Zodiac
    {44, 1'000'000}, // RYNO II
};
constexpr unsigned ship_masks[] = {0x30, 0x40, 0x180, 0x200, 0xc00, 0x1000, 0x2000, 0xc000};
constexpr unsigned ship_best = 0xbb50;

// Item IDs and eligible record rows from the stable launcher's save editor.
// Zero is an ownership-only weapon. A Mega Sheepinator is possible without XP.
struct Weapon { unsigned item; std::array<unsigned, 3> upgraded; };
constexpr Weapon weapons[] = {
    {9, {73}}, {12, {}}, {14, {}}, {16, {72, 110}}, {17, {}}, {18, {}},
    {22, {65, 83, 84}}, {23, {66, 87, 88}}, {24, {67, 85, 86}}, {25, {70, 103, 104}},
    {26, {68, 91, 92}}, {27, {69, 99, 100}}, {28, {62, 101, 102}}, {29, {63, 95, 96}},
    {30, {60, 79, 80}}, {31, {61, 93, 94}}, {32, {78, 105, 106}}, {37, {76, 97, 98}},
    {41, {64, 89, 90}}, {42, {71, 81, 82}}, {43, {}}, {44, {}}, {45, {77, 107, 108}}, {53, {}},
};

const std::uint8_t* chunk(const std::uint8_t* section, std::uint32_t section_size,
                          int id, std::uint32_t expected) {
    std::uint32_t size = 0;
    const auto* found = rac2_save_chunk(section, section_size, id, &size);
    return size == expected ? found : nullptr;
}
} // namespace

const std::array<Trophy, 35>& catalog() { return trophies; }

std::optional<Snapshot> decode_save(const std::uint8_t* bytes, std::size_t size) {
    static_assert(save_size == RAC2_SAVE_SIZE);
    if (size != save_size || !rac2_save_whole(bytes, static_cast<std::uint32_t>(size))) return std::nullopt;
    const auto* global = bytes + 8;
    const auto field = [global](int id, unsigned count) {
        return chunk(global, RAC2_SAVE_GLOBAL_SIZE, id, count);
    };
    const auto* level = field(0, 4);
    const auto* ticks = field(3, 4);
    const auto* flags = field(5, 205);
    const auto* skills = field(8, 32);
    const auto* owned = field(10, 56);
    const auto* records = field(30, 80);
    const auto* progress = field(39, 64);
    const auto* scenes = field(43, 256);
    const auto* ship = field(44, 4);
    if (!level || !ticks || !flags || !skills || !owned || !records || !progress || !scenes || !ship) {
        return std::nullopt;
    }

    const auto level_value = rac2_save_u32(level);
    if (level_value >= RAC2_SAVE_LEVEL_LIMIT) return std::nullopt;
    Snapshot out;
    out.level = static_cast<int>(level_value);
    out.play_ticks = rac2_save_u32(ticks);
    out.wallet = rac2_save_u32(progress);
    if (out.wallet > 0x7fffffff) return std::nullopt;
    const auto health = rac2_save_u32(progress + 8);
    out.nanotech = health <= 80 ? health : 0;
    out.challenge = progress[0x12];
    out.armor = progress[0x20];
    for (std::size_t i = 0; i < out.skills.size(); ++i) out.skills[i] = skills[i] != 0;
    for (std::size_t i = 0; i < out.owned.size(); ++i) out.owned[i] = owned[i] != 0;
    for (const auto& [item, price] : paid_weapons) {
        if (out.owned[item]) out.proven_bolt_spend += price;
    }
    for (const auto& weapon : weapons) {
        const auto& versions = weapon.upgraded;
        const unsigned record = records[weapon.item];
        out.weapons_ready += out.owned[weapon.item] && (versions[0] == 0
            || (record != 0 && std::find(versions.begin(), versions.end(), record) != versions.end()));
    }
    out.impossible = flags[70] != 0;
    // Starting Challenge Mode increments this counter while resetting the ending scene flags.
    out.protopet = out.challenge != 0 || (scenes[0xa1 / 8] & (1u << (0xa1 % 8))) != 0;
    const auto ship_word = rac2_save_u32(ship);
    for (auto mask : ship_masks) out.ship_parts += (ship_word & mask) == (ship_best & mask);
    for (unsigned i = 0; i < RAC2_SAVE_LEVELS; ++i) {
        const auto* section = global + RAC2_SAVE_GLOBAL_SIZE + i * RAC2_SAVE_LEVEL_SIZE;
        const auto* found = chunk(section, RAC2_SAVE_LEVEL_SIZE, 3003, 4);
        const auto* score = chunk(section, RAC2_SAVE_LEVEL_SIZE, 4008, 4);
        if (!found || !score) return std::nullopt;
        out.platinum_bolts += static_cast<unsigned>(std::count_if(found, found + 4, [](auto f) { return f != 0; }));
        const auto level_score = rac2_save_u32(score);
        if (level_score <= 0x7fffffff) out.scored_bolts += level_score;
    }
    return out;
}

std::optional<Progress> progress(const Profile& profile, const std::string& campaign,
                                 const Snapshot& s, std::size_t i) {
    for (const auto& [index, total] : bolt_trophies) {
        if (i != index) continue;
        const auto found = profile.campaigns.find(campaign);
        return Progress{found == profile.campaigns.end() ? s.bolt_floor() : found->second.bolts, total};
    }
    switch (i) {
        case platinum_power: return Progress{s.platinum_bolts, 40};
        case super_skilled: return Progress{s.skills.count(), 30};
        case destruction_time: return Progress{s.weapons_ready, std::size(weapons)};
        case nanotech: return Progress{s.nanotech, 80};
        case nice_ride: return Progress{s.ship_parts, std::size(ship_masks)};
        case platinum: return Progress{profile.unlocked.count() - profile.unlocked[platinum], platinum};
        default: return std::nullopt;
    }
}

Unlocks evaluate(Profile& profile, const std::string& campaign, const Snapshot& s, bool visited_museum) {
    if (!campaign.empty()) {
        auto& counter = profile.campaigns[campaign].bolts;
        counter = std::max(counter, s.bolt_floor());
    }
    Unlocks unlocked;
    const auto award = [&](Index i, bool condition) {
        if (condition && !profile.unlocked[i]) {
            profile.unlocked.set(i);
            unlocked.push_back(i);
        }
    };
    for (const auto& [index, skill] : skill_trophies) award(index, s.skills[skill]);
    for (const auto& [index, total] : bolt_trophies) award(index, progress(profile, campaign, s, index)->current >= total);
    award(platinum_power, s.platinum_bolts >= 40);
    award(super_skilled, s.skills.all());
    award(mutation_complete, s.protopet);
    award(nanotech, s.nanotech == 80 || s.skills[23]);
    award(impossible, s.impossible);
    award(nice_ride, s.ship_parts == std::size(ship_masks) || s.skills[29]);
    award(ryno, s.owned[44]);
    award(zodiac, s.owned[43]);
    award(museum, visited_museum || s.level == 21 || s.level == 30);
    award(carbonox, s.armor == 4);
    award(platinum, profile.unlocked.count() == platinum);
    return unlocked;
}
