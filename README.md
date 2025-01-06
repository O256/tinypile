# tinypile

> 该仓库clone自 [fgenesis/tinypile](https://github.com/fgenesis/tinypile)\
> 对其中的jps进行注释，用于了解jps的实现。

各种微型（单文件或双文件）库的集合。

- [x] 跨平台 C/C++。
- [x] 公共领域。
- [X] 独立自足。
- [x] 无异常，无 RTTI，完全控制内存分配。
- [x] 无构建系统，无麻烦。

|项目|文件|语言|概述|状态|
|:------|:-------|:-----|:-----|:-----|
|LuaAlloc|[.c](luaalloc.c) + [.h](luaalloc.h)|C99|Lua 小块分配器|稳定版|
|JPS v2|[.hh](jps.hh)|C++98|2D 寻路：A*、跳点搜索|实验版，需要测试|

由于历史原因，我的其他微型库位于独立仓库中：

|项目|语言|概述|
|:------|:-------|:-----|
|[JPS](https://github.com/fgenesis/jps)（旧版）|C++03|跳点搜索（2D 寻路）|
|[minihttp](https://github.com/fgenesis/minihttp)|C++03|HTTP(S) 客户端库|




## 灵感来源：

- 著名的 [nothings/stb](https://github.com/nothings/stb/)
- [r-lyeh/tinybits](https://github.com/r-lyeh/tinybits)
