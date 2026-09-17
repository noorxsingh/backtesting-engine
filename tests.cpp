#include "backtester.hpp"
#include <iostream>
#include <cassert>

int main() {
    FlatCost cost(0.01);
    Portfolio p(10000.0, &cost, 1.0);   // targetWeight 1.0, the case that used to break

    // simulate a few bars of buying and selling
    p.execute(Decision::Buy,  100.0, 1e9, 0);
    // >>> your assertion here: cash must never be negative, positions never negative
    assert(p.cash >= 0); 

    p.execute(Decision::Sell, 105.0, 1e9, 1);
    // >>> and here too
    assert(p.positions >= 0); 

    std::cout << "all invariants held\n";
    return 0;
}