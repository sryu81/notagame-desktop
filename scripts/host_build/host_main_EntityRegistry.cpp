#include "EntityRegistry.h"
#include "AndroidOut.h"

int main() {
    aout << "host EntityRegistry driver starting" << std::endl;
    game::EntityRegistry::selfTest();
    aout << "host EntityRegistry driver PASS" << std::endl;
    return 0;
}
