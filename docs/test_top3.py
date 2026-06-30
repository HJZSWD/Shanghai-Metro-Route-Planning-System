#!/usr/bin/env python
"""Test TOP3 transfer diversity and Century Avenue time display"""
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
    # Print result section
    lines = out.splitlines()
    capture = False
    for line in lines:
        if any(kw in line for kw in ['路径规划结果', '最短时间方案', '最少换乘方案',
                                      '方案1', '方案2', '方案3',
                                      '☰', '============']):
            capture = True
        if capture:
            stripped = line.rstrip()
            if stripped:
                print(stripped)
    print()

# === Test TOP3 transfer diversity: 陆家嘴 → 松江大学城 ===
# Flow: 主菜单1 → 子菜单4 → 陆家嘴→选1(2号线) → 松江大学城→选1(9号线) → 回车返回 → 5返回 → 3退出
run_test(
    "TOP3 Transfer: 陆家嘴(2号线) → 松江大学城(9号线)",
    '1\n4\n陆家嘴\n1\n松江大学城\n1\n\n5\n3\n'
)

# === Test Century Avenue time display (商城路9→上海科技馆2) ===
run_test(
    "Single: 商城路(9)→上海科技馆(2) 穿越世纪大道",
    '1\n1\n商城路\n上海科技馆\n\n5\n3\n'
)
