# Building Low Latency Applications with C++


## 20 Oct 2025

**Finish**
1. Deploy 3rd libraries and build them in build.sh
2. Revise CMakeLists.txt
   - the outer one finds packages and set CMAKE configurations
   - the inner one links the installed 3rd libraries to trading, compiles the whole trading functions into a static library libtrading.a
   - the trading_main.cpp utilizes libtrading.a, then compiled as the target executable trading

**Pending**
1. Succeed to compile with Folly, but unable to link it during runtime

**Start**
1. Define interface file of market update and order
2. Deploy a boost websocket client




