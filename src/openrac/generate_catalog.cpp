#include "launcher_summary.hpp"
#include "../core/atomic_file.hpp"

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    std::string error;
    if (!achievements::write_file_atomically(argv[1], achievements::trophy_summary({}), error)) {
        std::cerr << error << '\n';
        return 1;
    }
    return 0;
}
