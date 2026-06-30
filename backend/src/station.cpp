#include "station.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cctype>
#include <windows.h>

StationManager::StationManager() {}

// ==================== 编码转换工具（Windows GBK ↔ UTF-8） ====================

std::string StationManager::gbkToUtf8(const std::string &gbkStr) {
    try {
        if (gbkStr.empty()) return gbkStr;

        // 第1步：GBK → UTF-16LE
        int wideLen = MultiByteToWideChar(CP_ACP, 0,
                                           gbkStr.c_str(), static_cast<int>(gbkStr.size()),
                                           NULL, 0);
        if (wideLen <= 0) return gbkStr;

        std::wstring wideStr(wideLen, L'\0');
        MultiByteToWideChar(CP_ACP, 0,
                            gbkStr.c_str(), static_cast<int>(gbkStr.size()),
                            &wideStr[0], wideLen);

        // 第2步：UTF-16LE → UTF-8
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
        // 转换失败时返回原字符串，不崩溃
        return gbkStr;
    }
}

std::string StationManager::utf8ToGbk(const std::string &utf8Str) {
    try {
        if (utf8Str.empty()) return utf8Str;

        // 第1步：UTF-8 → UTF-16LE
        int wideLen = MultiByteToWideChar(CP_UTF8, 0,
                                           utf8Str.c_str(), static_cast<int>(utf8Str.size()),
                                           NULL, 0);
        if (wideLen <= 0) return utf8Str;

        std::wstring wideStr(wideLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0,
                            utf8Str.c_str(), static_cast<int>(utf8Str.size()),
                            &wideStr[0], wideLen);

        // 第2步：UTF-16LE → GBK
        int gbkLen = WideCharToMultiByte(CP_ACP, 0,
                                          wideStr.c_str(), wideLen,
                                          NULL, 0,
                                          NULL, NULL);
        if (gbkLen <= 0) return utf8Str;

        std::string gbkStr(gbkLen, '\0');
        WideCharToMultiByte(CP_ACP, 0,
                            wideStr.c_str(), wideLen,
                            &gbkStr[0], gbkLen,
                            NULL, NULL);

        return gbkStr;
    } catch (...) {
        return utf8Str;
    }
}

// ==================== 工具函数 ====================

std::string StationManager::trim(const std::string &str) {
    if (str.empty()) return str;
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string StationManager::toLower(const std::string &str) {
    std::string result = str;
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
    }
    return result;
}

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
    file.seekg(0, std::ios::beg);  // 回到文件开头
    return found;
}

/**
 * GB18030/GBK → UTF-8 编码转换（station.cpp 独立实现）
 * 功能：decodeField 的 GBK 回退路径，不依赖 StationManager 的私有方法
 */
static std::string localGbkToUtf8(const std::string &gbkStr) {
    try {
        if (gbkStr.empty()) return gbkStr;
        int wideLen = MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), static_cast<int>(gbkStr.size()), NULL, 0);
        if (wideLen <= 0) return gbkStr;
        std::wstring wideStr(wideLen, L'\0');
        MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), static_cast<int>(gbkStr.size()), &wideStr[0], wideLen);
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), wideLen, NULL, 0, NULL, NULL);
        if (utf8Len <= 0) return gbkStr;
        std::string utf8Str(utf8Len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), wideLen, &utf8Str[0], utf8Len, NULL, NULL);
        return utf8Str;
    } catch (...) { return gbkStr; }
}

/**
 * 根据文件编码对中文字段做条件转换：
 *   - 若文件是 UTF-8 编码（含BOM），直接返回原字符串（不做二次转码）
 *   - 若文件是 GBK/GB18030 编码（无BOM），调用 gbkToUtf8 转码
 * @param raw      原始字段字符串
 * @param fileIsUtf8 文件是否为 UTF-8 编码
 * @return UTF-8 编码的字符串
 */
static std::string decodeField(const std::string &raw, bool fileIsUtf8) {
    if (fileIsUtf8) {
        return raw;  // 已是 UTF-8，直接使用
    }
    return localGbkToUtf8(raw);
}

// ==================== CSV 文件读写 ====================

bool StationManager::loadStation(const std::string &filePath) {
    try {
        // 以二进制模式打开，避免文本模式下的编码干扰
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[错误] 无法打开文件: " << filePath << std::endl;
            return false;
        }

        // 检测文件编码：有 BOM(EF BB BF) 即为 UTF-8，否则为 GBK/GB18030
        bool fileIsUtf8 = hasUtf8Bom(file);

        m_stations.clear();
        std::string line;
        bool isHeader = true;

        while (std::getline(file, line)) {
            // 去除尾部 \r（二进制模式下可能残留）
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            line = trim(line);

            if (line.empty()) continue;

            // 第一行如果是UTF-8 BOM开头，剥离BOM
            if (isHeader && fileIsUtf8 && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line = line.substr(3);
            }

            if (isHeader) {
                isHeader = false;
                continue;
            }

            // 按逗号分割
            std::vector<std::string> fields;
            std::stringstream ss(line);
            std::string field;
            while (std::getline(ss, field, ',')) {
                fields.push_back(trim(field));
            }

            if (fields.size() < 4) {
                std::cerr << "[警告] 字段缺失，跳过行: " << line << std::endl;
                continue;
            }

            Station station;
            try {
                station.stationId = std::stoi(fields[0]);
                // 中文文本字段：根据编码做条件转换
                station.stationName = decodeField(fields[1], fileIsUtf8);
                station.lineName    = decodeField(fields[2], fileIsUtf8);
                // isOpen 转码后判断：兼容 "1/0" 数字格式和中文"开启"/"开放"/"关闭"格式
                std::string isOpenStr = decodeField(fields[3], fileIsUtf8);
                station.isOpen = (isOpenStr == "1" || isOpenStr == "true"
                               || isOpenStr == "True"
                               || isOpenStr == "开放" || isOpenStr == "开启");
            } catch (const std::exception &e) {
                std::cerr << "[警告] 格式错误，跳过行: " << line << " (" << e.what() << ")" << std::endl;
                continue;
            }

            // ID 重复检查
            bool duplicated = false;
            for (size_t i = 0; i < m_stations.size(); ++i) {
                if (m_stations[i].stationId == station.stationId) {
                    std::cerr << "[警告] 站点ID重复: " << station.stationId
                              << "，已忽略重复记录: " << station.stationName << std::endl;
                    duplicated = true;
                    break;
                }
            }
            if (!duplicated) {
                m_stations.push_back(station);
            }
        }

        file.close();
        std::cout << "[信息] 成功加载 " << m_stations.size() << " 个站点" << std::endl;
        return true;

    } catch (const std::exception &e) {
        std::cerr << "[错误] 读取CSV异常: " << e.what() << std::endl;
        return false;
    }
}

bool StationManager::saveStation(const std::string &filePath) {
    try {
        // 以二进制模式写入
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[错误] 无法写入文件: " << filePath << std::endl;
            return false;
        }

        // 写入 UTF-8 BOM + 表头
        file << "\xEF\xBB\xBF";
        file << "站点ID,站点名,所属线路,运营状态\r\n";

        // 写入数据行（内存中已是UTF-8，直接写入）
        for (size_t i = 0; i < m_stations.size(); ++i) {
            const Station &s = m_stations[i];
            file << s.stationId << ","
                 << s.stationName << ","
                 << s.lineName << ","
                 << (s.isOpen ? "1" : "0") << "\r\n";
        }

        file.close();
        std::cout << "[信息] 成功保存 " << m_stations.size() << " 个站点到 " << filePath << std::endl;
        return true;

    } catch (const std::exception &e) {
        std::cerr << "[错误] 写入CSV异常: " << e.what() << std::endl;
        return false;
    }
}

// ==================== 批量操作 ====================

bool StationManager::batchUpdateStatus(const std::string &csvPath) {
    try {
        std::ifstream file(csvPath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[错误] 无法打开批量更新文件: " << csvPath << std::endl;
            return false;
        }

        // 检测文件编码：有 BOM(EF BB BF) 即为 UTF-8，否则为 GBK/GB18030
        bool fileIsUtf8 = hasUtf8Bom(file);

        int updateCount = 0;
        std::string line;
        bool isHeader = true;

        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            line = trim(line);
            if (line.empty()) continue;

            // 第一行如果是UTF-8 BOM开头，剥离BOM
            if (isHeader && fileIsUtf8 && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line = line.substr(3);
            }

            if (isHeader) {
                isHeader = false;
                continue;
            }

            std::vector<std::string> fields;
            std::stringstream ss(line);
            std::string field;
            while (std::getline(ss, field, ',')) {
                fields.push_back(trim(field));
            }

            if (fields.size() < 2) {
                std::cerr << "[警告] 批量更新行字段缺失，跳过: " << line << std::endl;
                continue;
            }

            try {
                int stationId = std::stoi(fields[0]);
                // fields[1] 可能是中文"开启"/"开放"/"关闭"或数字"1/0"，根据编码做条件转换
                std::string statusStr = decodeField(fields[1], fileIsUtf8);
                bool isOpen = (statusStr == "1" || statusStr == "true"
                            || statusStr == "True"
                            || statusStr == "开放" || statusStr == "开启");

                bool found = false;
                for (size_t i = 0; i < m_stations.size(); ++i) {
                    if (m_stations[i].stationId == stationId) {
                        m_stations[i].isOpen = isOpen;
                        found = true;
                        updateCount++;
                        break;
                    }
                }
                if (!found) {
                    std::cerr << "[警告] 批量更新：站点ID " << stationId << " 不存在" << std::endl;
                }
            } catch (const std::exception &e) {
                std::cerr << "[警告] 批量更新行格式错误，跳过: " << line << " (" << e.what() << ")" << std::endl;
                continue;
            }
        }

        file.close();
        std::cout << "[信息] 批量更新完成，共修改 " << updateCount << " 个站点状态" << std::endl;
        return true;

    } catch (const std::exception &e) {
        std::cerr << "[错误] 批量更新异常: " << e.what() << std::endl;
        return false;
    }
}

bool StationManager::resetAllStation(const std::string &initCsvPath) {
    m_stations.clear();

    if (!loadStation(initCsvPath)) {
        std::cerr << "[错误] 恢复初始状态失败，无法读取: " << initCsvPath << std::endl;
        return false;
    }

    // 确保所有站点为开放状态
    for (size_t i = 0; i < m_stations.size(); ++i) {
        m_stations[i].isOpen = true;
    }

    std::cout << "[信息] 已恢复初始状态，当前 " << m_stations.size() << " 个站点全部开放" << std::endl;
    return true;
}

// ==================== 站点查询 ====================

std::vector<Station> StationManager::fuzzySearch(const std::string &keyword) const {
    std::vector<Station> result;
    if (keyword.empty()) return result;

    std::string lowerKeyword = toLower(keyword);

    for (size_t i = 0; i < m_stations.size(); ++i) {
        std::string lowerName = toLower(m_stations[i].stationName);
        if (lowerName.find(lowerKeyword) != std::string::npos) {
            result.push_back(m_stations[i]);
        }
    }
    return result;
}

std::vector<MergedStationInfo> StationManager::fuzzySearchMerged(const std::string &keyword) const {
    std::vector<MergedStationInfo> mergedResult;
    if (keyword.empty()) return mergedResult;

    // 先做普通模糊搜索
    std::vector<Station> raw = fuzzySearch(keyword);
    if (raw.empty()) return mergedResult;

    // 按站名分组合并
    for (size_t i = 0; i < raw.size(); ++i) {
        bool found = false;
        for (size_t j = 0; j < mergedResult.size(); ++j) {
            if (mergedResult[j].stationName == raw[i].stationName) {
                // 追加线路名称（去重）
                if (mergedResult[j].lineNames.find(raw[i].lineName) == std::string::npos) {
                    mergedResult[j].lineNames += "/" + raw[i].lineName;
                }
                // 追加站点ID
                mergedResult[j].stationIds.push_back(raw[i].stationId);
                // 任意一条线路开放即为开放
                if (raw[i].isOpen) {
                    mergedResult[j].isOpen = true;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            MergedStationInfo info;
            info.stationName = raw[i].stationName;
            info.lineNames = raw[i].lineName;
            info.stationIds.push_back(raw[i].stationId);
            info.isOpen = raw[i].isOpen;
            mergedResult.push_back(info);
        }
    }

    return mergedResult;
}

std::vector<Station> StationManager::getClosedStations() const {
    std::vector<Station> result;
    for (size_t i = 0; i < m_stations.size(); ++i) {
        if (!m_stations[i].isOpen) {
            result.push_back(m_stations[i]);
        }
    }
    return result;
}

std::vector<Station> StationManager::getStationByLine(int lineNum) const {
    std::vector<Station> result;
    std::string targetLine = std::to_string(lineNum) + "号线";

    for (size_t i = 0; i < m_stations.size(); ++i) {
        if (m_stations[i].lineName == targetLine) {
            result.push_back(m_stations[i]);
        }
    }
    return result;
}

// ==================== 单站点操作 ====================

bool StationManager::setStationStatus(int stationId, bool open) {
    for (size_t i = 0; i < m_stations.size(); ++i) {
        if (m_stations[i].stationId == stationId) {
            m_stations[i].isOpen = open;
            std::cout << "[信息] 站点 " << m_stations[i].stationName
                      << " (ID=" << stationId << ") 状态已改为 "
                      << (open ? "开放" : "关闭") << std::endl;
            return true;
        }
    }
    std::cerr << "[错误] 站点ID " << stationId << " 不存在" << std::endl;
    return false;
}

Station StationManager::getStationById(int id) const {
    for (size_t i = 0; i < m_stations.size(); ++i) {
        if (m_stations[i].stationId == id) {
            return m_stations[i];
        }
    }
    Station invalid;
    invalid.stationId = -1;
    invalid.stationName = "";
    invalid.lineName = "";
    invalid.isOpen = false;
    return invalid;
}

const std::vector<Station>& StationManager::getAllStations() const {
    return m_stations;
}
