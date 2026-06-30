#ifndef PATHFINDER_H
#define PATHFINDER_H

#include <vector>
#include <string>
#include "station.h"
#include "graph.h"

/**
 * 路径结果结构体
 * 功能：存储从起点到终点的一条完整路径信息
 * 字段说明：
 *   pathStations     - 途经站点编号序列（含起点和终点）
 *   totalTime        - 总耗时（分钟）
 *   transferCount    - 换乘次数
 *   transferStations - 换乘站点编号列表（在哪些站换乘）
 */
struct PathResult {
    std::vector<int> pathStations;
    int totalTime;
    int transferCount;
    std::vector<int> transferStations;
};

/**
 * 格式化打印完整路径信息（课程设计格式）
 * 功能：严格按照课程设计效果图标准逐行打印路径详情
 * 打印内容：
 *   1. 总耗时、换乘次数、途经站台总数
 *   2. 逐行输出站点名称（带序号），切换线路标注【换乘】
 *   3. 若路径经过关闭站点，提示"XX站点关闭，当前路径已自动规避"
 * @param res         路径结果
 * @param stationMgr  站点管理器引用（用于查询站点名称和线路）
 */
void printPath(const PathResult &res, const StationManager &stationMgr);

/**
 * 路径查找器类
 * 功能：实现Dijkstra最短时间路径和最少换乘路径算法
 * 说明：支持单条最优和前3条候选路径输出，自带路径去重
 */
class PathFinder {
public:
    /**
     * 构造函数
     * @param graph       地铁图引用
     * @param stationMgr  站点管理器引用（用于过滤关闭站点和显示站名）
     */
    PathFinder(const MetroGraph &graph, const StationManager &stationMgr);

    /**
     * 计算最短时间路径（Dijkstra）
     * 策略：时间优先，相同时间选换乘更少的路径
     * @param startId 起点站点编号
     * @param endId   终点站点编号
     * @return 最短时间路径结果
     */
    PathResult findShortestTimePath(int startId, int endId) const;

    /**
     * 计算最少换乘路径
     * 策略：换乘次数优先，相同换乘次数选时间更少的路径
     * @param startId 起点站点编号
     * @param endId   终点站点编号
     * @return 最少换乘路径结果
     */
    PathResult findMinTransferPath(int startId, int endId) const;

    /**
     * 获取前3条时间最优路径（含去重）
     * @param startId 起点站点编号
     * @param endId   终点站点编号
     * @return 最多3条不重复的路径结果，按时间升序排列
     */
    std::vector<PathResult> findTop3TimePaths(int startId, int endId) const;

    /**
     * 获取前3条换乘最优路径（含去重）
     * @param startId 起点站点编号
     * @param endId   终点站点编号
     * @return 最多3条不重复的路径结果，按换乘次数升序排列
     */
    std::vector<PathResult> findTop3TransferPaths(int startId, int endId) const;

    /**
     * 格式化打印路径信息
     * @param result    路径结果
     * @param rankLabel 排名标签（如"方案1"）
     * @note 输出格式：总耗时、换乘次数、途经站点序列、换乘站点
     */
    void printPath(const PathResult &result, const std::string &rankLabel) const;

    /**
     * 格式化打印多条路径
     * @param results    路径结果列表
     * @param title      标题（如"=== 最短时间方案 TOP3 ==="）
     */
    void printPaths(const std::vector<PathResult> &results, const std::string &title) const;

private:
    const MetroGraph &m_graph;              // 地铁图引用
    const StationManager &m_stationMgr;     // 站点管理器引用

    /**
     * 路径去重：判断两条路径是否相同
     * @param a 路径A
     * @param b 路径B
     * @return true=相同路径
     */
    static bool isSamePath(const PathResult &a, const PathResult &b);

    /**
     * 路径去重：从路径列表中移除重复路径
     * @param paths 路径列表（会被去重修改）
     */
    void deduplicatePaths(std::vector<PathResult> &paths) const;

    /**
     * 从站点序列重新计算路径指标（总耗时、换乘次数、换乘站点）
     * 功能：在Yen's算法合并根路径和偏差路径后，重新计算完整路径的指标
     * @param fullPath 包含完整pathStations的路径（会被修改totalTime等字段）
     * @return 填充好指标的路径结果
     */
    PathResult recalcPathMetrics(PathResult fullPath) const;

    /**
     * 从Dijkstra前驱数组还原路径
     * @param prev      前驱节点数组
     * @param prevLine  到达每个站点时所在线路名称
     * @param startId   起点编号
     * @param endId     终点编号
     * @return 还原后的路径结果（含总耗时、换乘次数、换乘站点）
     */
    PathResult reconstructPath(const std::vector<int> &prev,
                               const std::vector<std::string> &prevLine,
                               int startId, int endId) const;

    /**
     * 通用Dijkstra搜索（支持阻断边、禁止节点和双主键切换）
     * 功能：findShortestTimePath 和 findMinTransferPath 的底层引擎
     *       也用于Yen's K短路算法中偏差路径（spur path）的计算
     * @param startId         起点编号
     * @param endId           终点编号
     * @param blockedEdges    阻断边列表，pair<fromId, toId>
     * @param forbiddenNodes  禁止节点列表，路径中不可经过这些节点
     * @param primaryIsTime   true=(时间,换乘)排序, false=(换乘,时间)排序
     * @return 最优路径结果，不可达返回空
     */
    PathResult dijkstraSearch(int startId, int endId,
                              const std::vector<std::pair<int,int>> &blockedEdges,
                              const std::vector<int> &forbiddenNodes,
                              bool primaryIsTime) const;

    /**
     * 生成前K条候选路径（Yen's K短路算法）
     * Yen's算法通过逐条"偏差"已选路径来发现全局最优替代路径，
     * 克服单一边阻断法的高遗漏率问题。
     * @param startId      起点编号
     * @param endId        终点编号
     * @param primaryIsTime true=时间优先, false=换乘优先
     * @param maxCount     最多返回条数（默认3）
     * @return 去重排序后的候选路径列表（至少包含1条，最多maxCount条）
     */
    std::vector<PathResult> findTopKPaths(int startId, int endId,
                                          bool primaryIsTime, int maxCount = 3) const;
};

#endif // PATHFINDER_H
