#include "backtester.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

static std::vector<Bar> makeSampleData(int n) {
    std::vector<Bar> bars;
    for (int i = 0; i < n; ++i) {
        double close = 100.0 + 0.3 * i + 15.0 * std::sin(i / 6.0);
        Bar b;
        b.timestamp = (uint64_t)i;
        b.open = close - 0.5; b.high = close + 1.0; b.low = close - 1.0;
        b.close = close; b.volume = 1000.0;
        bars.push_back(b);
    }
    return bars;
}

int main() {
     std::vector<Bar> data = loadCSV("data.csv");
    for (int window = 5; window <= 30; window += 5) {
        MeanReversion strategy(window);
        BacktestingEngine engine(data, &strategy, 10000.0, .001, .5);
        engine.run();  
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "=== Backtest results ===\n";
        std::cout << "window: " << window << std::endl; 
        std::cout << "Total return: " << engine.totalReturn() * 100.0 << "%\n";
        std::cout << "Max drawdown: " << engine.maxDrawdown() * 100.0 << "%\n";
        std::cout << "Sharpe:       " << engine.sharpe() << "\n";
    }
}