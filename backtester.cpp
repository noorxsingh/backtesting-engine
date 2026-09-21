#include "backtester.hpp"
#include <vector>
#include <string> 
#include <algorithm>
#include <cmath>
#include <string>
#include <fstream> 
#include <sstream> 

SmaCrossover::SmaCrossover(int fast, int slow) : fastWindow(fast), slowWindow(slow) {};

double SmaCrossover::sma(int window, const std::vector<Bar>& history) const {
    double sum = 0.0;
    for (int i = (int)history.size() - window; i < (int)history.size(); ++i) {
        sum += history[i].close; 
    }
    return (sum/window);
}

Decision SmaCrossover::onBar(const std::vector<Bar>& history) {

    if ((int)history.size() < slowWindow) {
        return Decision::Hold;
    }
    double fast = sma(fastWindow, history);
    double slow = sma(slowWindow, history);
    if (fast > slow) {
        return Decision::Buy;
    } else if (fast < slow) {
        return Decision::Sell;
    } else {
        return Decision::Hold; 
    }
}

MeanReversion::MeanReversion(int window) : window(window) {}; 

double MeanReversion::sma(int window, const std::vector<Bar>& history) const {
    double sum = 0.0;
    for (int i = (int)history.size() - window; i < (int)history.size(); ++i) {
        sum += history[i].close; 
    }
    return (sum/window);
}

Decision MeanReversion::onBar(const std::vector<Bar>& history) {
    double avg = sma(window, history); 
    if (history.back().close < avg) {
        return Decision::Buy; 
    } else if (history.back().close > avg) {
        return Decision::Sell; 
    } else {
        return Decision::Hold; 
    }
}

FlatCost::FlatCost(double rate) : rate(rate) {}; 

double FlatCost::fillPrice(double price, Decision side, double shares, double volume) const {
    if (side == Decision::Buy) {
        return price * (1 + rate);
    } else if (side == Decision::Sell) {
        return price * (1 - rate);
    } else { return price; } 
}

VolumeSlippage::VolumeSlippage(double rate, double steepness) : rate(rate), steepness(steepness) {}; 

double VolumeSlippage::fillPrice(double price, Decision side, double shares, double volume) const {
    double participation = shares / volume;
    double push = rate + steepness * participation; 
    if (side == Decision::Buy) {
        return price * (1 + push);
    } else if (side == Decision::Sell) {
        return price * (1 - push);
    } else { return price; } 
}

Portfolio::Portfolio(double startingCash, CostModel* costM, double targetWeight) : cash(startingCash), positions(0), entryPrice(0), entryBar(0),costM(costM), targetWeight(targetWeight) {};

double Portfolio::equity(double price) const {
    return cash + positions * price;
}

void Portfolio::mark(double price) {
    double equityVal = equity(price); 
    equityCurve.push_back(equityVal); 
}

void Portfolio::execute(Decision signal, double price, double volume, int barIndex) {
    double dollarsToDeploy = targetWeight * cash;

    int shares = std::floor(dollarsToDeploy / price); 

    if (signal == Decision::Buy && positions == 0 && cash > 0) {
        entryPrice = costM->fillPrice(price, signal, shares, volume); 
        positions = std::floor(dollarsToDeploy / entryPrice); 
        cash -= positions * entryPrice;
        entryBar = barIndex;
    } else if (signal == Decision::Sell && positions > 0) {
        double exitPrice = costM->fillPrice(price, signal, positions, volume);
        double pnl = (exitPrice - entryPrice) * positions;
        trades.push_back({entryPrice, exitPrice, positions, pnl, entryBar, barIndex});
        cash += positions * exitPrice;
        positions = 0;
    } else {
        return;
    }
}

BlotterStats Portfolio::tradeStats() const {
    int winningTrades = 0;
    int losingTrades = 0;
    double pos_pnl = 0.0;
    double neg_pnl = 0.0;

    for (const TradeStruct& t : trades) {
        if (t.pnl > 0 ) {
            winningTrades++;
            pos_pnl += t.pnl;
        } else if (t.pnl < 0) {
            losingTrades++;
            neg_pnl += t.pnl;    
        } 
    }
    double winRate = 0.0;
    double profitFactor = 0.0;
    double avg_win = 0.0;
    double avg_loss = 0.0; 

    if (trades.size() != 0)   winRate  = (double) winningTrades / (double) trades.size();

    if (neg_pnl != 0)         profitFactor = pos_pnl / neg_pnl * -1.0;

    if (winningTrades != 0)   avg_win = pos_pnl / winningTrades;

    if (losingTrades != 0)    avg_loss = neg_pnl / losingTrades;

return {winRate, profitFactor, avg_win, avg_loss};

}

BacktestingEngine::BacktestingEngine(std::vector<Bar> bars, Strategy* strat, double startingCash, CostModel* costM, double targetWeight) : bars(bars), strat(strat), portfolio(startingCash, costM, targetWeight) {};

void BacktestingEngine::run() {
    std::vector<Bar> history;
    Decision pending = Decision::Hold;
    for (size_t i = 0; i < bars.size(); ++i) {
        const Bar& bar = bars[i];
        history.push_back(bar);
        portfolio.execute(pending, bar.open, bar.volume, (int)i);
        Decision fresh;
        if ((int)history.size() >= strat->warmup()) {
            fresh = strat->onBar(history); 
        } else {
            fresh = Decision::Hold; 
        }
        pending = fresh; 
        portfolio.mark(bar.close);
    }
}

double BacktestingEngine::totalReturn() const {
    if (portfolio.equityCurve.empty()) {
        return 0;
    }
    auto first = portfolio.equityCurve[0];
    auto last = portfolio.equityCurve[portfolio.equityCurve.size() - 1];
    return (last / first) - 1.0;
}

double BacktestingEngine::maxDrawdown() const {
    if (portfolio.equityCurve.size() == 0) {
        return 0; 
    }
    double peak = portfolio.equityCurve[0];
    double worst = 0.0;
    for (int i = 1; i < (int)portfolio.equityCurve.size(); ++i) {
        if (peak <= portfolio.equityCurve[i]) {
            peak = portfolio.equityCurve[i];
        } else if (peak > portfolio.equityCurve[i]) {
            double drawdown = (peak - portfolio.equityCurve[i]) / peak;
            if (drawdown > worst) {
                worst = drawdown;
            }
        }
    }
    return worst;
};

double BacktestingEngine::sharpe() const {
    if (portfolio.equityCurve.size() < 2) {
        return 0;
    }
    std::vector<double> sharpeVector;
    double sum = 0.0;
    double mean = 0.0;
    for (int i = 1; i < (int)portfolio.equityCurve.size(); ++i) {
        double localSum = ((portfolio.equityCurve[i] / portfolio.equityCurve[i-1]) - 1);
        sharpeVector.push_back(localSum); 
        sum += localSum; 
    }
    mean = sum / sharpeVector.size();
    double sqSum = 0.0;
    for (int i = 0; i < (int)sharpeVector.size(); ++i) {
        auto newVal = (sharpeVector[i] - mean) * (sharpeVector[i] - mean);
        sqSum += newVal;
    }
    auto variance = sqSum / sharpeVector.size();
    auto stddev = std::sqrt(variance);
    if (stddev == 0) {
        return 0;
    }

    return (mean / stddev) * std::sqrt(252.0); 
}

double BacktestingEngine::sortino() const {
    if (portfolio.equityCurve.size() < 2) {
        return 0;
    }
    std::vector<double> sharpeVector;
    double sum = 0.0;
    double mean = 0.0;
    for (int i = 1; i < (int)portfolio.equityCurve.size(); ++i) {
        double localSum = ((portfolio.equityCurve[i] / portfolio.equityCurve[i-1]) - 1);
        sharpeVector.push_back(localSum); 
        sum += localSum; 
    }
    mean = sum / sharpeVector.size();
    double sqSum = 0.0;
    for (int i = 0; i < (int)sharpeVector.size(); ++i) {
        if (sharpeVector[i] < 0) {
            auto newVal = (sharpeVector[i]) * (sharpeVector[i]);
            sqSum += newVal;
        }
    }
    auto downSideVariance = sqSum / sharpeVector.size();
    auto downsideStddev = std::sqrt(downSideVariance);
    if (downsideStddev == 0) {
        return 0;
    }
    return (mean / downsideStddev) * std::sqrt(252.0); 
}

double BacktestingEngine::CAGR() const {
    double comp_annual_growth_rate = std::pow((1 + totalReturn()), (double) 1 / (portfolio.equityCurve.size() / 252.0)) - 1; 
    return comp_annual_growth_rate; 
}

double BacktestingEngine::Calmar() const {
    if (maxDrawdown() == 0) {
        return 0; 
    }
    double cal = (double) (CAGR() / maxDrawdown()); 
    return cal; 
}

std::vector<Bar> loadCSV(const std::string& path) {
    std::ifstream file(path);
    std::vector<Bar> bars;
    std::string line;
    uint64_t index = 0;

    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string field;

        std::getline(ss, field, ','); 

        std::getline(ss, field, ',');
        double open = std::stod(field);

        std::getline(ss, field, ',');
        double high = std::stod(field);

        std::getline(ss, field, ',');
        double low = std::stod(field);

        std::getline(ss, field, ',');
        double close = std::stod(field);

        std::getline(ss, field, ',');
        double volume = std::stod(field);

        Bar b;
        b.timestamp = index;
        b.open = open;
        b.high = high;
        b.low = low;
        b.close = close;
        b.volume = volume;
        bars.push_back(b);
        index++;
    }
    return bars;
}   