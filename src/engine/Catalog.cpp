#include "Catalog.h"

#include "AndroidOut.h"

#include <cassert>
#include <memory>
#include <string>
#include <utility>

namespace engine {
namespace {

struct SimpleDef {
    int x = 0;
    std::string name;
};

} // namespace

void catalogSelfTest() {
    aout << "[catalog] self-test starting" << std::endl;

    {
        Catalog<int> c;
        assert(c.size() == 0);
        assert(!c.has("a"));
        assert(c.find("a") == nullptr);

        c.add("a", 1);
        c.add("b", 2);
        c.add("c", 3);
        assert(c.size() == 3);
        assert(c.has("a"));
        assert(c.has("b"));
        assert(c.has("c"));
        assert(!c.has("d"));

        const int* a = c.find("a");
        assert(a != nullptr);
        assert(*a == 1);

        int* a_mut = c.find("a");
        assert(a_mut != nullptr);
        *a_mut = 100;
        assert(*c.find("a") == 100);

        auto all = c.all();
        assert(all.size() == 3);

        assert(c.remove("b"));
        assert(c.size() == 2);
        assert(!c.has("b"));
        assert(!c.remove("z"));

        c.clear();
        assert(c.size() == 0);
        assert(c.find("a") == nullptr);
    }

    {
        Catalog<std::unique_ptr<int>> c;
        c.add("a", std::make_unique<int>(42));
        c.add("b", std::make_unique<int>(99));
        assert(c.size() == 2);

        const std::unique_ptr<int>* a = c.find("a");
        assert(a != nullptr);
        assert(**a == 42);

        auto drained = c.drain();
        assert(drained.size() == 2);
        assert(c.size() == 0);

        int sum = 0;
        for (const auto& p : drained) sum += *p;
        assert(sum == 141);
    }

    {
        Catalog<SimpleDef> c;
        c.add("knight", SimpleDef{10, "sir"});
        c.add("mage",   SimpleDef{5,  "gandalf"});

        const SimpleDef* k = c.find("knight");
        assert(k != nullptr);
        assert(k->x == 10);
        assert(k->name == "sir");

        const SimpleDef* m = c.find("mage");
        assert(m != nullptr);
        assert(m->x == 5);
        assert(m->name == "gandalf");

        assert(c.find("archer") == nullptr);
    }

    aout << "[catalog] self-test PASS" << std::endl;
}

} // namespace engine
