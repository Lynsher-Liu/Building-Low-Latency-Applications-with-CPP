# 本项目的模板化设计说明

本文记录 HFT 项目中已经落地的模板化设计，重点说明哪些部分适合模板化、模板参数如何组织、运行时如何从业务对象映射到编译期实体，以及这样做带来的收益和约束。

## 1. 设计目标

本项目的核心目标是低延迟、低分配、低分支开销。因此，模板化并不是为了“追求泛型而泛型”，而是为了把稳定不变的维度提前固化到编译期：

- 交易所维度：`ExchangeName`
- 标的维度：`SymbolName`
- 订单簿维度：`OrderBook<E, S>`
- 持仓维度：`PositionKeeper<E, Symbols...>`

通过把这些维度前移到编译期，可以减少热路径中的哈希查找、`unordered_map`、`variant` 访问和间接分发。

## 2. 适合模板化的部分

### 2.1 `OrderBook<ExchangeName, SymbolName>`

订单簿是典型的“交易所 + 标的”二维对象，不同交易所、不同标的的参数通常不同，例如：

- 深度档位不同
- tick size 不同
- 订单簿更新逻辑可能存在细微差异

把这些信息做成模板参数后，编译器可以在实例化阶段生成专用代码。热路径里不需要再通过运行时配置去判断“当前是哪一个交易所、哪一个币对”。

### 2.2 `PositionKeeper<ExchangeName E, SymbolName... Symbols>`

持仓管理在本项目里已经改成“按交易所拥有一组标的”的形式：

- 一个 `ExchangeProcessor` 对应一个 `PositionKeeper`
- 一个 `PositionKeeper` 内部持有该交易所下全部标的的 `PositionInfo`
- 标的集合在编译期固定为参数包 `Symbols...`

这比传统的 `unordered_map<SymbolName, PositionInfo>` 更适合 HFT 场景，因为标的集合通常很小且固定，线性扫描一个 `std::array` 往往比哈希更快、更稳定。

### 2.3 `ExchangeProcessor<ExchangeName E, SymbolName... Symbols>`

`ExchangeProcessor` 是目前模板化设计的核心枢纽。它把：

- 订单簿实例
- 持仓管理器
- 消息队列
- 线程运行逻辑

绑定到同一个交易所和同一个标的集合上。

## 3. 当前实现结构

### 3.1 `PositionKeeper`

当前实现位于 `hft/trading/strategy/position_keeper.h`。

它的关键设计点是：

- `PositionInfo` 保持非模板化，专注于单标的状态和快照发布
- `PositionKeeper<E, Symbols...>` 用 `std::array<PositionInfo, N>` 存储所有标的
- 通过 `SymbolName` 在线性小数组中查找索引
- 通过 `PositionInfo::writeSnapshot()` / `readSnapshot()` 提供单写多读快照语义

这种拆分方式有两个好处：

- 模板只放在“结构固定”的外层，避免把每个细粒度字段都模板化
- 单标的状态逻辑保持普通类，代码更容易维护，也不会因为过度模板化导致编译时间和二进制体积膨胀

### 3.2 `ExchangeProcessor`

当前实现位于 `hft/trading/strategy/exchange_processor.h`。

它使用：

- `std::tuple<OrderBook<E, Symbols>...>` 保存所有订单簿
- `PositionKeeper<E, Symbols...>` 保存对应持仓
- `kSymbols` 保存编译期标的列表
- `symbolIndex()` 在线性扫描中完成 `SymbolName -> index` 映射
- `visitOrderbook()` / `visitOrderbookImpl()` 通过索引访问 tuple 中的具体订单簿实例

这意味着外部消息进入 `ExchangeProcessor` 后，仍然可以按运行时 `SymbolName` 路由，但一旦找到索引，后续操作就落回到编译期确定的具体类型上。

## 4. 运行时到编译期的桥接

模板化设计最重要的问题不是“怎么写模板”，而是“怎么把运行时输入安全地映射到编译期实体”。本项目的桥接方式分成两步：

1. 先用 `SymbolName` 或 `TickerId` 定位目标标的
2. 再用编译期标的包里的索引访问对应对象

### 4.1 `SymbolName -> index`

`PositionKeeper` 和 `ExchangeProcessor` 都维护了一个编译期常量数组 `kSymbols`，通过线性扫描找到索引：

- 标的数量很小
- 访问模式高度局部
- 线性扫描比哈希查找更可预测

### 4.2 `TickerId -> SymbolName`

在 `ExchangeProcessor` 中还保留了一个轻量桥接函数，用于把成交回包里的 `TickerId` 映射成 `SymbolName`。

它的前提是：`TickerId` 的顺序与模板参数包中的顺序一致。这个约束使映射可以退化成数组索引访问，避免额外的运行时表结构。

## 5. 为什么 `PositionInfo` 不模板化

`PositionInfo` 是单标的的状态容器，但它本身并不依赖交易所或标的类型来改变内部算法，所以没有必要模板化。

保持它为普通类的原因：

- 成员字段类型是通用类型
- `addFill()`、`updatePnlByBBO()`、`writeSnapshot()` 逻辑不依赖特定交易所
- 继续模板化不会减少热路径开销，反而会增加代码膨胀

这也是当前设计里很重要的一点：只把“结构性维度”模板化，不把“纯算法状态”模板化。

## 6. 快照发布设计

`PositionInfo` 使用类似 seqlock 的方式发布快照：

- 单写线程在进入和退出写入时递增序列号
- 读线程先读序列号，再拷贝快照，再校验序列号是否一致
- 快照字段本身不需要做成原子类型

这种设计适合当前模型：

- 只有一个写者：`ExchangeProcessor` 热线程
- 可能有多个读者：监控、诊断、慢路径策略

因此，原子只放在版本号上，不放在整份快照数据上。

## 7. 旧路径兼容

项目里目前同时保留了两条路径：

- 新的模板化 `ExchangeProcessor` 路径
- 旧的 `TradeEngine` / `MarketOrderBook` 路径

原因是仓库里仍有一部分策略和执行逻辑依赖旧接口。为了让整个项目能持续编译和演进，当前实现保留了兼容层，而不是一次性删除旧代码。

这意味着模板化设计的目标不是“全仓库立刻模板化”，而是先把最值得优化的主路径模板化，再逐步收敛旧接口。

## 8. 这种设计的收益

- 降低热路径上的哈希查找和动态分发
- 更容易把交易所和标的的组合固定成编译期对象
- 订单簿和持仓的关系更清晰，所有权边界更明确
- 适合低延迟场景下的小集合路由和局部访问

## 9. 这种设计的约束

- 标的集合需要在编译期明确
- `TickerId` 与模板参数顺序必须保持一致
- 模板化会增加编译时间和二进制体积
- 仍需要兼容旧路径时，要控制接口重复和重复定义问题

## 10. 相关文件

- `hft/trading/strategy/position_keeper.h`
- `hft/trading/strategy/exchange_processor.h`
- `hft/trading/trading_main.cpp`
- `hft/trading/strategy/market_order_book.h`

## 11. 小结

本项目的模板化设计不是“把所有东西都模板化”，而是把最稳定、最适合编译期决定的维度固定下来：交易所、标的、订单簿实例和持仓容器。这样既保留了 HFT 需要的性能路径，也让代码结构更清晰，后续更容易继续拆分和优化。