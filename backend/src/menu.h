#ifndef MENU_H
#define MENU_H

#include "station.h"
#include "graph.h"
#include "pathfinder.h"

/**
 * 菜单控制器类
 * 功能：负责控制台菜单交互，站点关键词模糊匹配，非法输入友好提示
 * 说明：严格二级菜单层级，所有交互通过此类调度
 */
class MenuController {
public:
    /**
     * 构造函数
     * @param stationMgr  站点管理器引用
     * @param graph       地铁图引用
     * @param pathFinder  路径查找器引用
     */
    MenuController(StationManager &stationMgr, MetroGraph &graph, PathFinder &pathFinder);

    /**
     * 启动主菜单循环
     * @note 程序入口，包含一级菜单和二级菜单的所有交互逻辑
     */
    void run();

private:
    StationManager &m_stationMgr;
    MetroGraph &m_graph;
    PathFinder &m_pathFinder;

    /**
     * 显示一级菜单（路径规划 / 站点运营管理 / 退出）
     * @return 用户选择的选项编号（1-3）
     */
    int showMainMenu() const;

    /**
     * 路径规划二级菜单（在路径规划界面内循环，直到用户选择返回）
     */
    void pathPlanningMenu();

    /**
     * 站点管理二级菜单（在站点管理界面内循环，直到用户选择返回）
     */
    void stationManagementMenu();

    /**
     * 通过关键词模糊匹配获取单个站点编号
     * @param prompt 提示文本（如"请输入起点站关键词"）
     * @return 用户选中的站点编号，-1表示返回上级
     */
    int selectStationByKeyword(const std::string &prompt);

    /**
     * 读取一个经过校验的整数输入
     * @param prompt 提示文本
     * @return 输入的整数，-1表示无效输入
     */
    int readIntInput(const std::string &prompt) const;

    /**
     * 等待用户按回车后继续
     */
    void waitForReturn() const;

    /**
     * 执行单条最短时间路径查询
     */
    void queryShortestTimePath();

    /**
     * 执行前3条最短时间路径查询
     */
    void queryTop3TimePaths();

    /**
     * 执行单条最少换乘路径查询
     */
    void queryMinTransferPath();

    /**
     * 执行前3条最少换乘路径查询
     */
    void queryTop3TransferPaths();

    /**
     * 执行CSV批量更新站点状态
     */
    void executeBatchUpdate();

    /**
     * 执行手动开关站点
     */
    void executeManualSetStatus();

    /**
     * 查看所有关闭站点
     */
    void executeShowClosedStations();

    /**
     * 执行恢复初始状态
     */
    void executeRestoreInit();

    /**
     * 执行按线路查询站点
     */
    void executeQueryLineStations();
};

#endif // MENU_H
