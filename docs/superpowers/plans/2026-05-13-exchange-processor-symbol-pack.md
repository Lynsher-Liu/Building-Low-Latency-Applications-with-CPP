# ExchangeProcessor Symbol-Pack Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace runtime `unordered_map` + hard-coded `variant` in `ExchangeProcessor` and ticker-id indexed `PositionKeeper` with compile-time symbol-pack templates, keeping a low-overhead runtime routing path.

**Architecture:** `ExchangeProcessor<E, Symbols...>` owns `OrderBook<E, Symbols>...` in a tuple and a `PositionKeeper<E, Symbols...>`. Runtime `SymbolName` routing uses a tiny linear lookup over the compile-time pack. Fills map `TickerId -> SymbolName` via a constexpr helper and update positions by symbol.

**Tech Stack:** C++17 templates, `std::tuple`, `std::array`, low-overhead runtime dispatch.

---

### Task 1: Add symbol-pack PositionKeeper

**Files:**
- Modify: `hft/trading/strategy/position_keeper.h`

- [ ] **Step 1: Refactor PositionKeeper to template**
  - Introduce `template<ExchangeName E, SymbolName... Symbols> class PositionKeeper`.
  - Replace `std::array<PositionInfo, ME_MAX_TICKERS>` with `std::array<PositionInfo, sizeof...(Symbols)>`.
  - Add `find(SymbolName)` and `template<SymbolName S> info()`.

- [ ] **Step 2: Tighten initialization**
  - Ensure `PositionInfo::open_vwap_` is value-initialized (`{}`) to avoid undefined reads.
  - In `PositionKeeper` constructor, set each `PositionInfo.exchange` and `.symbol` based on `E` and the symbol pack.

- [ ] **Step 3: Adjust PositionKeeper APIs**
  - `addFill(const MEClientResponse*, SymbolName)`
  - `updatePnlByBBO(SymbolName, const BBO*)`
  - `getPositionInfo(SymbolName)` and keep `toString()`.


### Task 2: Add symbol-pack ExchangeProcessor

**Files:**
- Modify: `hft/trading/strategy/exchange_processor.h`

- [ ] **Step 1: Template ExchangeProcessor on symbol pack**
  - Change to `template<ExchangeName E, SymbolName... Symbols> class ExchangeProcessor`.
  - Remove `OrderBookVariant` hard-code and `std::unordered_map`.
  - Store `std::tuple<OrderBook<E, Symbols>...> orderbooks_`.

- [ ] **Step 2: Add low-overhead runtime routing helpers**
  - `static constexpr std::array<SymbolName, N> kSymbols{Symbols...}`.
  - `tryGetSymbolIndex(SymbolName, size_t&)`.
  - `tryMapTickerIdToSymbol(TickerId, SymbolName&)`.
  - Add a `tupleVisitByIndex` helper for runtime index -> orderbook reference.

- [ ] **Step 3: Wire PositionKeeper**
  - Replace member with `PositionKeeper<E, Symbols...> position_keeper_`.
  - In `run()`, route incoming `PriceLevel/Trade` updates by symbol to correct orderbook and then call `position_keeper_.updatePnlByBBO(symbol, book.getBBO())`.


### Task 3: Update ExchangeManager + main wiring

**Files:**
- Modify: `hft/trading/strategy/exchange_processor.h` (ExchangeManager section)
- Modify: `hft/trading/trading_main.cpp`

- [ ] **Step 1: Make ExchangeManager symbol-pack aware**
  - Introduce `template<SymbolName... Symbols> class ExchangeManagerT` and alias `using ExchangeManager = ExchangeManagerT<SymbolName::BTC_USDT, SymbolName::BTC_USDT_SWAP>;`.
  - Update `ProcessorVariant` to hold pointers to `ExchangeProcessor<ExchangeName::X, Symbols...>`.

- [ ] **Step 2: Update trading_main instantiations**
  - Instantiate processors as `ExchangeProcessor<ExchangeName::OKX, BTC_USDT, BTC_USDT_SWAP>` etc.


### Task 4: Verify build

**Files:**
- None

- [ ] **Step 1: Build**
  - Run `./hft/build.sh` (or CMake build directory command).
  - Fix any compile errors caused by signature changes.

- [ ] **Step 2: Smoke run (optional)**
  - Run `./hft/run.sh` if your environment supports it.
