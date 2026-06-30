#!/usr/bin/env python
"""Test Bug 1 and Bug 2 fixes"""
import subprocess
import os

os.chdir(r'd:\Develop\Shanghai Metro Route Planning System')

def run_test(name, input_str):
    print(f"{'='*60}")
    print(f"  TEST: {name}")
    print(f"{'='*60}")
    proc = subprocess.run(
        [r'backend-service.exe'],
        input=input_str.encode('utf-8'),
        capture_output=True,
        timeout=15
    )
    out = proc.stdout.decode('utf-8', errors='replace')
    # Extract and print result section
    lines = out.splitlines()
    in_result = False
    result_lines = []
    for line in lines:
        if '路径规划结果' in line or '最短时间方案' in line:
            in_result = True
        if in_result:
            stripped = line.rstrip()
            if stripped:
                result_lines.append(stripped)
    if result_lines:
        for l in result_lines:
            print(l)
    else:
        # Fallback: show route output lines
        route_lines = [l for l in lines if any(kw in l for kw in [
            '路径规划结果', '起点', '终点', '总耗时', '换乘次数', '途经站台',
            '关闭站点', '换乘站点', '-->', '分钟', '方案', '☰',
            '============', '------------', '按回车'])]
        for l in route_lines:
            print(l.strip())
    print()


# === Test Bug 1: 四号线到世纪大道，目的地不应有站内换乘 ===
# 东安路(4号线) → 世纪大道作为目的地
run_test(
    "Bug 1: 东安路→世纪大道（4号线到目的地）",
    '1\n1\n东安路\n世纪大道\n\n5\n3\n'
)

# === Test Bug 2: 穿越世纪大道不应有多次换乘 ===
# 找个从浦东穿过世纪大道到其他地方的路线
run_test(
    "Bug 2: 穿越世纪大道的路径",
    '1\n2\n商城路\n杨树浦路\n\n5\n3\n'
)

# === Test normal: 直达不应有换乘 ===
run_test(
    "Normal: 徐家汇→衡山路（1号线直达）",
    '1\n1\n徐家汇\n衡山路\n\n5\n3\n'
)
