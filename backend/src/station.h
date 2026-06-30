#ifndef STATION_H
#define STATION_H

#include <string>
#include <vector>

/**
 * 合并站点信息结构体
 * 功能：将同名但不同线路的站台合并为一条记录（如"人民广场"跨1/2/8号线）
 * 字段说明：
 *   stationName  - 站点名称（如"人民广场"）
 *   lineNames    - 合并后的线路名称（如"1号线/2号线/8号线"）
 *   stationIds   - 该站点在不同线路下的所有编号
 *   isOpen       - 任意一条线路开放即为true
 */
struct MergedStationInfo {
    std::string stationName;
    std::string lineNames;
    std::vector<int> stationIds;
    bool isOpen;
};

/**
 * 站点结构体
 * 功能：存储单个站点的全部基础信息
 * 字段说明：
 *   stationId   - 站点唯一编号（正整数）
 *   stationName - 站点名称（如"人民广场"）
 *   lineName    - 所属线路名称（如"1号线"）
 *   isOpen      - 运营状态，true=开放，false=关闭
 */
struct Station {
    int stationId;
    std::string stationName;
    std::string lineName;
    bool isOpen;
};

/**
 * 站点管理器类
 * 功能：负责站点数据的加载、存储、查询和修改
 * 说明：所有站点数据封装在类内部，通过类实例传参调用
 */
class StationManager {
public:
    StationManager();

    // ==================== CSV 文件读写 ====================

    /**
     * 从 Station.csv 加载站点数据
     * @param filePath CSV文件路径
     * @return true=加载成功，false=加载失败
     * @note 异常捕获：文件不存在、空行、字段缺失、格式错误
     */
    bool loadStation(const std::string &filePath);

    /**
     * 将内存站点数据写回 Station.csv
     * @param filePath CSV文件路径
     * @return true=保存成功，false=保存失败
     */
    bool saveStation(const std::string &filePath);

    // ==================== 批量操作 ====================

    /**
     * 读取 update_station_status.csv 批量修改站点开闭
     * @param csvPath CSV文件路径（格式：stationId,isOpen）
     * @return true=批量更新成功，false=失败
     * @note 异常捕获：文件不存在、字段缺失、格式错误
     */
    bool batchUpdateStatus(const std::string &csvPath);

    /**
     * 读取 Station_init.csv 恢复所有站点为初始开放状态
     * @param initCsvPath Station_init.csv 文件路径
     * @return true=恢复成功，false=失败
     * @note 该操作会将当前站点数据替换为初始数据
     */
    bool resetAllStation(const std::string &initCsvPath);

    // ==================== 站点查询 ====================

    /**
     * 模糊匹配站名，包含关键词即返回（不区分大小写）
     * @param keyword 搜索关键词
     * @return 匹配的站点列表
     */
    std::vector<Station> fuzzySearch(const std::string &keyword) const;

    /**
     * 模糊匹配站名并合并同名换乘站
     * 功能：将不同线路的同名站点合并为一条记录（如"人民广场"只显示一次）
     *       自动汇总线路名称，用"/"分隔
     * @param keyword 搜索关键词
     * @return 合并后的站点信息列表
     */
    std::vector<MergedStationInfo> fuzzySearchMerged(const std::string &keyword) const;

    /**
     * 获取所有关闭站点
     * @return 关闭站点列表
     */
    std::vector<Station> getClosedStations() const;

    /**
     * 根据线路编号返回该线路全部站点
     * @param lineNum 线路编号（如 1 表示"1号线"）
     * @return 该线路上的站点列表
     */
    std::vector<Station> getStationByLine(int lineNum) const;

    // ==================== 单站点操作 ====================

    /**
     * 手动修改单个站点开闭状态
     * @param stationId 目标站点编号
     * @param open      目标状态（true=开放，false=关闭）
     * @return true=修改成功，false=站点不存在
     */
    bool setStationStatus(int stationId, bool open);

    /**
     * 根据ID查询单个站点信息
     * @param id 站点编号
     * @return 站点结构体（若未找到，stationId置为-1）
     */
    Station getStationById(int id) const;

    /**
     * 获取所有站点列表
     * @return 全部站点列表的常引用
     */
    const std::vector<Station>& getAllStations() const;

private:
    std::vector<Station> m_stations;  // 当前站点数据

    /**
     * 去除字符串首尾空白字符
     * @param str 输入字符串
     * @return 去除首尾空白后的字符串
     */
    static std::string trim(const std::string &str);

    /**
     * 将字符串转换为小写（用于不区分大小写的搜索）
     * @param str 输入字符串
     * @return 全小写字符串
     */
    static std::string toLower(const std::string &str);

    /**
     * 将GBK编码字符串转换为UTF-8编码（Windows平台）
     * 功能：解决Excel导出的CSV文件（GBK编码）中文乱码问题
     *       内部使用 MultiByteToWideChar + WideCharToMultiByte API
     * @param gbkStr GBK编码的输入字符串
     * @return UTF-8编码的输出字符串
     * @note 若转换失败或输入为空，返回原字符串
     */
    static std::string gbkToUtf8(const std::string &gbkStr);

    /**
     * 将UTF-8编码字符串转换为GBK编码（Windows平台）
     * 功能：用于保存CSV文件时保证Excel可正常打开
     * @param utf8Str UTF-8编码的输入字符串
     * @return GBK编码的输出字符串
     * @note 若转换失败或输入为空，返回原字符串
     */
    static std::string utf8ToGbk(const std::string &utf8Str);
};

#endif // STATION_H
