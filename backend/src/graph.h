#ifndef GRAPH_H
#define GRAPH_H

#include <vector>
#include <string>
#include "station.h"

/**
 * 地铁边结构体
 * 功能：存储一条有向边的全部信息，对应Edge.csv中的一行
 * 字段说明：
 *   startId   - 起点站点编号
 *   endId     - 终点站点编号
 *   lineName  - 所属线路名称（如"1号线"）
 *   direction - 运行方向（如"上行/下行/换乘"）
 *   time      - 通行时间（分钟），相邻站为行驶时间，换乘统一为5分钟
 */
struct MetroEdge {
    int startId;
    int endId;
    std::string lineName;
    std::string direction;
    int time;
};

/**
 * 地铁图类（有向邻接表）
 * 功能：基于Edge.csv构建地铁有向图，提供邻接查询接口
 * 说明：A→B和B→A作为两条独立有向边存储；邻接表下标直接对应站点编号
 */
class MetroGraph {
public:
    /**
     * 构造函数
     * @param stationCount 站点总数（用于预分配邻接表大小）
     */
    explicit MetroGraph(int stationCount = 0);

    // ==================== 图构建 ====================

    /**
     * 读取Edge.csv构建有向邻接表
     * 相邻站点A→B、B→A生成两条独立边
     * @param filePath Edge.csv文件路径
     * @return true=加载成功，false=加载失败
     * @note 异常捕获：文件不存在、无效站点ID、字段缺失
     */
    bool loadEdge(const std::string &filePath);

    /**
     * 添加一条有向边到邻接表
     * @param edge 要添加的边
     * @note 若邻接表长度不足，自动扩容
     */
    void addEdge(const MetroEdge &edge);

    // ==================== 邻接查询 ====================

    /**
     * 返回指定站点所有邻接边（不过滤关闭站点）
     * @param stationId 当前站点编号
     * @return 从该站点出发的所有有向边列表
     */
    std::vector<MetroEdge> getAdjEdges(int stationId) const;

    /**
     * 返回指定站点的可用邻接边（自动过滤目标端关闭的站点）
     * @param stationId   当前站点编号
     * @param stationMgr  站点管理器引用
     * @return 过滤掉目标站点已关闭后的有向边列表
     */
    std::vector<MetroEdge> getAvailableEdges(int stationId, const StationManager &stationMgr) const;

    // ==================== 站点状态查询 ====================

    /**
     * 调用站点模块判断站点是否关闭
     * @param stationId   目标站点编号
     * @param stationMgr  站点管理器引用
     * @return true=站点已关闭或不存在，false=站点正常开放
     */
    bool isStationClosed(int stationId, const StationManager &stationMgr) const;

    // ==================== 其他接口 ====================

    /**
     * 获取站点总数
     * @return 站点数量
     */
    int getStationCount() const;

    /**
     * 获取邻接表的常引用
     * @return 邻接表
     */
    const std::vector<std::vector<MetroEdge>>& getAdjList() const;

    /**
     * 清空图
     */
    void clear();

private:
    std::vector<std::vector<MetroEdge>> m_adj;  // 有向邻接表，下标=站点编号
    int m_stationCount;                          // 站点总数

    /**
     * 解析一行CSV记录为MetroEdge对象
     * @param line  CSV行文本（格式：startId,endId,lineName,direction,time）
     * @param delim 分隔符，默认逗号
     * @return 解析后的MetroEdge
     * @throws std::runtime_error 解析失败时抛出
     */
    static MetroEdge parseCsvLine(const std::string &line, char delim = ',');
};

#endif // GRAPH_H
