# OKX Agent Skills 在 HFT 系统中的应用手册

> 针对本仓库 C++20 低延迟加密货币 HFT 系统（`ExchangeProcessor` + `EventBus` + `StrategyManager` + `AsyncWebsocketClient`）

## 概述

OKX agent skills 是一组基于 Markdown 的指令文件，告诉 AI agent 如何通过 `okx` CLI 操作 OKX 交易所。本手册记录每个 skill 如何映射到 HFT 系统的开发、测试、监控和运营工作流。

e.g.,
/okx-cex-market
这个skill会拆解输入，调用对应okx cli指令，例如okx market orderbook BTC-USDT --sz 5


---

## 目录

1. [Skill 与 HFT 模块映射](#skill-与-hft-模块映射)
2. [日常开发工作流](#日常开发工作流)
3. [回测工作流](#回测工作流)
4. [生产监控工作流](#生产监控工作流)
5. [指令执行方式](#指令执行方式)
6. [从 C++ HFT 系统自动调用 OKX CLI](#从-c-hft-系统自动调用-okx-cli)
7. [重要约束](#重要约束)

---

## Skill 与 HFT 模块映射

### 1. `okx-cex-market` — 市场数据验证

**在 HFT 中的角色**：验证 WebSocket 数据流准确性、回测数据源、技术指标交叉核对

```bash
# 1.1 Order book 快照 — 与内存中的 OrderBook<E,S> 对比
okx market orderbook BTC-USDT --sz 5
okx market orderbook BTC-USDT-SWAP --sz 10

# 1.2 K 线数据 — 用于回测
okx market candles BTC-USDT --bar 1m --limit 1000
okx market candles BTC-USDT-SWAP --bar 1m --limit 1000 --after <timestamp_ms>

# 1.3 技术指标 — 与 FeatureEngine 计算结果交叉核对
okx market indicator rsi BTC-USDT --bar 1H
okx market indicator macd BTC-USDT --bar 1H --params 12,26,9
okx market indicator bb BTC-USDT --bar 1H --params 20,2

# 1.4 资金费率 — 用于永续合约策略校准
okx market funding-rate BTC-USDT-SWAP --history --limit 100

# 1.5 最近成交 — 验证 Trade 消息解析逻辑
okx market trades BTC-USDT --limit 20

# 1.6 合约信息 — 验证合约规格（ctVal, tickSz, lotSz）
okx market instruments --instType SWAP --instId BTC-USDT-SWAP
```

**HFT 集成点**：
- `OrderBook<E,S>`：对比 WebSocket 推 best bid/ask 与 REST 快照
- `FeatureEngine`：验证 RSI/MACD/BB 计算与 OKX 官方值是否一致
- `ExchangeProcessor`：确认 `PriceLevel` 和 `Trade` 结构体字段与 OKX API 格式匹配
- `PositionKeeper`：使用资金费率验证 P&L 计算中的持仓成本

---

### 2. `okx-cex-trade` — 订单执行验证

**在 HFT 中的角色**：验证订单格式、测试订单管理器逻辑、验证风控规则

```bash
# 2.1 测试下单（永远先用 --demo）
okx --demo spot place --instId BTC-USDT --side buy --ordType limit --sz 0.01 --px 95000
okx --demo swap place --instId BTC-USDT-SWAP --side buy --ordType market --sz 1 --tdMode cross --posSide long

# 2.2 冰山 / TWAP 订单 — 与 OrderManager 逻辑对比
okx --demo spot place --instId BTC-USDT --side buy --ordType iceberg --sz 1 --px 95000
okx --demo spot algo place --instId BTC-USDT --side sell --ordType oco --sz 0.01 \
  --tpTriggerPx 105000 --tpOrdPx=-1 --slTriggerPx 88000 --slOrdPx=-1

# 2.3 查看未成交订单
okx --demo spot orders
okx --demo swap orders
okx --demo swap algo orders

# 2.4 撤单
okx --demo spot cancel --instId BTC-USDT --ordId <ordId>

# 2.5 查看成交记录
okx --demo spot fills --limit 20
okx --demo swap fills --limit 20
```

**HFT 集成点**：
- `OrderManager`：验证订单参数映射（side, sz, px, tdMode, posSide）
- `RiskManager`：测试最大下单量、最大持仓、最大亏损约束
- `TradeEngine`：验证成交回包解析和持仓更新逻辑
- `OMOrder`：确认订单生命周期状态机与 OKX 行为一致

---

### 3. `okx-cex-portfolio` — 账户与持仓核对

**在 HFT 中的角色**：验证 PositionKeeper P&L、监控保证金、对账余额

```bash
# 3.1 交易账户余额
okx account balance USDT
okx account balance BTC

# 3.2 资金账户余额（用于转账）
okx account asset-balance USDT

# 3.3 当前持仓 — 与 PositionKeeper 对比
okx account positions --instType SWAP
okx account positions --instId BTC-USDT-SWAP

# 3.4 已平仓 / 已实现盈亏
okx account positions-history --instType SWAP --limit 20

# 3.5 手续费等级 — 用于净盈亏计算
okx account fees --instType SWAP

# 3.6 最大下单量 — 验证 RiskManager 限制
okx account max-size --instId BTC-USDT-SWAP --tdMode cross

# 3.7 账户配置 — 检查持仓模式（net vs hedge）
okx account config

# 3.8 资金划转（资金账户 → 交易账户，给 bot 注资）
okx account transfer --ccy USDT --amt 1000 --from 6 --to 18
```

**HFT 集成点**：
- `PositionKeeper::updatePnlByBBO()`：与 OKX 核对未实现盈亏
- `PositionInfo::writeSnapshot()`：验证 seqlock 快照值与交易所一致
- `RiskManager`：用 `max-size` 和 `max-avail-size` 验证内部限制
- `StrategyManager`：用账户配置（posMode）判断是否为对冲模式

---

### 4. `okx-cex-bot` — 策略基准对比

**在 HFT 中的角色**：将自定义策略与 OKX 服务端机器人对比

```bash
# 4.1 创建网格机器人作为性能基准
okx bot grid create --instId BTC-USDT --algoOrdType grid \
  --minPx 90000 --maxPx 100000 --gridNum 10 --quoteSz 1000

# 4.2 创建 DCA 机器人
okx bot dca create --instId BTC-USDT --side buy --amount 100 --frequency daily

# 4.3 监控运行中的机器人
okx bot grid orders --status active
okx bot dca orders --status active

# 4.4 停止机器人
okx bot grid stop --algoId <algoId>
okx bot dca stop --algoId <algoId>
```

**HFT 集成点**：
- `SimpleMM`（做市策略）：将你的自定义 MM 逻辑与 OKX 网格机器人性能对比
- `CrossExArb`（跨所套利）：用 bot P&L 作为基准线；你的 HFT 系统应大幅 outperform
- `OrderManager`：学习 OKX bot 的下单模式（网格间距、再平衡）
- `FeatureEngine`：用 bot 表现校准信号阈值

---

### 5. `okx-sentiment-tracker` + `okx-cex-smartmoney` — 情绪与聪明钱信号

**在 HFT 中的角色**：为策略决策矩阵增加情绪/社交因子

```bash
# 5.1 币种情绪
okx news coin-sentiment --coins BTC
okx news coin-sentiment --coins BTC,ETH,SOL

# 5.2 情绪排名（热门币种）
okx news sentiment-rank --limit 10

# 5.3 最新资讯
okx news latest --limit 10
okx news important --limit 5

# 5.4 聪明钱共识信号
okx smartmoney signal-overview-by-filter --topInstruments 10 --period 7

# 5.5 顶尖交易员持仓
okx smartmoney traders-by-filter --period 7 --limit 10
okx smartmoney trader-positions --authorId <id> --instId BTC-USDT-SWAP

# 5.6 经济日历（宏观事件）
okx news economic-calendar --limit 20
```

**HFT 集成点**：
- `StrategyManager`：在分发逻辑中加入情绪分数作为辅助信号
- `SimpleMM`：情绪极度恐惧时（`sentiment-rank` < 阈值）减少报价量
- `CrossExArb`：用聪明钱共识（`signal-overview-by-filter`）作为方向性偏置
- `RiskManager`：高影响宏观事件（`economic-calendar`）期间暂停新订单
- `ExchangeProcessor`：记录情绪变化与 price level 更新，用于盘后分析

---

## 日常开发工作流

### 开盘前启动

```bash
# 1. 检查认证状态
okx auth status --json

# 2. 确认账户余额
okx account balance USDT
okx account balance BTC

# 3. 快速数据健康检查
okx market ticker BTC-USDT
okx market ticker BTC-USDT-SWAP
okx market orderbook BTC-USDT --sz 5

# 4. 查看市场情绪
okx news coin-sentiment --coins BTC
okx smartmoney signal-overview-by-filter --topInstruments 5 --period 7
```

### 交易中并行监控

```bash
# 终端 1：监控持仓
watch -n 5 'okx account positions --instType SWAP'

# 终端 2：监控情绪
watch -n 30 'okx news sentiment-rank --limit 5'

# 终端 3：监控聪明钱
watch -n 60 'okx smartmoney signal-overview-by-filter --instCcyList BTC --period 7'
```

### 收盘后复盘

```bash
# 1. 当日成交
okx account positions-history --instType SWAP --limit 50
okx account bills --limit 50

# 2. 盈亏分析
okx account fees --instType SWAP

# 3. 下载 K 线用于明日回测
okx market candles BTC-USDT --bar 1m --limit 1440  # 1 天 1 分钟 K 线
okx market candles BTC-USDT-SWAP --bar 1m --limit 1440

# 4. 资金费率趋势
okx market funding-rate BTC-USDT-SWAP --history --limit 24
```

---

## 回测工作流

### 第 1 步：下载历史数据

```bash
# 1 分钟 K 线，1 周（7 * 1440 = 10080 根）
# 用 --after 向前翻页
timestamp=$(date -d '7 days ago' +%s)000
okx market candles BTC-USDT --bar 1m --limit 1000 --after $timestamp > btc_spot_candles.json
okx market candles BTC-USDT-SWAP --bar 1m --limit 1000 --after $timestamp > btc_swap_candles.json

# Order book 快照（用于市场冲击建模）
for i in {1..100}; do
  okx market orderbook BTC-USDT --sz 20 >> ob_snapshots.json
done

# 资金费率历史
okx market funding-rate BTC-USDT-SWAP --history --limit 500 > funding_history.json
```

### 第 2 步：转换为 HFT 系统格式

你的 C++ 系统使用 `PriceLevel` 和 `Trade` 结构体。写一个小的 Python/Node 脚本把 OKX JSON 转为你系统期望的格式（如 binary 或 CSV 用于 replay）。

### 第 3 步：回放与对比

```bash
# 运行 HFT 系统回放模式（如支持）
./trading_main --replay btc_replay_data.bin

# 同时查询同一时段 OKX 指标
okx market indicator rsi BTC-USDT --bar 1m --limit 1000 > okx_rsi.json
# 与你的 FeatureEngine 输出对比
```

---

## 生产监控工作流

### 健康检查（每 30 秒从监控线程执行）

```bash
# 数据新鲜度检查
okx market ticker BTC-USDT --json | jq '.data[0].ts'
# 与系统 last_update_time 对比 — 差距 > 5s = 数据 stale

# Order book 合理性
okx market orderbook BTC-USDT --sz 1 --json | jq '.data.bids[0][0], .data.asks[0][0]'
# 与内存 BBO 对比

# 持仓对账
okx account positions --instId BTC-USDT-SWAP --json | jq '.data[0].pos, .data[0].upl'
# 与 PositionKeeper 快照对比
```

### 告警触发条件

| 条件 | OKX CLI 检查 | HFT 系统动作 |
|---|---|---|
| WebSocket 延迟（>5s 无更新） | `market ticker --json` 时间戳差 | 重启 `AsyncWebsocketClient` |
| BBO 不匹配（价格偏差 >1%） | `market orderbook --sz 1` vs 内存 | 记录错误，标记复核 |
| 持仓漂移（>1 张合约差异） | `account positions` vs `PositionKeeper` | REST 对账，暂停新单 |
| 保证金 < 20% | `account balance` 权益 / 维持保证金 | 减仓 |
| 情绪剧烈变化 | `news sentiment-rank` 变化 > 50% | 减少报价量，拉宽 spread |

---

## 指令执行方式

> TL;DR：**需要 Node.js 16+ 的命令（例如 `okx ...`）建议先 `source ./initOkxCli.sh`**，保证当前 shell 使用的是 nvm 的 Node 版本，并且 `okx` 已安装。

### 方式 A：OpenCode 的 Bash 工具里执行（推荐）

在每次要用 OKX CLI 前，先执行：

```bash
source ./initOkxCli.sh
```

然后再执行你的命令：

```bash
okx market orderbook BTC-USDT --sz 5
```

### 方式 B：SSH / 本地终端执行

同样先 `source` 初始化脚本：

```bash
cd /path/to/my_HFT
source ./initOkxCli.sh
okx market orderbook BTC-USDT --sz 5
```

### 方式 C：让 HFT 自动化对账（可行，但不要在撮合热路径调用）

你可以把 `okx` CLI 当作“外部真值来源”，定期拉取 REST 快照（orderbook / positions / balance），与内存数据对比：

**推荐架构**：单独的低频监控线程/进程（比如每 1s/5s）执行 `okx ...`，把结果写入日志/共享内存/IPC，然后异步对比。

> 不建议在极低延迟策略主线程里直接 `system()`/`popen()` 执行 `okx`：进程创建、IO、JSON 解析都会引入抖动。

### 方式一：在 OpenCode / AI Agent 中执行（推荐用于开发调试）

所有 `okx` CLI 命令都可以在 OpenCode 的 Bash 工具中直接执行。

**适用场景**：
- 开发阶段快速查询市场数据、验证 API 连通性
- 手动下单测试（始终先用 `--demo`）
- 监控持仓和账户状态
- 下载历史数据用于回测

**示例**：
```bash
# 在 OpenCode 的 Bash 工具中执行
okx market ticker BTC-USDT
okx --demo spot orders
okx account balance USDT
```

**注意事项**：
- OpenCode 执行环境需先激活 Node.js 18（见下方环境准备）
- 认证信息（API Key / OAuth）存储在 `~/.okx/config.toml`，不暴露于对话
- 所有写操作（下单、转账）先用 `--demo` 验证

---

### 方式二：在系统终端中手动执行（推荐用于运营监控）

登录服务器后，在 shell 中直接运行 `okx` 命令。

**适用场景**：
- 7×24 交易期间的人工巡检
- 紧急排查（数据异常、持仓对账）
- 盘后数据下载和分析

**示例**：
```bash
# SSH 登录后
export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && . "$NVM_DIR/nvm.sh"
nvm use 18

# 然后执行任意 okx 命令
okx market orderbook BTC-USDT --sz 5
okx account positions --instType SWAP
```

---

### 方式三：从 C++ HFT 系统自动化调用（生产监控）

你的 C++ 系统**可以**在后台监控线程中自动调用 OKX CLI，与内存数据对比。具体方案见下一章。

---

## 从 C++ HFT 系统自动调用 OKX CLI

### 方案 A：Shell-out（最简单，延迟较高）

在后台监控线程中使用 `std::popen()` 或 `std::system()`：

```cpp
// 在监控线程中执行（绝对不能在热路径！）
void MonitorThread::runHealthCheck() {
    FILE* pipe = popen("okx market orderbook BTC-USDT --sz 1 --json", "r");
    if (!pipe) return;
    
    char buffer[4096];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    pclose(pipe);
    
    // 用 nlohmann/json 解析
    auto json = nlohmann::json::parse(output);
    double okx_bid = std::stod(json["data"]["bids"][0][0].get<std::string>());
    double okx_ask = std::stod(json["data"]["asks"][0][0].get<std::string>());
    
    // 与内存 OrderBook 对比
    auto bbo = orderbook_.getBBO();
    if (std::abs(okx_bid - bbo->bid_price_) > threshold_) {
        ASN_ERROR(logger_, "BBO 不匹配: OKX bid=" + std::to_string(okx_bid) +
                  " 内存 bid=" + std::to_string(bbo->bid_price_));
    }
}
```

**优点**：实现简单，无需额外依赖  
**缺点**：每次调用延迟 ~50-200ms（进程创建 + JSON 解析），绝对不能用于热路径

**适用场景**：
- 每 5-30 秒运行一次的后台健康检查线程
- 与 `ExchangeProcessor::run()` 独立运行，不阻塞热路径

---

### 方案 B：直接调用 OKX REST API（推荐用于生产自动化）

不经过 `okx` CLI（Node.js 额外开销），直接用 C++ 调用 OKX REST API。你的系统已经有 `AsyncWebsocketClient`（Boost.Beast + SSL），只需增加 HTTP REST 客户端：

```cpp
// 复用现有 Boost.Beast HTTP 基础设施
// OKX REST API 文档：https://www.okx.com/docs-v5/en/#rest-api
// 基础 URL：https://www.okx.com（或 https://aws.okx.com 延迟更低）

// 示例：GET /api/v5/market/ticker?instId=BTC-USDT
// 示例：GET /api/v5/account/positions
// 认证：HMAC SHA256 签名（使用 config/.env 中的 API key）
```

**优点**：
- 延迟仅 ~5-20ms
- 无进程创建开销
- 与现有 Boost.Beast 代码集成

**缺点**：
- 需要手写 C++ REST 客户端封装
- 需要处理认证（HMAC SHA256 签名生成）

**适用场景**：
- 高频对账（每 1-5 秒一次）
- 与内存 `OrderBook`、`PositionKeeper` 实时比对
- 延迟敏感的监控逻辑

**实施建议**：
1. 在 `common/` 下新增 `okx_rest_client.h`，封装 Boost.Beast HTTP GET/POST
2. 使用 `config/.env` 中的 `OKX_API_KEY`、`OKX_SECRETKEY`、`OKX_PASSPHRASE` 生成签名
3. 在独立线程中运行，通过 `EventBus` 发布对账结果，不阻塞 `ExchangeProcessor`

---

### 方案 C：独立 Python/Node 监控进程（松耦合）

运行一个轻量 Python/Node 脚本与 C++ HFT 系统并行：

```python
# monitor.py — 与主进程并行，通过 Unix socket 或共享内存通信
import subprocess
import json
import time

while True:
    result = subprocess.run(
        ["okx", "market", "orderbook", "BTC-USDT", "--sz", "1", "--json"],
        capture_output=True, text=True
    )
    data = json.loads(result.stdout)
    # 写入 Unix socket 或共享内存供 C++ 进程读取
    time.sleep(5)
```

**优点**：
- 与 C++ 进程解耦，监控进程可独立重启
- 不增加 C++ 代码复杂度
- 可用 Python 丰富的 JSON/数据处理库

**缺点**：
- 需要进程间通信（IPC）机制
- 额外的系统资源占用

**适用场景**：
- 复杂的监控逻辑（如多指标聚合、告警通知）
- 需要独立运维的团队场景

---

### 三种方案对比

| 方案 | 实现复杂度 | 单次延迟 | 资源开销 | 推荐场景 |
|---|---|---|---|---|
| A. Shell-out | 低 | ~50-200ms | 中（每次创建进程）| 简单健康检查，开发调试 |
| B. 直接 REST API | 中 | ~5-20ms | 低（纯 C++）| 生产高频对账，延迟敏感 |
| C. 独立进程 | 中 | ~50-200ms | 高（额外进程）| 复杂监控，团队独立运维 |

---

## 重要约束

### 速率限制

| 接口类型 | 限制 |
|---|---|
| 市场数据（公开）| 20 请求 / 2秒 / IP |
| 交易（私有）| 60 请求 / 2秒 / UID |
| 账户（私有）| 10 请求 / 2秒 / UID |

**规则**：绝对不要在热路径（`ExchangeProcessor::run()` 循环）中调用 OKX CLI。仅在后台监控线程中以 >=5 秒间隔调用。

### 认证

- **API Key**：存储在 `~/.okx/config.toml`。用 `--profile <name>` 选择实盘/模拟。
- **OAuth**：用 `okx auth login --manual` 交互式配置。会话自动刷新。
- **绝对不要**把 API key 硬编码在 C++ 源码中或提交到 git。

### 执行环境

- source ./initOkxCli.sh
- e.g. okx market orderbook BTC-USDT --sz 5
  Asks (price / size):
           78071.2  0.01782244
           78069.3  0.015379
           78068.8  0.48
           78068.7  0.14328
           78066.5  2.60932963
  Bids (price / size):
           78066.4  0.20655
           78066.3  0.58826046
           78065.8  0.05085046
           78065.7  0.07923
             78064  0.015379

### 数据对比注意事项

- **时间戳偏差**：OKX REST API 可能比 WebSocket 滞后 100-500ms。对比时允许 ±1s 误差。
- **价格精度**：OKX 返回字符串价格（如 `"95000.5"`）。你的 `PriceLevel` 用 `double`。注意浮点精度漂移。
- **合约面值**：永续合约 `sz` 单位是张（contracts），不是币。对比持仓前务必通过 `market instruments` 确认 `ctVal`。

---

## 速查：最常用命令

```bash
# --- 市场数据 ---
okx market ticker BTC-USDT
okx market orderbook BTC-USDT --sz 5
okx market candles BTC-USDT --bar 1m --limit 100
okx market indicator rsi BTC-USDT --bar 1H

# --- 交易（模拟盘）---
okx --demo spot place --instId BTC-USDT --side buy --ordType market --sz 0.01
okx --demo swap place --instId BTC-USDT-SWAP --side buy --ordType market --sz 1 --tdMode cross --posSide long
okx --demo spot orders

# --- 账户 ---
okx account balance USDT
okx account positions --instType SWAP
okx account positions-history --limit 20
okx account fees --instType SWAP

# --- 情绪 ---
okx news coin-sentiment --coins BTC
okx news sentiment-rank --limit 5
okx smartmoney signal-overview-by-filter --topInstruments 5 --period 7

# --- 机器人（基准）---
okx bot grid create --instId BTC-USDT --algoOrdType grid --minPx 90000 --maxPx 100000 --gridNum 10 --quoteSz 1000
okx bot grid orders --status active
```

---

*为 my_HFT C++20 HFT 系统编写。最后更新：2026-05-21*
