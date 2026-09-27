#include "Bag.h"
#include "EquipSlots.h"

#include "AndroidOut.h"

#include <cassert>
#include <string>
#include <utility>

namespace engine {
namespace {

struct Potion {
    std::string name;
    int heal = 0;
};

} // namespace

void bagSelfTest() {
    aout << "[bag] self-test starting" << std::endl;

    {
        Bag<int> b;
        assert(b.empty());
        assert(b.size() == 0);
        assert(b.totalItems() == 0);
        assert(b.countOf("x") == 0);
        assert(b.find("x") == nullptr);

        b.add("x", 1);
        b.add("y", 2);
        assert(b.size() == 2);
        assert(b.totalItems() == 2);
        assert(b.countOf("x") == 1);

        b.add("x", 10, 3);
        assert(b.size() == 2);
        assert(b.countOf("x") == 4);

        const auto* e = b.find("x");
        assert(e != nullptr);
        assert(e->item == 1);
        assert(e->count == 4);

        assert(b.remove("x", 2));
        assert(b.countOf("x") == 2);
        assert(b.remove("x", 10));
        assert(b.countOf("x") == 0);
        assert(b.find("x") == nullptr);
        assert(b.size() == 1);

        assert(!b.remove("z"));
        b.clear();
        assert(b.empty());
    }

    {
        Bag<Potion> b;
        b.add("heal_small", Potion{"small", 10}, 3);
        b.add("heal_large", Potion{"large", 50});

        assert(b.size() == 2);
        assert(b.countOf("heal_small") == 3);
        assert(b.countOf("heal_large") == 1);
        assert(b.totalItems() == 4);

        const auto* e = b.find("heal_large");
        assert(e != nullptr);
        assert(e->item.heal == 50);
    }

    {
        Bag<std::string, int> b;
        b.add(1, "one");
        b.add(2, "two");
        b.add(1, "ONE", 2);
        assert(b.size() == 2);
        assert(b.countOf(1) == 3);
        assert(b.countOf(2) == 1);
    }

    aout << "[bag] self-test PASS" << std::endl;
}

} // namespace engine
