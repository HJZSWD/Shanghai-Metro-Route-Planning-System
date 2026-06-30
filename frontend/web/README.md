# 前端 Web 应用

## 启动

```bash
cd frontend/web
python -m http.server 8080
# → http://localhost:8080
```

## 文件说明

| 文件 | 说明 |
|------|------|
| `index.html` | 主页面（HTML+CSS+JS一体化，约40KB） |
| `data.js` | 数据集（从CSV自动生成，524站/1232边） |

## 功能

- 智能搜索：模糊匹配站名，同名换乘站合并为一条
- 四种搜索模式：最快路径、最少换乘、TOP3时间、TOP3换乘
- 路径可视化：时间线、线路标签、换乘标注
- 快速选择：预设常用路线

## 性能

- BinaryHeap 优先队列 O(log n) push/pop
- Int32Array 扁平数组替代 Map，~10x 加速
- Yen's K短路算法 JS 实现，毫秒级响应
