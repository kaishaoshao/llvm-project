#!/bin/bash
# 采样 DeepSeek 余额并记账：tools/costlog.sh [备注]
L=/home/shaokai/Desktop/work/tx82/docs/compiler/COST_LOG.tsv
K=$(python3 -c "import re;s=open(\"/home/shaokai/.dsh/.credentials.yaml\").read();m=re.search(r\"^\\s*DEEPSEEK_API_KEY:\\s*(\\S+)\\s*$\",s,re.M);print(m.group(1).strip(chr(34)) if m else \"\")" 2>/dev/null)
B=$( [ -n "$K" ] && curl -s --max-time 15 https://api.deepseek.com/user/balance -H "Authorization: Bearer $K" | python3 -c "import sys,json;d=json.load(sys.stdin);print(d[\"balance_infos\"][0][\"total_balance\"])" 2>/dev/null )
[ -z "$B" ] && B=$(awk '{print $1}' ~/.dsh/.balance-last 2>/dev/null)
T=$(date +%s)
TS=$( [ -n "$T" ] && date -d @$T '+%m-%d %H:%M' || date '+%m-%d %H:%M')
[ ! -f "$L" ] && printf 'timestamp\tbalance_yuan\tnote\n' > "$L"
prev=$(tail -1 "$L" | cut -f2)
delta=""
case "$prev" in (*[!0-9.]*|"") ;; (*) delta=$(awk -v a="$prev" -v b="$B" 'BEGIN{printf "%.2f", a-b}');; esac
printf '%s\t%s\t%s%s\n' "$TS" "$B" "${1:-}" "${delta:+  (Δ -¥$delta)}" >> "$L"
tail -3 "$L" | sed 's/^/  /'
