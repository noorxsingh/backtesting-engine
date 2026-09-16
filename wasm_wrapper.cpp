#include "backtester.hpp"
#include <emscripten.h>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdlib>


static std::vector<Bar> parseCSVText(const std::string& text, std::vector<std::string>& dates) {
    std::vector<Bar> bars;
    std::stringstream file(text);
    std::string line;
    std::getline(file, line); 
    uint64_t index = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string cell;
        std::getline(ss, cell, ','); std::string date = cell;
        std::getline(ss, cell, ','); double open = std::stod(cell);
        std::getline(ss, cell, ','); double high = std::stod(cell);
        std::getline(ss, cell, ','); double low = std::stod(cell);
        std::getline(ss, cell, ','); double close = std::stod(cell);
        std::getline(ss, cell, ','); double volume = std::stod(cell);
        Bar bar;
        bar.open = open; bar.high = high; bar.low = low;
        bar.close = close; bar.volume = volume; bar.timestamp = index++;
        bars.push_back(bar);
        dates.push_back(date);
    }
    return bars;
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
char* runBacktest(const char* csvText,
                  int strategyType, int fast, int slow, int window,
                  int costType, double costRate, double steepness,
                  double startingCash, double targetWeight) {
    std::string result;
    Strategy* strat = nullptr;
    CostModel* cost = nullptr;
    try {
        std::vector<std::string> dates;
        std::vector<Bar> bars = parseCSVText(std::string(csvText), dates);
        if (bars.empty()) {
            result = "{\"error\":\"No data rows parsed. Check the CSV format (Date,Open,High,Low,Close,Volume).\"}";
        } else {
            if (strategyType == 1) strat = new MeanReversion(window);
            else                   strat = new SmaCrossover(fast, slow);

            if (costType == 1) cost = new VolumeSlippage(costRate, steepness);
            else               cost = new FlatCost(costRate);

            BacktestingEngine engine(bars, strat, startingCash, cost, targetWeight);
            engine.run();
            BlotterStats bs = engine.portfolio.tradeStats();

            std::ostringstream json;
            json.setf(std::ios::fixed);
            json.precision(6);
            json << "{";
            json << "\"bars\":" << bars.size() << ",";
            json << "\"totalReturn\":" << engine.totalReturn() << ",";
            json << "\"maxDrawdown\":" << engine.maxDrawdown() << ",";
            json << "\"sharpe\":" << engine.sharpe() << ",";
            json << "\"sortino\":" << engine.sortino() << ",";
            json << "\"cagr\":" << engine.CAGR() << ",";
            json << "\"calmar\":" << engine.Calmar() << ",";
            json << "\"winRate\":" << bs.winRate << ",";
            json << "\"profitFactor\":" << bs.profitFactor << ",";
            json << "\"avgWin\":" << bs.avg_win << ",";
            json << "\"avgLoss\":" << bs.avg_loss << ",";
            json << "\"numTrades\":" << engine.portfolio.trades.size() << ",";
            json << "\"finalEquity\":" << engine.portfolio.equityCurve.back() << ",";
            json << "\"startingCash\":" << startingCash << ",";
            json << "\"equity\":[";
            const auto& eq = engine.portfolio.equityCurve;
            for (size_t i = 0; i < eq.size(); i++) {
                if (i) json << ",";
                json << eq[i];
            }
            json << "],";
            json << "\"close\":[";
            for (size_t i = 0; i < bars.size(); i++) {
                if (i) json << ",";
                json << bars[i].close;
            }
            json << "],";
            json << "\"open\":[";
            for (size_t i = 0; i < bars.size(); i++) { if (i) json << ","; json << bars[i].open; }
            json << "],";
            json << "\"high\":[";
            for (size_t i = 0; i < bars.size(); i++) { if (i) json << ","; json << bars[i].high; }
            json << "],";
            json << "\"low\":[";
            for (size_t i = 0; i < bars.size(); i++) { if (i) json << ","; json << bars[i].low; }
            json << "],";
            json << "\"dates\":[";
            for (size_t i = 0; i < dates.size(); i++) { if (i) json << ","; json << "\"" << dates[i] << "\""; }
            json << "],";
            json << "\"trades\":[";
            const auto& tr = engine.portfolio.trades;
            for (size_t i = 0; i < tr.size(); i++) {
                if (i) json << ",";
                json << "{\"entryBar\":" << tr[i].entryBar
                     << ",\"exitBar\":" << tr[i].exitBar
                     << ",\"entryPrice\":" << tr[i].entryPrice
                     << ",\"exitPrice\":" << tr[i].exitPrice
                     << ",\"shares\":" << tr[i].shares
                     << ",\"pnl\":" << tr[i].pnl << "}";
            }
            json << "]";
            json << "}";
            result = json.str();
        }
    } catch (const std::exception& e) {
        result = std::string("{\"error\":\"Parse error: ") + e.what() + "\"}";
    } catch (...) {
        result = "{\"error\":\"Unknown error during backtest.\"}";
    }
    delete strat;
    delete cost;

    char* out = (char*)malloc(result.size() + 1);
    std::memcpy(out, result.c_str(), result.size() + 1);
    return out;
}

EMSCRIPTEN_KEEPALIVE
void free_result(char* ptr) {
    free(ptr);
}

}
