#pragma once
#include <cstdint>
#include <vector>
#include <string> 



struct Bar {
    public:
    double open;
    double close;
    double high;
    double low;
    uint64_t timestamp;
    double volume;
};

struct TradeStruct {
    public:
    double entryPrice;
    double exitPrice;
    double shares;
    double pnl;
    int entryBar;
    int exitBar; 
};

struct BlotterStats {
    public:
    double winRate;
    double profitFactor;
    double avg_win; 
    double avg_loss; 
};

std::vector<Bar> loadCSV(const std::string& path);

enum class Decision { Buy, Sell, Hold};

class Strategy {
    public:
    virtual int warmup() const = 0; 
    virtual Decision onBar (const std::vector<Bar>& history) = 0; 
    virtual ~Strategy() = default; 
};

class SmaCrossover : public Strategy {
    public:
    SmaCrossover(int fast, int slow);
    Decision onBar(const std::vector<Bar>& history) override;
    int warmup() const override { return slowWindow; } 
    private:
    int fastWindow;
    int slowWindow;
    double sma(int window, const std::vector<Bar>& history) const;
};

class MeanReversion : public Strategy {
    public:
    MeanReversion(int window); 
    Decision onBar(const std::vector<Bar>& history) override; 
    int warmup() const override { return window; } 
    private:
    int window; 
    double sma(int window, const std::vector<Bar>& history) const; 
};

class CostModel {
    public:
    virtual double fillPrice(double price, Decision side, double shares, double volume) const = 0;
    virtual ~CostModel() = default; 
};

class FlatCost : public CostModel {
    public:
    double rate;
    double fillPrice(double price, Decision side, double shares, double volume) const override; 
    FlatCost(double rate); 
};

class VolumeSlippage : public CostModel {
    public: 
    double rate;
    double steepness; 
    double fillPrice(double price, Decision side, double shares, double volume) const override;
    VolumeSlippage(double rate, double steepness);
};

class Portfolio {
    public:
    double cash;
    double positions;
    double entryPrice;
    int entryBar;
    std::vector<double> equityCurve;
    std::vector<TradeStruct> trades;
    Portfolio(double startingCash, CostModel* costM, double targetWeight);
    void execute(Decision signal, double price, double volume, int barIndex);
    double equity(double price) const;
    void mark(double price);
    CostModel* costM; 
    double targetWeight;
    BlotterStats tradeStats() const; 
};  

template<typename StrategyT> 
class TemplatedEngine {
    StrategyT strat;
    std::vector<Bar> bars; 

    public:
    Portfolio portfolio;
    TemplatedEngine(StrategyT strat, std::vector<Bar> bars, double startingCash, CostModel* costM, double targetWeight) : strat(strat), bars(bars), portfolio(startingCash, costM, targetWeight) {};
    void run() {
        std::vector<Bar> history; 
        Decision pending = Decision::Hold;
        for (size_t i = 0; i < bars.size(); ++i) {
            const Bar& bar = bars[i];
            history.push_back(bar);
            portfolio.execute(pending, bar.open, bar.volume, (int)i);
            Decision fresh;
            if ((int)history.size() >= strat.warmup()) {
                fresh = strat.onBar(history); 
            } else {
                fresh = Decision::Hold; 
            }
            pending = fresh; 
            portfolio.mark(bar.close);
        }
    }
};

class BacktestingEngine {
    public:
    BacktestingEngine(std::vector<Bar> bars, Strategy* strat, double startingCash, CostModel* costM, double targetWeight);
    std::vector<Bar> bars;
    Strategy* strat;
    Portfolio portfolio;
    void run(); 
    double totalReturn() const;
    double maxDrawdown() const;
    double sharpe() const;
    double CAGR() const;
    double Calmar() const;
    double sortino() const; 
};