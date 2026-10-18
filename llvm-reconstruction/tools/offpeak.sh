#!/bin/bash
# DeepSeek 计费时段闸门：空闲时段(半价) exit 0；高峰时段 exit 1。
# 规则（官方）：北京时间周一至周五（不含法定节假日）9:00-12:00、14:00-18:00 = 高峰；其余 = 空闲。
# 用法: offpeak.sh [--quiet]   节假日可写进 tools/cn_holidays.txt（每行 YYYY-MM-DD，含调休放假）
HOL=/home/shaokai/Desktop/work/tx82/tools/cn_holidays.txt
python3 - "$HOL" <<'PY'
import sys, datetime as dt
from zoneinfo import ZoneInfo
hol = set()
try:
    for ln in open(sys.argv[1]):
        ln=ln.strip()
        if ln and not ln.startswith('#'): hol.add(ln)
except FileNotFoundError: pass
now = dt.datetime.now(ZoneInfo("Asia/Shanghai"))
wd, hm = now.weekday(), now.hour*60+now.minute          # 周一=0
peak = (wd < 5) and (now.strftime('%Y-%m-%d') not in hol) and ((540 <= hm < 720) or (840 <= hm < 1080))
def nxt():
    t = now.replace(second=0, microsecond=0)
    for _ in range(8*24*60):                              # 逐分钟找下一个切换点
        t += dt.timedelta(minutes=1)
        w, h = t.weekday(), t.hour*60+t.minute
        p = (w < 5) and (t.strftime('%Y-%m-%d') not in hol) and ((540 <= h < 720) or (840 <= h < 1080))
        if p != peak: return t
m = nxt() - now
print(f"  {'高峰时段（全价）' if peak else '空闲时段（半价）'}  现在 {now.strftime('%m-%d %a %H:%M')} CST"
      f"  距切换 {m.seconds//3600}h{m.seconds%3600//60:02d}m → {m and ''}{nxt().strftime('%m-%d %H:%M')}")
sys.exit(1 if peak else 0)
PY
