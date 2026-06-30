#include "menu.h"
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cctype>
#include <limits>

MenuController::MenuController(StationManager &stationMgr, MetroGraph &graph, PathFinder &pathFinder)
    : m_stationMgr(stationMgr), m_graph(graph), m_pathFinder(pathFinder) {}

// ==================== 主循环 ====================

void MenuController::run() {
    std::cout << "============================================" << std::endl;
    std::cout << "    上海地铁线路查询系统" << std::endl;
    std::cout << "============================================" << std::endl;

    bool running = true;
    while (running) {
        int choice = showMainMenu();
        switch (choice) {
            case 1:
                pathPlanningMenu();
                break;
            case 2:
                stationManagementMenu();
                break;
            case 3:
                std::cout << "感谢使用上海地铁线路查询系统，再见！" << std::endl;
                running = false;
                break;
            default:
                std::cout << "[错误] 无效选项，请输入 1-3" << std::endl;
                break;
        }
    }
}

// ==================== 一级菜单 ====================

int MenuController::showMainMenu() const {
    std::cout << "\n---------- 主菜单 ----------" << std::endl;
    std::cout << "  1. 路径规划功能" << std::endl;
    std::cout << "  2. 线路站点/运营状态管理" << std::endl;
    std::cout << "  3. 退出程序" << std::endl;
    std::cout << "----------------------------" << std::endl;

    return readIntInput("请输入选项（1-3）：");
}

// ==================== 路径规划二级菜单 ====================

void MenuController::pathPlanningMenu() {
    bool inSubMenu = true;
    while (inSubMenu) {
        std::cout << "\n---------- 路径规划 ----------" << std::endl;
        std::cout << "  1. 单条最短时间路径" << std::endl;
        std::cout << "  2. 3条最短时间候选路径" << std::endl;
        std::cout << "  3. 单条最少换乘路径" << std::endl;
        std::cout << "  4. 3条最少换乘候选路径" << std::endl;
        std::cout << "  5. 返回上级菜单" << std::endl;
        std::cout << "-----------------------------" << std::endl;

        int choice = readIntInput("请输入选项（1-5）：");
        switch (choice) {
            case 1: queryShortestTimePath();   waitForReturn(); break;
            case 2: queryTop3TimePaths();      waitForReturn(); break;
            case 3: queryMinTransferPath();    waitForReturn(); break;
            case 4: queryTop3TransferPaths();  waitForReturn(); break;
            case 5: inSubMenu = false; break;
            default:
                std::cout << "[错误] 无效选项，请输入 1-5" << std::endl;
                break;
        }
    }
}

// ==================== 站点管理二级菜单 ====================

void MenuController::stationManagementMenu() {
    bool inSubMenu = true;
    while (inSubMenu) {
        std::cout << "\n---------- 站点运营管理 ----------" << std::endl;
        std::cout << "  1. CSV批量更新站点开闭" << std::endl;
        std::cout << "  2. 手工修改单个站点状态" << std::endl;
        std::cout << "  3. 显示当前所有关闭站点" << std::endl;
        std::cout << "  4. 恢复全部站点初始状态" << std::endl;
        std::cout << "  5. 查询指定线路全部站点" << std::endl;
        std::cout << "  6. 返回上级菜单" << std::endl;
        std::cout << "-------------------------------" << std::endl;

        int choice = readIntInput("请输入选项（1-6）：");
        switch (choice) {
            case 1: executeBatchUpdate();          waitForReturn(); break;
            case 2: executeManualSetStatus();      waitForReturn(); break;
            case 3: executeShowClosedStations();   waitForReturn(); break;
            case 4: executeRestoreInit();          waitForReturn(); break;
            case 5: executeQueryLineStations();    waitForReturn(); break;
            case 6: inSubMenu = false; break;
            default:
                std::cout << "[错误] 无效选项，请输入 1-6" << std::endl;
                break;
        }
    }
}

// ==================== 输入校验工具 ====================

int MenuController::readIntInput(const std::string &prompt) const {
    std::string input;
    std::cout << prompt;
    std::getline(std::cin, input);

    // 检查输入是否为空
    if (input.empty()) return -1;

    // 检查是否全为数字（允许前导负号）
    bool isNumber = true;
    bool hasDigit = false;
    for (size_t i = 0; i < input.size(); ++i) {
        char ch = input[i];
        if (i == 0 && ch == '-') continue;       // 允许负号开头
        if (ch >= '0' && ch <= '9') {
            hasDigit = true;
        } else {
            isNumber = false;
            break;
        }
    }
    if (!isNumber || !hasDigit) return -1;

    // 转换为整数
    std::stringstream ss(input);
    int result;
    ss >> result;
    return result;
}

void MenuController::waitForReturn() const {
    std::cout << "\n按回车返回菜单...";
    std::cin.get();  // 等待用户按回车（前序getline已消费换行符，只需一次）
}

// ==================== 关键词选站 ====================

int MenuController::selectStationByKeyword(const std::string &prompt) {
    std::cout << prompt;
    std::string keyword;
    std::getline(std::cin, keyword);

    // 输入为空返回
    if (keyword.empty()) return -1;

    // 模糊搜索（不合并同名站，各线路独立显示）
    std::vector<Station> results = m_stationMgr.fuzzySearch(keyword);
    if (results.empty()) {
        std::cout << "[提示] 未找到匹配的站点，请重试" << std::endl;
        return -1;
    }

    // 展示搜索结果（每个站名+线路独立一项）
    std::cout << "  匹配到 " << results.size() << " 个站点：" << std::endl;
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << results[i].stationName
                  << "（" << results[i].lineName << "）"
                  << (results[i].isOpen ? "" : " [关闭]")
                  << std::endl;
    }

    // 如果只有1个结果，直接选中
    if (results.size() == 1) {
        std::cout << "  → 自动选中：" << results[0].stationName
                  << "（" << results[0].lineName << "）" << std::endl;
        return results[0].stationId;
    }

    // 用户选择序号
    int choice = readIntInput("请输入序号选择站点（输入0或空取消）：");
    if (choice <= 0 || choice > static_cast<int>(results.size())) {
        std::cout << "[提示] 已取消选择" << std::endl;
        return -1;
    }

    return results[choice - 1].stationId;
}

// ==================== 路径规划执行函数 ====================

void MenuController::queryShortestTimePath() {
    std::cout << "\n--- 单条最短时间路径 ---" << std::endl;

    int startId = selectStationByKeyword("请输入起点站关键词：");
    if (startId == -1) return;

    int endId = selectStationByKeyword("请输入终点站关键词：");
    if (endId == -1) return;

    if (startId == endId) {
        std::cout << "[提示] 起点和终点相同" << std::endl;
        return;
    }

    PathResult result = m_pathFinder.findShortestTimePath(startId, endId);
    if (result.pathStations.empty()) {
        Station s = m_stationMgr.getStationById(startId);
        Station e = m_stationMgr.getStationById(endId);
        std::cout << "  无法从 " << s.stationName << " 到达 " << e.stationName << std::endl;
        return;
    }

    printPath(result, m_stationMgr);
}

void MenuController::queryTop3TimePaths() {
    std::cout << "\n--- 3条最短时间候选路径 ---" << std::endl;

    int startId = selectStationByKeyword("请输入起点站关键词：");
    if (startId == -1) return;

    int endId = selectStationByKeyword("请输入终点站关键词：");
    if (endId == -1) return;

    if (startId == endId) {
        std::cout << "[提示] 起点和终点相同" << std::endl;
        return;
    }

    std::vector<PathResult> results = m_pathFinder.findTop3TimePaths(startId, endId);
    if (results.empty()) {
        Station s = m_stationMgr.getStationById(startId);
        Station e = m_stationMgr.getStationById(endId);
        std::cout << "  无法从 " << s.stationName << " 到达 " << e.stationName << std::endl;
        return;
    }

    std::cout << "\n========== 最短时间方案 TOP" << results.size() << " ==========" << std::endl;
    for (size_t i = 0; i < results.size(); ++i) {
        std::string label = "方案" + std::to_string(i + 1);
        std::cout << "\n  ----- " << label << " -----" << std::endl;
        printPath(results[i], m_stationMgr);
    }
}

void MenuController::queryMinTransferPath() {
    std::cout << "\n--- 单条最少换乘路径 ---" << std::endl;

    int startId = selectStationByKeyword("请输入起点站关键词：");
    if (startId == -1) return;

    int endId = selectStationByKeyword("请输入终点站关键词：");
    if (endId == -1) return;

    if (startId == endId) {
        std::cout << "[提示] 起点和终点相同" << std::endl;
        return;
    }

    PathResult result = m_pathFinder.findMinTransferPath(startId, endId);
    if (result.pathStations.empty()) {
        Station s = m_stationMgr.getStationById(startId);
        Station e = m_stationMgr.getStationById(endId);
        std::cout << "  无法从 " << s.stationName << " 到达 " << e.stationName << std::endl;
        return;
    }

    printPath(result, m_stationMgr);
}

void MenuController::queryTop3TransferPaths() {
    std::cout << "\n--- 3条最少换乘候选路径 ---" << std::endl;

    int startId = selectStationByKeyword("请输入起点站关键词：");
    if (startId == -1) return;

    int endId = selectStationByKeyword("请输入终点站关键词：");
    if (endId == -1) return;

    if (startId == endId) {
        std::cout << "[提示] 起点和终点相同" << std::endl;
        return;
    }

    std::vector<PathResult> results = m_pathFinder.findTop3TransferPaths(startId, endId);
    if (results.empty()) {
        Station s = m_stationMgr.getStationById(startId);
        Station e = m_stationMgr.getStationById(endId);
        std::cout << "  无法从 " << s.stationName << " 到达 " << e.stationName << std::endl;
        return;
    }

    std::cout << "\n========== 最少换乘方案 TOP" << results.size() << " ==========" << std::endl;
    for (size_t i = 0; i < results.size(); ++i) {
        std::string label = "方案" + std::to_string(i + 1);
        std::cout << "\n  ----- " << label << " -----" << std::endl;
        printPath(results[i], m_stationMgr);
    }
}

// ==================== 站点管理执行函数 ====================

void MenuController::executeBatchUpdate() {
    std::cout << "\n--- CSV批量更新站点开闭 ---" << std::endl;
    std::cout << "请输入 update_station_status.csv 文件路径" << std::endl;
    std::cout << "（直接回车使用默认路径 data/update_station_status.csv）：" << std::endl;

    std::string filePath;
    std::cout << ">> ";
    std::getline(std::cin, filePath);

    if (filePath.empty()) {
        filePath = "data/update_station_status.csv";
    }

    bool ok = m_stationMgr.batchUpdateStatus(filePath);
    if (ok) {
        std::cout << "[成功] 批量更新完成" << std::endl;
    } else {
        std::cout << "[失败] 批量更新未完成" << std::endl;
    }

    // 保存到Station.csv
    m_stationMgr.saveStation("data/Station.csv");
    std::cout << "[信息] 已同步保存到 Station.csv" << std::endl;
}

void MenuController::executeManualSetStatus() {
    std::cout << "\n--- 手工修改单个站点状态 ---" << std::endl;

    int stationId = selectStationByKeyword("请输入要修改的站点关键词：");
    if (stationId == -1) return;

    Station s = m_stationMgr.getStationById(stationId);
    std::cout << "  当前状态：" << s.stationName << "（" << s.lineName << "）"
              << (s.isOpen ? " [开放]" : " [关闭]") << std::endl;

    std::cout << "  输入 1 开启，0 关闭：";
    int status = readIntInput("");
    if (status != 0 && status != 1) {
        std::cout << "[错误] 无效输入，请输入 1 或 0" << std::endl;
        return;
    }

    bool ok = m_stationMgr.setStationStatus(stationId, (status == 1));
    if (ok) {
        m_stationMgr.saveStation("data/Station.csv");
        std::cout << "[信息] 已同步保存到 Station.csv" << std::endl;
    }
}

void MenuController::executeShowClosedStations() {
    std::cout << "\n--- 当前所有关闭站点 ---" << std::endl;

    std::vector<Station> closed = m_stationMgr.getClosedStations();
    if (closed.empty()) {
        std::cout << "  当前没有关闭站点，所有站点正常运行" << std::endl;
        return;
    }

    std::cout << "  共 " << closed.size() << " 个站点关闭：" << std::endl;
    for (size_t i = 0; i < closed.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << closed[i].stationName
                  << "（" << closed[i].lineName << "）"
                  << "  ID=" << closed[i].stationId << std::endl;
    }
}

void MenuController::executeRestoreInit() {
    std::cout << "\n--- 恢复全部站点初始状态 ---" << std::endl;
    std::cout << "  该操作将清除所有手动修改的记录" << std::endl;
    std::cout << "  是否继续？（y/n）：";

    std::string confirm;
    std::getline(std::cin, confirm);
    // 统一转小写匹配
    std::string confirmLower;
    for (size_t i = 0; i < confirm.size(); ++i) {
        confirmLower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(confirm[i]))));
    }
    if (confirmLower != "y" && confirmLower != "yes") {
        std::cout << "[提示] 已取消" << std::endl;
        return;
    }

    std::string initPath = "data/Station_init.csv";
    bool ok = m_stationMgr.resetAllStation(initPath);
    if (ok) {
        m_stationMgr.saveStation("data/Station.csv");
        std::cout << "[成功] 已恢复全部站点为初始开放状态" << std::endl;
    } else {
        std::cout << "[失败] 恢复失败，请检查 " << initPath << " 是否存在" << std::endl;
    }
}

void MenuController::executeQueryLineStations() {
    std::cout << "\n--- 查询指定线路全部站点 ---" << std::endl;
    std::cout << "  请输入线路编号（如 1 表示1号线、2 表示2号线）：" << std::endl;

    int lineNum = readIntInput(">> ");
    if (lineNum <= 0) {
        std::cout << "[错误] 无效的线路编号" << std::endl;
        return;
    }

    std::vector<Station> stations = m_stationMgr.getStationByLine(lineNum);
    if (stations.empty()) {
        std::cout << "  未找到 " << lineNum << " 号线的站点信息" << std::endl;
        return;
    }

    std::cout << "  " << lineNum << "号线共 " << stations.size() << " 个站点：" << std::endl;
    for (size_t i = 0; i < stations.size(); ++i) {
        Station &s = stations[i];
        std::cout << "  " << (i + 1) << ". " << s.stationName
                  << (s.isOpen ? " [开放]" : " [关闭]")
                  << "  ID=" << s.stationId << std::endl;
    }
}
