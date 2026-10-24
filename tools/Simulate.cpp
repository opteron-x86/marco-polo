#include "core/Economy.h"
#include <fstream>
#include <iostream>
int main(int argc, char **argv) {
    try {
        if (argc < 2) {
            std::cerr << "marco_sim economy.json [days] [seed]\n";
            return 2;
        }
        std::ifstream file(argv[1]);
        auto config = marco::Config::Parse(marco::json::parse(file));
        marco::Economy e(config);
        e.NewGame(0, argc > 3 ? std::stoull(argv[3]) : 42);
        auto days = argc > 2 ? std::stoul(argv[2]) : 365;
        while (e.Advance(days, 32))
            e.Validate();
        std::cout << "market,commodity,stock,multiplier,produced,consumed,imports,exports,losses\n";
        for (auto &[id, m] : e.Get().markets)
            for (auto &[c, s] : m.goods)
                std::cout << id << ',' << c << ',' << s.quantity << ',' << s.price << ','
                          << s.produced << ',' << s.consumed << ',' << s.imported << ','
                          << s.exported << ',' << s.lost << '\n';
    } catch (const std::exception &ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
