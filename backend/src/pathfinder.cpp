#include "pathfinder.h"
#include <iostream>
#include <queue>
#include <limits>
#include <algorithm>
#include <set>

/**
 * 构造函数
 * 功能：初始化路径查找器，绑定地铁图和站点管理器引用
 * @param graph       地铁图引用（有向邻接表，提供邻边查询接口）
 * @param stationMgr  站点管理器引用（提供站点信息查询及关闭状态过滤）
 * @note 两个参数均为外部对象引用，生命周期由调用方管理
 */
PathFinder::PathFinder(const MetroGraph &graph, const StationManager &stationMgr)
    : m_graph(graph), m_stationMgr(stationMgr) {}

// ==================== 通用 Dijkstra 双关键字引擎 ====================

/**
 * 通用 Dijkstra 搜索（核心算法引擎）
 * 功能：在带权有向图上执行双关键字最短路径搜索，支持阻断边和禁止节点
 * 使用场景：
 *   1. singleIsTime=true → 时间优先（主=时间，次=换乘）
 *   2. singleIsTime=false → 换乘优先（主=换乘，次=时间）
 *   3. blockedEdges/forbiddenNodes → Yen's K短路偏差路径计算
 * 算法原理：
 *   - 标准 Dijkstra 优先队列（小顶堆），按 (主关键字, 次关键字) 双键排序
 *   - 首次出队的节点即该节点在当前双键排序下的最优值
 *   - 起点采用 "边界值入队、松弛后更新" 的标准流程
 * 换乘判定规则：
 *   - 边 lineName="换乘" 时，必然计 1 次换乘
 *   - 边 lineName 与上一条边不同，且上一条边不是 "换乘" 时，计 1 次换乘
 *   - "换乘" 边后的线路名由紧随其后的非换乘边决定
 * @param startId         起点站点编号
 * @param endId           终点站点编号（基准终点，实际终点可能为同名站）
 * @param blockedEdges    阻断边列表，pair<fromId, toId>；Yen's算法中用于排除已选路径的特定出边
 * @param forbiddenNodes  禁止节点列表；Yen's算法中根路径节点不可在偏差路径中重复出现
 * @param primaryIsTime   true=时间优先主键，(时间,换乘)排序；false=换乘优先主键，(换乘,时间)排序
 * @return PathResult 最优路径结果，不可达返回空（pathStations为空）
 * @note Bug 1 修复：到达与终点同名的任意站即终止，避免站内多余换乘
 */
PathResult PathFinder::dijkstraSearch(int startId, int endId,
                                       const std::vector<std::pair<int,int>> &blockedEdges,
                                       const std::vector<int> &forbiddenNodes,
                                       bool primaryIsTime) const {
    PathResult emptyResult;

    // ---------- 边界校验 ----------
    // 校验起点和终点是否在数据集中存在且处于开放状态
    Station startStation = m_stationMgr.getStationById(startId);
    Station endStation = m_stationMgr.getStationById(endId);
    if (startStation.stationId == -1 || endStation.stationId == -1) {
        return emptyResult;  // 站点不存在
    }
    if (!startStation.isOpen || !endStation.isOpen) {
        return emptyResult;  // 站点关闭
    }
    // 起点等于终点时直接返回（零耗时、零换乘的单站路径）
    if (startId == endId) {
        emptyResult.pathStations.push_back(startId);
        emptyResult.totalTime = 0;
        emptyResult.transferCount = 0;
        return emptyResult;
    }

    // ---------- 禁止节点集预处理 ----------
    // 禁止节点集（Yen's算法中根路径节点禁止在偏差路径中出现）
    std::set<int> forbiddenSet(forbiddenNodes.begin(), forbiddenNodes.end());
    // 偏差路径不能经过根路径节点，但起点（偏差节点）和终点例外
    forbiddenSet.erase(startId);
    forbiddenSet.erase(endId);

    int n = m_graph.getStationCount();
    if (n <= 0) return emptyResult;

    const int INF = std::numeric_limits<int>::max();

    // ---------- 双关键字距离数组 ----------
    // Dijkstra 的核心数据结构，使用双关键字而非单关键字：
    // minPrimary:   主关键字数组（时间 或 换乘次数）
    // minSecondary: 次关键字数组（换乘次数 或 时间）
    // 松弛判定：主关键字更小优先；主关键字相等时次关键字更小优先
    std::vector<int> minPrimary(n, INF);   // 主关键字：时间（primaryIsTime=true）或换乘（false）
    std::vector<int> minSecondary(n, INF); // 次关键字：换乘（primaryIsTime=true）或时间（false）
    std::vector<int> prevStation(n, -1);   // 前驱节点数组，用于最终路径回溯
    std::vector<std::string> prevLine(n, ""); // 到达每个站点时的线路名，用于换乘检测

    // 将阻断边集转为 std::set 以便 O(1) 查询（阻断边数量通常 ≤3）
    std::set<std::pair<int,int>> blockedSet(blockedEdges.begin(), blockedEdges.end());

    // ---------- 优先队列初始化 ----------
    // 优先队列元素：((主关键字, 次关键字), 站点ID)
    // 使用 std::greater 实现小顶堆（最小键优先出队）
    typedef std::pair<int, int> Key;
    typedef std::pair<Key, int> QueueElement;
    std::priority_queue<QueueElement, std::vector<QueueElement>, std::greater<QueueElement>> pq;

    // Bug 1 修复：预查终点站名
    // 如目的地是"世纪大道"，则任意一条线路上的"世纪大道"站点均视为到达
    std::string destStationName = endStation.stationName;
    int effectiveEndId = endId;  // 最终实际用作路径终点的站点ID

    // 起点入队（双键均为 0）
    minPrimary[startId] = 0;
    minSecondary[startId] = 0;
    pq.push({{0, 0}, startId});

    // ---------- 主循环：贪心扩展 ----------
    while (!pq.empty()) {
        QueueElement top = pq.top();
        pq.pop();

        int curPri = top.first.first;
        int curSec = top.first.second;
        int curId  = top.second;

        // 跳过过时状态：该节点已被更优路径更新过，当前出队记录已失效
        if (curPri > minPrimary[curId] || curSec > minSecondary[curId]) {
            continue;
        }

        // 终点优先终止：终点首次出队即当前双键排序下的全局最优
        if (curId == endId) {
            effectiveEndId = endId;
            break;
        }

        // Bug 1 修复：到达与终点同名的任意站即视为到达目的地
        // 例如终点为世纪大道（2号线），但 Dijkstra 先扩展到世纪大道（9号线）
        // 此时直接终止，避免在世纪大道内部进行无意义的站内换乘
        Station curStation = m_stationMgr.getStationById(curId);
        if (curStation.stationId != -1 && curStation.stationName == destStationName) {
            effectiveEndId = curId;
            break;
        }

        // 获取当前节点的所有可用邻接边（已自动过滤关闭站点）
        std::vector<MetroEdge> edges = m_graph.getAvailableEdges(curId, m_stationMgr);

        // ---------- 邻边扩展与松弛 ----------
        for (size_t i = 0; i < edges.size(); ++i) {
            const MetroEdge &edge = edges[i];
            int nextId = edge.endId;

            // 裁剪规则 1：避免回路——不入前驱节点
            if (nextId == prevStation[curId]) continue;

            // 裁剪规则 2：阻断边过滤——Yen's算法中被禁止的出边
            if (blockedSet.count({curId, nextId}) > 0) continue;

            // 裁剪规则 3：禁止节点过滤——Yen's算法中根路径节点不可出现在偏差路径中
            if (forbiddenSet.count(nextId) > 0) continue;

            // ---------- 双关键字增量计算 ----------
            // 根据主键类型组装新的 (主键, 次键) 值
            int newPri, newSec;
            if (primaryIsTime) {
                // 主=时间，次=换乘
                // 时间累加：每条边的耗时直接累加
                newPri = (prevStation[curId] == -1) ? edge.time : (minPrimary[curId] + edge.time);
                // 换乘累加：只有发生线路切换时 +1
                newSec = (prevStation[curId] == -1) ? 0 : (minSecondary[curId] + (
                    // 换乘判定：遇到 "换乘" 边 或 线路名变化时计 1 次换乘
                    (edge.lineName == "换乘" || (
                        prevLine[curId] != "换乘" && prevLine[curId] != edge.lineName
                    )) ? 1 : 0
                ));
            } else {
                // 主=换乘，次=时间
                // 换乘增量计算（只有发生真正的线路切换时才计 1）
                int transferDelta = 0;
                if (prevStation[curId] != -1) {
                    std::string lastLine = prevLine[curId];
                    if (edge.lineName == "换乘") {
                        // 遇到显式的 "换乘" 边（如世纪大道站内的连接边）：计 1 次换乘
                        transferDelta = 1;
                    } else if (lastLine != "换乘" && lastLine != edge.lineName) {
                        // 线路名确实发生变化（且不是从 "换乘" 到实际线路）：计 1 次换乘
                        transferDelta = 1;
                    }
                    // 如果 lastLine == "换乘"，说明上一步已经在换乘过程中，
                    // 此时走到实际线路不计新换乘（换乘动作在上一步已计）
                }
                newPri = minPrimary[curId] + transferDelta;
                newSec = minSecondary[curId] + edge.time;
            }

            // ---------- 双关键字松弛判定 ----------
            // 主键更小 或 (主键相等且次键更小) → 更新
            bool needUpdate = false;
            if (newPri < minPrimary[nextId]) {
                needUpdate = true;
            } else if (newPri == minPrimary[nextId] && newSec < minSecondary[nextId]) {
                needUpdate = true;
            }

            if (needUpdate) {
                minPrimary[nextId]   = newPri;
                minSecondary[nextId] = newSec;
                prevStation[nextId]  = curId;
                prevLine[nextId]     = edge.lineName;
                pq.push({{newPri, newSec}, nextId});
            }
        }
    }

    // 不可达检测：终点前驱为 -1 说明从未被扩展到
    // 排除起点自身（起点前驱也为 -1 但合法）
    if (prevStation[effectiveEndId] == -1 && effectiveEndId != startId) {
        return emptyResult;
    }

    // 从前驱数组回溯构建完整路径
    return reconstructPath(prevStation, prevLine, startId, effectiveEndId);
}

// ==================== 最短时间路径（对外接口） ====================

/**
 * 计算最短时间路径
 * 功能：对外提供的最短时间路径查询接口
 * 策略：时间优先，相同时间选换乘更少的路径（由 dijkstraSearch 双关键字保证）
 * @param startId 起点站点编号
 * @param endId   终点站点编号
 * @return 最短时间路径结果
 * @note 无阻断边、无禁止节点，使用 primaryIsTime=true
 */
PathResult PathFinder::findShortestTimePath(int startId, int endId) const {
    std::vector<std::pair<int,int>> noBlock;
    std::vector<int> noForbid;
    PathResult result = dijkstraSearch(startId, endId, noBlock, noForbid, true);

    if (result.pathStations.empty()) {
        Station startS = m_stationMgr.getStationById(startId);
        Station endS = m_stationMgr.getStationById(endId);
        if (startS.stationId != -1 && endS.stationId != -1) {
            std::cerr << "[错误] 无法从 " << startS.stationName
                      << " 到达 " << endS.stationName << std::endl;
        }
    }
    return result;
}

// ==================== 最少换乘路径（对外接口） ====================

/**
 * 计算最少换乘路径
 * 功能：对外提供的最少换乘路径查询接口
 * 策略：换乘次数优先，相同换乘次数选时间更少的路径（由 dijkstraSearch 双关键字保证）
 * @param startId 起点站点编号
 * @param endId   终点站点编号
 * @return 最少换乘路径结果
 * @note 无阻断边、无禁止节点，使用 primaryIsTime=false
 */
PathResult PathFinder::findMinTransferPath(int startId, int endId) const {
    std::vector<std::pair<int,int>> noBlock;
    std::vector<int> noForbid;
    PathResult result = dijkstraSearch(startId, endId, noBlock, noForbid, false);

    if (result.pathStations.empty()) {
        Station startS = m_stationMgr.getStationById(startId);
        Station endS = m_stationMgr.getStationById(endId);
        if (startS.stationId != -1 && endS.stationId != -1) {
            std::cerr << "[错误] 无法从 " << startS.stationName
                      << " 到达 " << endS.stationName << std::endl;
        }
    }
    return result;
}

// ==================== Yen's K短路算法（核心多路径引擎） ====================

/**
 * 生成前K条候选路径（Yen's K短路算法）
 * 功能：在单条最优路径基础上，通过逐条"偏差"已选路径来发现全局最优替代路径
 * 应用场景：TOP3 时间 / TOP3 换乘 查询
 *
 * 算法原理（Yen's 算法，1971）：
 *   给定起始路径 P1，对每条已选路径 Pk，依次对其每个节点做"偏差"：
 *     1. 取根路径 rootPath = Pk[0..i]（已选路径从起点到偏差节点的前缀）
 *     2. 阻断所有已选路径中以 rootPath 为前缀的路径在偏差节点处走的出边
 *     3. 禁止根路径中除偏差节点外的所有其他节点
 *     4. 以偏差节点为起点运行 Dijkstra 找"偏差路径"
 *     5. rootPath + spurPath[1..] = 一条新的候选路径
 *   所有候选路径放入堆中，每次取最优加入最终结果
 *
 * 与简单边阻断法的区别：
 *   简单边阻断只堵一条出边，漏率高；Yen's 同时维护"前驱一致性"，
 *   确保替代路径与已选路径在结构上真正不同。
 *
 * 去重策略：
 *   每条候选路径在加入堆前与堆内已有路径进行 isSamePath 去重，
 *   避免因折叠/多线路等导致的内容相同的路径重复入堆。
 *
 * @param startId      起点站点编号
 * @param endId        终点站点编号
 * @param primaryIsTime true=时间优先模式, false=换乘优先模式
 * @param maxCount     最多返回路径条数（课程设计要求为 3）
 * @return 去重排序后的候选路径列表（至少包含1条，最多maxCount条）
 * @note 路径以完整形式（不折叠）存储在 finalPaths 中，确保阻断边计算正确
 */
std::vector<PathResult> PathFinder::findTopKPaths(int startId, int endId,
                                                    bool primaryIsTime, int maxCount) const {
    std::vector<PathResult> finalPaths;

    // ---------- 第1步：第一条最优路径 ----------
    // 无阻断边、无禁止节点，直接用 Dijkstra 找全局最优
    std::vector<std::pair<int,int>> noBlock;
    std::vector<int> noForbid;
    PathResult firstPath = dijkstraSearch(startId, endId, noBlock, noForbid, primaryIsTime);
    if (firstPath.pathStations.empty()) {
        return finalPaths;  // 不可达，直接返回空列表
    }
    finalPaths.push_back(firstPath);

    // ---------- 候选路径最小堆 ----------
    // 比较器：按 (主关键字, 次关键字) 升序排列，保证堆顶为当前最优候选
    auto cmp = [primaryIsTime](const PathResult &a, const PathResult &b) {
        if (primaryIsTime) {
            if (a.totalTime != b.totalTime) return a.totalTime > b.totalTime;
            return a.transferCount > b.transferCount;
        } else {
            if (a.transferCount != b.transferCount) return a.transferCount > b.transferCount;
            return a.totalTime > b.totalTime;
        }
    };
    std::vector<PathResult> candidateHeap;

    // ---------- Yen's 算法主循环 ----------
    // 每次从上一路径（finalPaths[k-1]）做偏差，生成新候选路径
    // 循环执行 maxCount-1 次（第一次已由 Dijkstra 产生）
    for (int k = 1; k < maxCount; ++k) {
        const PathResult &prevPath = finalPaths[k - 1];
        const std::vector<int> &prevStations = prevPath.pathStations;

        // 对上一路径的每个节点尝试偏差（最后一个节点是终点，不产生偏差路径）
        for (size_t i = 0; i + 1 < prevStations.size(); ++i) {
            int spurNode = prevStations[i];  // 偏差节点

            // --- 构建根路径 ---
            // 从起点到偏差节点（含）的完整前缀
            std::vector<int> rootPath(prevStations.begin(), prevStations.begin() + i + 1);

            // --- 构建阻断边集 ---
            // 阻断规则：对所有已选路径中与 rootPath 前缀重合的路径，
            // 阻断偏差节点在已选路径中走的那条出边
            std::vector<std::pair<int,int>> blockedEdges;
            for (size_t p = 0; p < finalPaths.size(); ++p) {
                const std::vector<int> &fp = finalPaths[p].pathStations;
                // 路径 p 长度必须至少为 i+1 才能与 rootPath 比较前缀
                if (fp.size() <= i) continue;
                // 检查路径 p 的前 i+1 站是否与 rootPath 完全一致
                bool rootMatches = true;
                for (size_t t = 0; t <= i; ++t) {
                    if (fp[t] != rootPath[t]) {
                        rootMatches = false;
                        break;
                    }
                }
                if (rootMatches && fp.size() > i + 1) {
                    // 阻断偏差节点在路径 p 中走的出边 → fp[i] → fp[i+1]
                    blockedEdges.push_back({fp[i], fp[i + 1]});
                }
            }

            // --- 构建禁止节点集 ---
            // 根路径中除偏差节点外（偏差节点作为新 Dijkstra 的起点）的所有节点
            // 不可在偏差路径中出现，防止路径回溯走回头路
            std::vector<int> forbiddenNodes(rootPath.begin(), rootPath.end() - 1);

            // --- 计算偏差路径 ---
            // 以 spurNode 为新起点，在阻断边和禁止节点的约束下跑 Dijkstra
            PathResult spurPath = dijkstraSearch(spurNode, endId,
                                                  blockedEdges, forbiddenNodes,
                                                  primaryIsTime);
            if (spurPath.pathStations.empty()) continue;  // 偏差失败，尝试下一个偏差点

            // --- 合并根路径 + 偏差路径 ---
            PathResult mergedPath;
            // 先拷贝根路径的全部节点
            for (size_t r = 0; r < rootPath.size(); ++r) {
                mergedPath.pathStations.push_back(rootPath[r]);
            }
            // 再追加偏差路径中除起点外的其余节点（避免起点重复）
            for (size_t s = 1; s < spurPath.pathStations.size(); ++s) {
                mergedPath.pathStations.push_back(spurPath.pathStations[s]);
            }

            // 重新计算合并后完整路径的指标（总耗时、换乘次数、换乘站点）
            mergedPath = recalcPathMetrics(mergedPath);

            // --- 去重检查 ---
            // 与候选堆中已有路径逐条比较，避免相同路径重复入堆
            bool duplicate = false;
            for (size_t c = 0; c < candidateHeap.size(); ++c) {
                if (isSamePath(mergedPath, candidateHeap[c])) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                candidateHeap.push_back(mergedPath);
                std::push_heap(candidateHeap.begin(), candidateHeap.end(), cmp);
            }
        }

        // 候选堆空 → 无法生成更多候选路径
        if (candidateHeap.empty()) break;

        // 从候选堆中取最优出堆，作为下一条最终路径
        std::pop_heap(candidateHeap.begin(), candidateHeap.end(), cmp);
        finalPaths.push_back(candidateHeap.back());
        candidateHeap.pop_back();
    }

    return finalPaths;
}

// ==================== 前3条时间最优路径 ====================

/**
 * 获取前3条时间最优路径
 * 功能：对外接口，调用 findTopKPaths 获取 TOP3 时间最优候选路径
 * @param startId 起点站点编号
 * @param endId   终点站点编号
 * @return 最多3条不重复的路径，按时间升序排列（时间相同按换乘次数升序）
 */
std::vector<PathResult> PathFinder::findTop3TimePaths(int startId, int endId) const {
    return findTopKPaths(startId, endId, true, 3);
}

// ==================== 前3条换乘最优路径 ====================

/**
 * 获取前3条换乘最优路径
 * 功能：对外接口，调用 findTopKPaths 获取 TOP3 换乘最优候选路径
 * @param startId 起点站点编号
 * @param endId   终点站点编号
 * @return 最多3条不重复的路径，按换乘次数升序排列（换乘相同按时间升序）
 */
std::vector<PathResult> PathFinder::findTop3TransferPaths(int startId, int endId) const {
    return findTopKPaths(startId, endId, false, 3);
}

// ==================== Dijkstra 前驱数组路径还原 ====================

/**
 * 从 Dijkstra 前驱数组还原完整路径并计算指标
 * 功能：给定前驱数组和线路名数组，回溯构建站点序列，
 *       同时遍历路径计算总耗时、换乘次数和换乘站点
 *
 * 核心流程：
 *   1. 从终点反向回溯到起点，得到原始站点序列
 *   2. 正向遍历，逐站提取边信息累加耗时
 *   3. 检测线路切换，统计换乘次数和换乘站点（含 Bug 2 同名合并）
 *   4. 返回完整路径（不折叠，Yen's 算法依赖完整路径）
 *
 * Bug 2 修复说明：
 *   世纪大道站有 4 个站内互通站 ID（249/417/632/942），之间有连续的
 *   "换乘" 边连接。遍历时这些边会产生多次换乘计数。通过记录
 *   lastTransferStationName，仅在站点名不同时才计新换乘，将
 *   4 条线路间的连续过渡合并为一次 "世纪大道站内换乘"。
 *
 * @param prev      前驱节点数组（Dijkstra 结果），prev[id] = 到达 id 的前一个站
 * @param prevLine  到达每个站点时的线路名数组
 * @param startId   起点编号
 * @param endId     终点编号（可能是 effectiveEndId，即同名站替换后的终点）
 * @return PathResult 结构体，包含完整站点序列和计算好的路径指标
 */
PathResult PathFinder::reconstructPath(const std::vector<int> &prev,
                                        const std::vector<std::string> &prevLine,
                                        int startId, int endId) const {
    PathResult result;

    // ---------- 第1步：回溯完整站点序列 ----------
    // 从终点开始反向追踪前驱，直到 -1（起点的前驱为 -1）
    std::vector<int> path;
    int cur = endId;
    while (cur != -1) {
        path.push_back(cur);
        cur = prev[cur];
    }
    std::reverse(path.begin(), path.end());  // 反转得到正向序列
    result.pathStations = path;

    // ---------- 第2步：遍历路径计算耗时与换乘 ----------
    result.totalTime = 0;
    result.transferCount = 0;
    std::string currentLine = "";                 // 当前所在线路名
    std::string lastTransferStationName = "";     // Bug 2: 上次换乘站名，用于同名合并

    for (size_t i = 1; i < path.size(); ++i) {
        int fromId = path[i - 1];
        int toId   = path[i];

        // 查找从 fromId 到 toId 的边（从邻接表中匹配终点 ID）
        std::vector<MetroEdge> edges = m_graph.getAdjEdges(fromId);
        MetroEdge usedEdge;
        bool found = false;
        for (size_t j = 0; j < edges.size(); ++j) {
            if (edges[j].endId == toId) {
                usedEdge = edges[j];
                found = true;
                break;
            }
        }
        if (!found) continue;

        // 累加该边的运行耗时（分钟）
        result.totalTime += usedEdge.time;

        if (i == 1) {
            // 第一段边：记录初始线路
            currentLine = usedEdge.lineName;
        } else {
            // ---------- 换乘检测（三种情况） ----------
            if (usedEdge.lineName == "换乘") {
                // 情况1：显式 "换乘" 边（站内连接边）
                // Bug 2: 与上次换乘同名则合并为一次站内换乘
                std::string fromName = m_stationMgr.getStationById(fromId).stationName;
                if (fromName != lastTransferStationName) {
                    result.transferCount++;
                    result.transferStations.push_back(fromId);
                    lastTransferStationName = fromName;
                }
                // "换乘" 边之后，线路由紧接着的下一条非换乘边决定
                if (i + 1 < path.size()) {
                    int nextTo = path[i + 1];
                    std::vector<MetroEdge> nextEdges = m_graph.getAdjEdges(toId);
                    for (size_t k = 0; k < nextEdges.size(); ++k) {
                        if (nextEdges[k].endId == nextTo) {
                            currentLine = nextEdges[k].lineName;
                            break;
                        }
                    }
                }
            } else if (currentLine != usedEdge.lineName) {
                // 情况2：线路名确实发生了变化（不是 "换乘" 边）
                // Bug 2: 同名站内连续换乘合并为一次
                std::string fromName = m_stationMgr.getStationById(fromId).stationName;
                if (fromName != lastTransferStationName) {
                    result.transferCount++;
                    result.transferStations.push_back(fromId);
                    lastTransferStationName = fromName;
                }
                currentLine = usedEdge.lineName;
            }
            // 情况3：线路未变化（currentLine == usedEdge.lineName）
            // 普通同线行驶，不计换乘
        }
    }

    // 保持完整路径（不折叠），Yen's 算法需要完整路径来计算阻断边
    // 折叠逻辑仅在 printPath 显示时进行，避免阻断边变为"幻影边"
    result.pathStations = path;

    return result;
}

// ==================== 路径去重工具函数 ====================

/**
 * 判断两条路径是否相同
 * 功能：通过逐站比较判断两条路径在站点序列上是否完全一致
 * @param a 路径A
 * @param b 路径B
 * @return true=两条路径的途经站点序列完全相同
 * @note 仅比较站点ID序列，不关心耗时/换乘等派生指标
 */
bool PathFinder::isSamePath(const PathResult &a, const PathResult &b) {
    if (a.pathStations.size() != b.pathStations.size()) return false;
    for (size_t i = 0; i < a.pathStations.size(); ++i) {
        if (a.pathStations[i] != b.pathStations[i]) return false;
    }
    return true;
}

/**
 * 从路径列表中移除重复路径
 * 功能：遍历路径列表，通过 isSamePath 两两比较剔除重复，
 *       保持首次出现的路径顺序不变
 * @param paths 路径列表（会被去重修改）
 * @note 时间复杂度 O(n²)，n 为路径条数（通常 ≤10，可接受）
 */
void PathFinder::deduplicatePaths(std::vector<PathResult> &paths) const {
    std::vector<PathResult> uniquePaths;
    for (size_t i = 0; i < paths.size(); ++i) {
        bool duplicate = false;
        for (size_t j = 0; j < uniquePaths.size(); ++j) {
            if (isSamePath(paths[i], uniquePaths[j])) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            uniquePaths.push_back(paths[i]);
        }
    }
    paths = uniquePaths;
}

// ==================== 路径指标重算（Yen's 路径合并后使用） ====================

/**
 * 从站点序列重新计算路径指标（总耗时、换乘次数、换乘站点）
 * 功能：在 Yen's 算法合并根路径和偏差路径后，得到一条新的完整路径，
 *       但该路径尚未计算耗时和换乘信息。本函数遍历新路径的每段边，
 *       重新汇总计算各项指标。
 *
 * 与 reconstructPath 的关系：
 *   reconstructPath 从 Dijkstra 前驱数组构建路径，同时计算指标；
 *   recalcPathMetrics 从已有的站点序列反向查找边并计算指标。
 *   二者的换乘检测逻辑完全相同（含 Bug 2 同名合并），以保证一致性。
 *
 * @param fullPath 包含完整 pathStations 的路径（会被修改 totalTime 等字段）
 * @return 填充好 totalTime / transferCount / transferStations 的路径结果
 * @note 路径站点序列保持完整（不折叠），Yen's 算法依赖完整路径
 */
PathResult PathFinder::recalcPathMetrics(PathResult fullPath) const {
    const std::vector<int> &path = fullPath.pathStations;
    // 边界处理：少于2站无法构成有效路径
    if (path.size() < 2) {
        fullPath.totalTime = 0;
        fullPath.transferCount = 0;
        fullPath.transferStations.clear();
        return fullPath;
    }

    fullPath.totalTime = 0;
    fullPath.transferCount = 0;
    fullPath.transferStations.clear();

    std::string currentLine = "";
    std::string lastTransferStationName = "";  // Bug 2: 同名站内换乘合并

    // ---------- 逐边遍历计算 ----------
    // 与 reconstructPath 中的遍历逻辑保持同步
    for (size_t i = 1; i < path.size(); ++i) {
        int fromId = path[i - 1];
        int toId   = path[i];

        // 从邻接表中查找 fromId → toId 的边
        std::vector<MetroEdge> edges = m_graph.getAdjEdges(fromId);
        MetroEdge usedEdge;
        bool found = false;
        for (size_t j = 0; j < edges.size(); ++j) {
            if (edges[j].endId == toId) {
                usedEdge = edges[j];
                found = true;
                break;
            }
        }
        if (!found) continue;

        fullPath.totalTime += usedEdge.time;

        if (i == 1) {
            // 第一段边：记录初始线路
            currentLine = usedEdge.lineName;
        } else {
            if (usedEdge.lineName == "换乘") {
                // 显式 "换乘" 边：Bug 2 同名合并
                std::string fromName = m_stationMgr.getStationById(fromId).stationName;
                if (fromName != lastTransferStationName) {
                    fullPath.transferCount++;
                    fullPath.transferStations.push_back(fromId);
                    lastTransferStationName = fromName;
                }
                // 查找换乘后的实际线路
                if (i + 1 < path.size()) {
                    int nextTo = path[i + 1];
                    std::vector<MetroEdge> nextEdges = m_graph.getAdjEdges(toId);
                    for (size_t k = 0; k < nextEdges.size(); ++k) {
                        if (nextEdges[k].endId == nextTo) {
                            currentLine = nextEdges[k].lineName;
                            break;
                        }
                    }
                }
            } else if (currentLine != usedEdge.lineName) {
                // 线路切换：Bug 2 同名合并
                std::string fromName = m_stationMgr.getStationById(fromId).stationName;
                if (fromName != lastTransferStationName) {
                    fullPath.transferCount++;
                    fullPath.transferStations.push_back(fromId);
                    lastTransferStationName = fromName;
                }
                currentLine = usedEdge.lineName;
            }
        }
    }

    // 保持完整路径（不折叠），Yen's 算法需要完整路径计算阻断边
    // 折叠逻辑仅在 printPath 显示时进行
    fullPath.pathStations = path;

    return fullPath;
}

// ==================== 格式化打印（算法调试/日志输出） ====================

/**
 * 格式化打印单条路径（成员函数）
 * 功能：以简洁格式输出单条路径的关键信息：途经站点序列、总耗时、换乘次数、换乘站点
 * 使用场景：PathFinder 内部算法调试输出，如 Yen's 候选路径确认
 * @param result    路径结果
 * @param rankLabel 排名标签（如 "方案1"、"方案2"）
 * @note 显示时自动折叠同名连续中间站（如 942→632→417→249 → 942→249）
 */
void PathFinder::printPath(const PathResult &result, const std::string &rankLabel) const {
    if (result.pathStations.empty()) {
        std::cout << "  无可用路径" << std::endl;
        return;
    }

    std::cout << "  " << rankLabel << "：" << std::endl;
    std::cout << "    途经：";

    // ---------- 折叠同名连续中间站（仅用于显示） ----------
    // 世纪大道站有 4 个互通 ID（942/632/417/249），路径中可能顺序出现
    // 942(9)→632(6)→417(4)→249(2)，显示时折叠为 942(9)→249(2)
    std::vector<int> displayPath;
    for (size_t i = 0; i < result.pathStations.size(); ++i) {
        int curId = result.pathStations[i];
        if (displayPath.empty()) {
            displayPath.push_back(curId);
        } else {
            std::string lastName = m_stationMgr.getStationById(displayPath.back()).stationName;
            std::string curName  = m_stationMgr.getStationById(curId).stationName;
            if (curName == lastName && i + 1 < result.pathStations.size()) {
                std::string nextName = m_stationMgr.getStationById(result.pathStations[i + 1]).stationName;
                if (nextName == curName) {
                    continue; // 中间同名过渡站 → 跳过（段尾保留）
                }
            }
            displayPath.push_back(curId);
        }
    }

    // 输出折叠后的站点序列
    for (size_t i = 0; i < displayPath.size(); ++i) {
        Station s = m_stationMgr.getStationById(displayPath[i]);
        std::cout << s.stationName;
        if (i < displayPath.size() - 1) {
            std::cout << " → ";
        }
    }
    std::cout << std::endl;

    // 输出路径指标
    std::cout << "    总耗时：" << result.totalTime << " 分钟" << std::endl;
    std::cout << "    换乘次数：" << result.transferCount << " 次" << std::endl;
    if (result.transferCount > 0) {
        std::cout << "    换乘站点：";
        for (size_t i = 0; i < result.transferStations.size(); ++i) {
            Station s = m_stationMgr.getStationById(result.transferStations[i]);
            std::cout << s.stationName;
            if (i < result.transferStations.size() - 1) {
                std::cout << " → ";
            }
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

/**
 * 格式化打印多条路径
 * 功能：遍历路径列表，逐条调用 printPath 输出
 * 使用场景：Yen's 算法完成后的 TOP3 结果输出
 * @param results    路径结果列表
 * @param title      标题字符串（如 "=== 最短时间方案 TOP3 ==="）
 */
void PathFinder::printPaths(const std::vector<PathResult> &results, const std::string &title) const {
    std::cout << title << std::endl;
    if (results.empty()) {
        std::cout << "  暂无路径结果" << std::endl;
        return;
    }
    for (size_t i = 0; i < results.size(); ++i) {
        std::string label = "方案" + std::to_string(i + 1);
        printPath(results[i], label);
    }
}

// ==================== 独立打印函数（课程设计菜单输出格式） ====================

/**
 * 格式化打印完整路径信息（课程设计标准输出格式）
 * 功能：严格按照课程设计效果图标准逐行打印路径详情
 * 打印内容：
 *   1. 路径规划结果标题块（=== 包裹）
 *   2. 起点、终点、总耗时、换乘次数、途经站台总数
 *   3. 逐行输出站点（带序号和线路名），线路切换时标注【换乘】
 *   4. 关闭站点检查提示
 *   5. 换乘站点汇总
 * 使用场景：菜单模块 (menu.cpp) 调用，向用户展示最终查询结果
 * @param res         路径结果
 * @param stationMgr  站点管理器引用（用于查询站点名称和线路）
 * @note 显示时自动折叠同名连续中间站；关闭站点检测基于原始完整路径
 */
void printPath(const PathResult &res, const StationManager &stationMgr) {
    if (res.pathStations.empty()) {
        std::cout << "  无可用路径" << std::endl;
        return;
    }

    // 获取起终点名称
    Station startStation = stationMgr.getStationById(res.pathStations.front());
    Station endStation   = stationMgr.getStationById(res.pathStations.back());

    // ---------- 折叠同名连续中间站（仅用于显示，不影响指标计算） ----------
    std::vector<int> displayPath;
    for (size_t i = 0; i < res.pathStations.size(); ++i) {
        int curId = res.pathStations[i];
        if (displayPath.empty()) {
            displayPath.push_back(curId);
        } else {
            std::string lastName = stationMgr.getStationById(displayPath.back()).stationName;
            std::string curName  = stationMgr.getStationById(curId).stationName;
            if (curName == lastName && i + 1 < res.pathStations.size()) {
                std::string nextName = stationMgr.getStationById(res.pathStations[i + 1]).stationName;
                if (nextName == curName) {
                    continue; // 中间同名过渡站 → 跳过
                }
            }
            displayPath.push_back(curId);
        }
    }

    // ---------- 输出标题块 ----------
    std::cout << "============================================" << std::endl;
    std::cout << "  路径规划结果" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  起点：" << startStation.stationName << std::endl;
    std::cout << "  终点：" << endStation.stationName << std::endl;
    std::cout << "  总耗时：" << res.totalTime << " 分钟" << std::endl;
    std::cout << "  换乘次数：" << res.transferCount << " 次" << std::endl;
    std::cout << "  途经站台总数：" << displayPath.size() << " 站" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;

    // ---------- 逐行输出站点（检测线路切换标注【换乘】） ----------
    std::string currentLine = "";

    for (size_t i = 0; i < displayPath.size(); ++i) {
        Station s = stationMgr.getStationById(displayPath[i]);
        std::string lineInfo = s.lineName;
        std::string transferMark = "";

        if (i == 0) {
            // 起点：记录初始线路
            currentLine = s.lineName;
        } else {
            // 判断本站是否在换乘列表中（通过 stationId 精确匹配）
            bool isTransfer = false;
            for (size_t k = 0; k < res.transferStations.size(); ++k) {
                if (res.transferStations[k] == displayPath[i]) {
                    isTransfer = true;
                    break;
                }
            }

            if (isTransfer) {
                transferMark = " 【换乘】";
            }
        }

        // 输出序号、站名和线路名
        std::cout << "  " << (i + 1) << ". " << s.stationName
                  << "（" << lineInfo << "）"
                  << transferMark << std::endl;

        // 更新当前线路（换乘之后线路切换为当前站的线路）
        if (!transferMark.empty()) {
            currentLine = s.lineName;
        }
    }

    // ---------- 关闭站点检查 ----------
    std::cout << "--------------------------------------------" << std::endl;

    bool hasClosed = false;
    for (size_t i = 0; i < res.pathStations.size(); ++i) {
        Station s = stationMgr.getStationById(res.pathStations[i]);
        if (!s.isOpen) {
            std::cout << "  [提示] " << s.stationName << "站点关闭，当前路径已自动规避" << std::endl;
            hasClosed = true;
        }
    }
    if (!hasClosed) {
        std::cout << "  该路径未经过关闭站点" << std::endl;
    }

    // ---------- 换乘信息汇总 ----------
    if (res.transferCount > 0) {
        std::cout << "  换乘站点：";
        for (size_t i = 0; i < res.transferStations.size(); ++i) {
            Station s = stationMgr.getStationById(res.transferStations[i]);
            std::cout << s.stationName;
            if (i < res.transferStations.size() - 1) {
                std::cout << " → ";
            }
        }
        std::cout << std::endl;
    }

    std::cout << "============================================" << std::endl;
}
