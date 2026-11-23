#include "save_fixture.hpp"

#include <fstream>
#include <iostream>

using namespace achievements;
using fixtures::check;

int main(int argc, char** argv) try {
    const auto fresh = fixtures::save(0, 0, 0);
    const auto blank = decode_save(fresh.data(), fresh.size());
    check(blank && blank->level == 0 && blank->skills.none(), "fresh save did not decode");
    Profile profile;
    check(evaluate(profile, "fresh", *blank).empty(), "clean start earned a trophy");
    const auto plus = fixtures::save(0, 5'000, 0, 2);
    const auto returned = decode_save(plus.data(), plus.size());
    Profile challenge;
    check(returned && returned->protopet &&
          evaluate(challenge, "challenge", *returned) == Unlocks{3},
          "Challenge Mode without the ending scene flag did not prove Mutation Complete");

    const auto scored = fixtures::save(1, 600, 3'000, 0, false, false, 50'000);
    const auto spent = decode_save(scored.data(), scored.size());
    check(spent && spent->scored_bolts == 50'000, "save score did not sum across levels");
    Profile historical;
    check(evaluate(historical, "spent", *spent).size() == 1 &&
          historical.campaigns.at("spent").bolts == 50'000, "save score did not prove spent bolt trophy");

    auto bytes = fixtures::save(20, 4000, 2'000'000, 1, true, true);
    const auto completed = decode_save(bytes.data(), bytes.size());
    check(completed && completed->platinum_bolts == 40 && completed->challenge == 1, "complete save did not decode");
    const auto paid = fixtures::save(20, 4000, 40'500, 1, true, true, 500'000);
    const auto paid_save = decode_save(paid.data(), paid.size());
    check(paid_save && paid_save->proven_bolt_spend == 2'500'000 &&
          paid_save->bolt_floor() == 2'540'500, "RYNO II or Zodiac price was lost or counted twice");
    Profile purchases;
    evaluate(purchases, "paid", *paid_save);
    check(purchases.unlocked[4], "old weapon purchases did not prove the two-million trophy");
    const auto all_but_museum = evaluate(profile, "slot0", *completed);
    check(all_but_museum.size() == 33 && !profile.unlocked[34], "completion earned premature platinum");
    const auto final = evaluate(profile, "slot0", *completed, true);
    check(final.size() == 2 && final.back() == 34 && profile.unlocked.all(), "platinum did not follow final trophy");
    check(evaluate(profile, "slot0", *completed, true).empty(), "duplicate observation re-awarded trophies");

    evaluate(profile, "slot1", *blank);
    check(profile.campaigns.at("slot0").bolts == 4'500'000 && profile.campaigns.at("slot1").bolts == 0,
          "slot progress was mixed");
    evaluate(profile, "slot0", *blank);
    check(profile.campaigns.at("slot0").bolts == 4'500'000, "reload lowered the proven total");
    bytes[26] ^= 1;
    check(!decode_save(bytes.data(), bytes.size()), "corrupt global packet accepted");
    check(!decode_save(fresh.data(), fresh.size() - 1), "truncated save accepted");
    check(!decode_save(nullptr, save_size), "null save accepted");

    if (argc >= 2) {
        std::ifstream file(argv[1], std::ios::binary);
        file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        check(static_cast<std::size_t>(file.gcount()) == bytes.size() && file.peek() == EOF, "save fixture dimensions");
        const auto real = decode_save(bytes.data(), bytes.size());
        check(real.has_value(), "real RAC2 save failed decoding");
        if (argc == 3 && std::string(argv[2]) == "--finished") {
            check(real->protopet, "real save did not prove Mutation Complete");
        }
        std::cout << "Existing save decoded: " << real->skills.count() << " skill points, "
                  << real->platinum_bolts << " Platinum Bolts. Challenge counter " << real->challenge
                  << ", Protopet defeat " << (real->protopet ? "proved" : "unproven") << ".\n";
    }
    for (unsigned i = 0; i < catalog().size(); ++i) {
        const auto& trophy = catalog()[i];
        std::cout << trophy.name << (profile.unlocked[i] ? ": verified in fixture\n" : ": missing\n");
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
