# Low-latency Backtesting Engine

An event-driven backtesting engine written from scratch in modern C++, compiled to WebAssembly so it runs entirely in the browser. Write a strategy, plug in a cost model, run it on your own OHLCV data, and get a full institutional metrics suite plus an equity curve. No server, no dependencies.

**Live demo:** https://noorxsingh.github.io/backtesting-engine/

## What it does

- **Two strategies out of the box:** SMA crossover and mean reversion.
- **Pluggable cost models:** a flat transaction cost, and a participation-based volume-slippage model (`impact = steepness × shares / bar volume`) that correctly vanishes for small orders and punishes large ones.
- **Full metrics suite:** total return, CAGR, max drawdown, Sharpe, Sortino, Calmar, win rate, profit factor, trade count, and final equity.
- **Realistic execution:** signals are computed on the closed bar and filled at the next open, so there is no lookahead bias.
- **Runs in the browser:** the C++ engine is compiled to WebAssembly with Emscripten and bundled into a single HTML file. Upload any `Date,Open,High,Low,Close,Volume` CSV.

## Design

The core idea is that the **interfaces are the product**. The engine never knows which strategy or cost model it is running.

- `Strategy` is an abstract base with `warmup()` and `onBar(history)`. Add a strategy by subclassing it; the engine is untouched.
- `CostModel` is an abstract base with `fillPrice(price, side, shares, volume)`. Add a fill model the same way.

The `BacktestingEngine` holds a `Strategy*` and the `Portfolio` holds a `CostModel*`, so both are swapped by a single line at the call site.

## Build and run natively

```sh
clang++ -std=c++17 main.cpp backtester.cpp -o bt && ./bt
```

`main.cpp` runs a walk-forward analysis over `data.csv`. You must compile both `.cpp` files together.

## Build the web UI

The browser build needs [Emscripten](https://emscripten.org/).

```sh
em++ -std=c++17 -O2 wasm_wrapper.cpp backtester.cpp -o engine.js \
  -s MODULARIZE=1 -s EXPORT_NAME=createBacktester \
  -s "EXPORTED_RUNTIME_METHODS=['cwrap','ccall','UTF8ToString']" \
  -s "EXPORTED_FUNCTIONS=['_runBacktest','_free_result','_malloc','_free']" \
  -s ALLOW_MEMORY_GROWTH=1 -s SINGLE_FILE=1
```

Then inline `engine.js` and Chart.js into the page template to produce the single self-contained `index.html`.

## License

MIT. See [LICENSE](LICENSE).
