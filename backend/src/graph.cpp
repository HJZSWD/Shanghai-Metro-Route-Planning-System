#include "graph.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <windows.h>

/**
 * 检查文件流的前3字节是否为 UTF-8 BOM (EF BB BF)
 * @param file 已打开的文件流（读取后位置会回退到开头）
 * @return true=文件含 UTF-8 BOM，false=无BOM（可能是GBK/GB18030）
 */
static bool hasUtf8Bom(std::ifstream &file) {
    unsigned char bom[3] = {0};
    file.read(reinterpret_cast<char*>(bom), 3);
    bool found = (file.gcount() == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF);
    file.clear();
    file.seekg(0, std::ios::beg);
    return found;
}

// 文件级：标记当前正在加载的Edge.csv编码类型（true=UTF-8, false=GBK/GB18030）
// 由 loadEdge 设置，parseCsvLine 读取，避免修改 graph.h 的函数签名
static bool g_edgeFileIsUtf8 = false;

/**
 * GB18030/GBK → UTF-8 编码转换（graph.cpp 独立实现）
 * 功能：解决Edge.csv中文乱码问题，不依赖 StationManager 的私有方法
 * 说明：内部使用 MultiByteToWideChar + WideCharToMultiByte Windows API
 * @param gbkStr GB18030/GBK 编码输入字符串
 * @return UTF-8 编码输出字符串，转换失败返回原字符串
 */
static std::string localGbkToUtf8(const std::string &gbkStr) {
    try {
        if (gbkStr.empty()) return gbkStr;

        int wideLen = MultiByteToWideChar(CP_ACP, 0,
                                           gbkStr.c_str(), static_cast<int>(gbkStr.size()),
                                           NULL, 0);
        if (wideLen <= 0) return gbkStr;

        std::wstring wideStr(wideLen, L'\0');
        MultiByteToWideChar(CP_ACP, 0,
                            gbkStr.c_str(), static_cast<int>(gbkStr.size()),
                            &wideStr[0], wideLen);

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0,
                                           wideStr.c_str(), wideLen,
                                           NULL, 0,
                                           NULL, NULL);
        if (utf8Len <= 0) return gbkStr;

        std::string utf8Str(utf8Len, '\0');
        WideCharToMultiByte(CP_UTF8, 0,
                            wideStr.c_str(), wideLen,
                            &utf8Str[0], utf8Len,
                            NULL, NULL);

        return utf8Str;
    } catch (...) {
        return gbkStr;
    }
}

MetroGraph::MetroGraph(int stationCount) : m_stationCount(stationCount) {
    if (stationCount > 0) {
        m_adj.resize(stationCount);
    }
}

// ==================== 图构建 ====================

bool MetroGraph::loadEdge(const std::string &filePath) {
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[错误] 无法打开文件: " << filePath << std::endl;
            return false;
        }

        // 检测文件编码：有 BOM(EF BB BF) 即为 UTF-8，否则为 GBK/GB18030
        bool fileIsUtf8 = hasUtf8Bom(file);
        g_edgeFileIsUtf8 = fileIsUtf8;

        int edgeCount = 0;
        std::string line;
        bool isHeader = true;

        // 先清空已有数据
        clear();

        while (std::getline(file, line)) {
            // 去除尾部 \r（二进制模式）
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            // 去除首尾空白
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            size_t end = line.find_last_not_of(" \t\r\n");
            line = line.substr(start, end - start + 1);

            if (line.empty()) continue;

            // 第一行如果是UTF-8 BOM开头，剥离BOM
            if (isHeader && fileIsUtf8 && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line = line.substr(3);
            }

            // 跳过表头
            if (isHeader) {
                isHeader = false;
                continue;
            }

            try {
                MetroEdge edge = parseCsvLine(line);
                addEdge(edge);
                edgeCount++;
            } catch (const std::exception &e) {
                std::cerr << "[警告] 跳过无效边: " << line << " (" << e.what() << ")" << std::endl;
                continue;
            }
        }

        file.close();
        std::cout << "[信息] 成功加载 " << edgeCount << " 条有向边" << std::endl;
        return true;

    } catch (const std::exception &e) {
        std::cerr << "[错误] 读取Edge.csv异常: " << e.what() << std::endl;
        return false;
    }
}

void MetroGraph::addEdge(const MetroEdge &edge) {
    // 自动扩容：确保邻接表下标能容纳起点和终点
    int maxId = (edge.startId > edge.endId) ? edge.startId : edge.endId;
    if (maxId >= static_cast<int>(m_adj.size())) {
        m_adj.resize(maxId + 1);
    }
    if (maxId >= m_stationCount) {
        m_stationCount = maxId + 1;
    }

    m_adj[edge.startId].push_back(edge);
}

// ==================== 邻接查询 ====================

std::vector<MetroEdge> MetroGraph::getAdjEdges(int stationId) const {
    if (stationId < 0 || stationId >= static_cast<int>(m_adj.size())) {
        return {};
    }
    return m_adj[stationId];
}

std::vector<MetroEdge> MetroGraph::getAvailableEdges(int stationId,
                                                      const StationManager &stationMgr) const {
    std::vector<MetroEdge> result;
    if (stationId < 0 || stationId >= static_cast<int>(m_adj.size())) {
        return result;
    }

    const std::vector<MetroEdge> &edges = m_adj[stationId];
    for (size_t i = 0; i < edges.size(); ++i) {
        // 过滤掉目标站点已关闭的边
        if (!isStationClosed(edges[i].endId, stationMgr)) {
            result.push_back(edges[i]);
        }
    }
    return result;
}

// ==================== 站点状态查询 ====================

bool MetroGraph::isStationClosed(int stationId, const StationManager &stationMgr) const {
    Station s = stationMgr.getStationById(stationId);
    // stationId = -1 表示站点不存在（视为关闭）
    return (s.stationId == -1) || (!s.isOpen);
}

// ==================== 其他接口 ====================

int MetroGraph::getStationCount() const {
    return m_stationCount;
}

const std::vector<std::vector<MetroEdge>>& MetroGraph::getAdjList() const {
    return m_adj;
}

void MetroGraph::clear() {
    m_adj.clear();
    m_stationCount = 0;
}

// ==================== CSV解析 ====================

MetroEdge MetroGraph::parseCsvLine(const std::string &line, char delim) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;

    while (std::getline(ss, field, delim)) {
        // 去除字段首尾空白
        size_t start = field.find_first_not_of(" \t\r\n");
        size_t end = field.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) {
            fields.push_back("");
        } else {
            fields.push_back(field.substr(start, end - start + 1));
        }
    }

    if (fields.size() < 5) {
        throw std::runtime_error("字段不足5个，实际 " + std::to_string(fields.size()) + " 个");
    }

    MetroEdge edge;
    try {
        edge.startId = std::stoi(fields[0]);
        edge.endId = std::stoi(fields[1]);
    } catch (const std::exception &e) {
        throw std::runtime_error("无效站点ID: " + fields[0] + "->" + fields[1]);
    }
    edge.lineName = g_edgeFileIsUtf8 ? fields[2] : localGbkToUtf8(fields[2]);
    edge.direction = g_edgeFileIsUtf8 ? fields[3] : localGbkToUtf8(fields[3]);
    try {
        edge.time = std::stoi(fields[4]);
    } catch (const std::exception &e) {
        throw std::runtime_error("无效时间值: " + fields[4]);
    }

    // 校验：时间不能为负
    if (edge.time < 0) {
        throw std::runtime_error("通行时间不能为负: " + std::to_string(edge.time));
    }

    return edge;
}
