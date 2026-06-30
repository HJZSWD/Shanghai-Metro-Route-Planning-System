#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <cctype>
#include <windows.h>
#include "station.h"
#include "graph.h"
#include "pathfinder.h"
#include "menu.h"

/**
 * 获取程序所在目录的上级目录（项目根目录）
 * 功能：根据 metro.exe 所在位置自动推导 data/ 目录路径
 *       支持 exe 在 backend/ 或项目根目录两种部署方式
 * @return 项目根目录绝对路径（末尾带反斜杠）
 */
static std::string getProjectRoot() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string path(exePath);

    // 取 exe 所在目录
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        path = path.substr(0, pos + 1);
    }

    // 如果 exe 在 backend/ 下，返回上级目录
    std::string lower = path;
    for (size_t i = 0; i < lower.size(); ++i) {
        lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));
    }
    if (lower.find("backend") != std::string::npos ||
        lower.find("backend\\") != std::string::npos) {
        // 上移一层到项目根目录
        pos = path.substr(0, path.length() - 1).find_last_of("\\/");
        if (pos != std::string::npos) {
            path = path.substr(0, pos + 1);
        }
    }

    return path;
}

/**
 * 程序入口
 * 功能：初始化各模块，加载 data/ 目录下4个CSV文件，启动菜单交互
 * 说明：
 *   1. 支持命令行参数 --hidden：以隐藏控制台窗口方式运行，适用于后台模式
 *   2. 自动加载 Station.csv（站点主数据）、Edge.csv（地铁边数据）
 *      Station_init.csv（初始备份）、update_station_status.csv（批量更新）
 *   3. 任何CSV加载失败会控制台提示并建议检查 data/ 目录
 *   4. 所有模块通过实例传参调用，无全局变量
 */
int main(int argc, char *argv[]) {
    // 检查是否以隐藏模式运行
    bool hiddenMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--hidden") {
            hiddenMode = true;
            break;
        }
    }

    if (hiddenMode) {
        // 隐藏控制台窗口，程序仍在后台正常运行
        HWND consoleWnd = GetConsoleWindow();
        if (consoleWnd != NULL) {
            ShowWindow(consoleWnd, SW_HIDE);
        }
    }

    // Windows 控制台默认代码页为 GBK（936），切换到 UTF-8（65001）解决中文乱码
    system("chcp 65001 > nul");

    // ==================== 数据文件路径（自动推导） ====================
    const std::string PROJECT_ROOT = getProjectRoot();
    const std::string DATA_DIR         = PROJECT_ROOT + "data/";
    const std::string STATION_FILE     = DATA_DIR + "Station.csv";
    const std::string EDGE_FILE        = DATA_DIR + "Edge.csv";
    const std::string INIT_FILE        = DATA_DIR + "Station_init.csv";
    const std::string UPDATE_FILE      = DATA_DIR + "update_station_status.csv";

    // ==================== 启动界面 ====================
    std::cout << "============================================" << std::endl;
    std::cout << "     上海地铁线路查询系统" << std::endl;
    std::cout << "     东华大学 · 数据结构课程设计" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  正在加载数据文件..." << std::endl;
    std::cout << "--------------------------------------------" << std::endl;

    // 预检查 data/ 目录是否存在
    {
        std::ifstream probe((DATA_DIR + "dummy").c_str());
        // 不关心结果，仅用于触发文件系统错误提示
    }

    // ==================== 1. 加载站点数据 ====================
    StationManager stationMgr;

    std::cout << "  [1/4] 加载站点数据  " << STATION_FILE << " ... ";
    try {
        if (!stationMgr.loadStation(STATION_FILE)) {
            std::cout << "失败" << std::endl;
            std::cerr << "\n[严重错误] 站点数据加载失败！" << std::endl;
            std::cerr << "  请检查以下内容：" << std::endl;
            std::cerr << "  1. data/ 文件夹是否存在" << std::endl;
            std::cerr << "  2. " << STATION_FILE << " 是否存在" << std::endl;
            std::cerr << "  3. CSV格式是否为: stationId,stationName,lineName,isOpen" << std::endl;
            std::cerr << "  按回车键退出...";
            std::cin.get();
            return 1;
        }
        std::cout << "完成（" << stationMgr.getAllStations().size() << " 个站点）" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "\n[严重错误] 读取站点CSV异常: " << e.what() << std::endl;
        std::cerr << "  按回车键退出...";
        std::cin.get();
        return 1;
    }

    // ==================== 2. 加载初始备份数据（用于恢复功能） ====================
    std::cout << "  [2/4] 加载初始备份  " << INIT_FILE << " ... ";
    {
        // 读取初始备份文件并暂存到 m_initialStations（内部调用）
        // 由于 StationManager 目前用 resetAllStation 做恢复，
        // 此处仅验证文件存在性，不覆盖主数据
        std::ifstream f(INIT_FILE.c_str());
        if (f.good()) {
            std::cout << "就绪" << std::endl;
        } else {
            std::cout << "不存在（恢复功能不可用）" << std::endl;
        }
        f.close();
    }

    // ==================== 3. 构建地铁图 ====================
    int stationCount = static_cast<int>(stationMgr.getAllStations().size());
    MetroGraph graph(stationCount);

    std::cout << "  [3/4] 加载边数据    " << EDGE_FILE << " ... ";
    try {
        if (!graph.loadEdge(EDGE_FILE)) {
            std::cout << "失败" << std::endl;
            std::cerr << "\n[严重错误] 边数据加载失败！" << std::endl;
            std::cerr << "  请检查以下内容：" << std::endl;
            std::cerr << "  1. " << EDGE_FILE << " 是否存在" << std::endl;
            std::cerr << "  2. CSV格式是否为: startId,endId,lineName,direction,time" << std::endl;
            std::cerr << "  按回车键退出...";
            std::cin.get();
            return 1;
        }
        std::cout << "完成" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "\n[严重错误] 读取边CSV异常: " << e.what() << std::endl;
        std::cerr << "  按回车键退出...";
        std::cin.get();
        return 1;
    }

    // ==================== 4. 预检查批量更新文件 ====================
    std::cout << "  [4/4] 检查更新文件  " << UPDATE_FILE << " ... ";
    {
        std::ifstream f(UPDATE_FILE.c_str());
        if (f.good()) {
            std::cout << "就绪" << std::endl;
        } else {
            std::cout << "不存在（批量更新功能受限）" << std::endl;
        }
        f.close();
    }

    // ==================== 初始化完成 ====================
    std::cout << "--------------------------------------------" << std::endl;
    std::cout << "  系统初始化完成！" << std::endl;
    std::cout << "  ● 站点: " << stationMgr.getAllStations().size() << " 个" << std::endl;
    std::cout << "  ● 线路: 已构建有向邻接表" << std::endl;
    std::cout << "  ● 换乘: 统一增加 5 分钟耗时" << std::endl;
    std::cout << "============================================" << std::endl;

    // ==================== 启动菜单交互 ====================
    PathFinder pathFinder(graph, stationMgr);
    MenuController menu(stationMgr, graph, pathFinder);
    menu.run();

    return 0;
}
