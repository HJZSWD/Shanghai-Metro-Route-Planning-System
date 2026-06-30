# 上海地铁线路查询系统

东华大学 · 数据结构课程设计项目

```
┌─────────────────────────────────────┐
│  frontend/web/  ← 立即打开浏览器    │
│  backend/src/   ← C++ 算法源码      │
│  data/          ← CSV 数据集         │
└─────────────────────────────────────┘
```

## 快速启动 — 推荐方式

### 🚀 一键启动（推荐）

| 操作 | 说明 |
|------|------|
| **双击 `launch.bat`** | 自动启动前端 + 后台服务 + 打开浏览器 |
| 双击 `launch_hidden.vbs` | 静默启动，零控制台窗口 |

`launch.bat` 会自动完成三步：① 启动 HTTP 服务器 ② 打开浏览器 ③ 隐藏运行 backend-service.exe 后台服务。

### 🔧 手动方式

#### 仅前端（无需编译）

```bash
cd frontend/web
python -m http.server 8080
# 浏览器打开 http://localhost:8080
```

#### 编译 + 运行后端

```bash
g++ backend/src/main.cpp backend/src/station.cpp backend/src/graph.cpp backend/src/pathfinder.cpp backend/src/menu.cpp -o backend-service.exe -std=c++11 -static-libgcc -static-libstdc++ -static -pthread
.\backend-service.exe
```

## 项目结构

```
├── frontend/           ← Web 前端（优先展示）
│   └── web/            ← 单页应用（HTML+JS）
├── backend/            ← C++ 后端
│   ├── src/            ← 源码（模块化组织）
│   └── algo/           ← 算法文档
├── data/               ← CSV 数据集
├── docs/               ← 项目文档
├── launch.bat          ← 一键启动脚本 ★
├── launch_hidden.vbs   ← 静默启动脚本
├── backend-service.exe ← 后端服务（编译输出）
└── README.md
```

## 核心功能

- **路径规划**：Dijkstra 最短时间 / 最少换乘（双关键字排序）
- **TOP3候选**：Yen's K短路算法（全局最优替代路径）
- **同名换乘站折叠**：连续同名站中间过渡站自动折叠（如世纪大道9→2跳过6/4号线中间站）
- **换乘站独立显示**：同名多线路站点各自独立展示，用户精确选择
- **站点管理**：CSV批量更新 / 开关 / 查询 / 恢复初始
- **可视化前端**：响应式Web界面，4种查询模式，毫秒级响应

## 更新记录

### 2026-06-30 — Bug修复与优化

| 修复项 | 说明 |
|--------|------|
| **Bug 1：目的地同名站终止** | Dijkstra 到达与终点同名的任意站即终止，避免多余站内换乘 |
| **Bug 2：连续同名换乘合并** | 世纪大道等换乘站多条线路间仅计1次换乘 |
| **同名中间站折叠** | 路径中跳过同名中间过渡站（如942→632→417→249 → 942→249） |
| **站点选择反向优化** | 同名不同线站点各自独立显示，用户精确选择所需线路 |
| **前端同步** | 前端JavaScript同步所有后端Bug修复，算法同构 |
| **TOP3路径去重** | Yen's算法使用完整路径（不折叠）计算阻断边，消除重复路径 |
| **"?"时间显示修复** | 折叠后无直接边的站间显示默认换乘耗时5min |
