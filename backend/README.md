# C++ 后端

## 目录结构

```
backend/
├── src/           # 源代码（9个文件）
│   ├── main.cpp         # 程序入口
│   ├── station.h/cpp    # 站点数据管理
│   ├── graph.h/cpp      # 有向邻接表图结构
│   ├── pathfinder.h/cpp # Dijkstra + Yen's K短路算法
│   └── menu.h/cpp       # 二级菜单交互
├── algo/          # 算法文档
└── algo/          # 算法文档

## 编译

```bash
# 在项目根目录执行
g++ backend/src/main.cpp backend/src/station.cpp backend/src/graph.cpp backend/src/pathfinder.cpp backend/src/menu.cpp -o backend-service.exe -std=c++11 -static-libgcc -static-libstdc++ -static -pthread
```

## 数据路径

CSV 数据位于项目根目录 `data/`。`main.cpp` 使用 `GetModuleFileNameA` 自动推导 exe 所在位置，
因此 `backend-service.exe` 放在项目根目录或 `backend/` 下均可正确加载数据。
