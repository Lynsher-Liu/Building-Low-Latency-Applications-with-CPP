# Exchange Processor Symbol Pack Design (2026-05-13)

## Goal
Define compile-time symbol packs for `ExchangeProcessor` and `PositionKeeper` to remove runtime maps/variants on the hot path, while keeping a runtime routing path by `SymbolName`. Add a small constexpr helper for `TickerId -> SymbolName` inside `ExchangeProcessor`.

## Context
- Each `ExchangeProcessor` owns a single `PositionKeeper` for that exchange.
- Each exchange currently uses symbols: `BTC_USDT`, `BTC_USDT_SWAP`.
- Order books are already templated by exchange + symbol.

## Proposed Interfaces
### PositionKeeper
- Template: `template<ExchangeName E, SymbolName... Symbols>`.
- Storage: `std::array<PositionInfo, sizeof...(Symbols)> positions_`.
- Runtime lookup: `PositionInfo* find(SymbolName symbol) noexcept` using constexpr index over the symbol pack.
- Compile-time lookup: `template<SymbolName S> PositionInfo& info() noexcept`.

### ExchangeProcessor
- Template: `template<ExchangeName E, SymbolName... Symbols>`.
- Order book storage: fixed array or tuple of `OrderBook<E, Symbols>`.
- Runtime routing: `symbolIndex(symbol)` returns index or `npos`.
- `OrderBookVariant` removed; no `unordered_map`.
- `PositionKeeper` member becomes `PositionKeeper<E, Symbols...>`.

### Runtime Symbol Routing
- Provide `OrderBook* findOrderBook(SymbolName symbol) noexcept` which uses the symbol pack index.
- If symbol not in pack, log and drop update (no exceptions on hot path).

### TickerId -> SymbolName
- Add a small constexpr mapping helper inside `ExchangeProcessor`:
  - `static constexpr SymbolName kSymbols[] = { Symbols... }`.
  - `static constexpr SymbolName tickerIdToSymbol(TickerId id) noexcept`.
  - If `id` is out of range, return a sentinel or log and drop.

## Data Flow
- Market updates (PriceLevel/Trade) route by `SymbolName` to the correct `OrderBook` and then to `PositionKeeper::updatePnlByBBO`.
- Fills (`MEClientResponse`) route by `TickerId` -> `SymbolName` using the constexpr helper, then to `PositionKeeper::addFill`.

## Error Handling
- For unknown symbols or out-of-range ticker ids: log with exchange and drop the update.
- No exceptions in the hot path.

## Testing/Verification
- Build the trading target after refactor.
- Add a lightweight unit check (if available) or a small runtime assert in debug builds to ensure symbol pack size matches expected ticker ids.

## Migration Notes
- Update `trading_main.cpp` instantiations to include symbol pack for each exchange.
- Update `ExchangeManager` type aliases if needed to match the new template signatures.
- Remove any remaining use of `OrderBookVariant` or `unordered_map` in `ExchangeProcessor`.
