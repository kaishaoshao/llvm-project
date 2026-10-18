#!/bin/bash
# 预算闸门：余额低于 (基线-100+10) 时落 tools/.budget-stop，并记日志。
W=/home/shaokai/Desktop/work/tx82
BASE=$(cat $W/tools/.budget-baseline 2>/dev/null || echo 0)
STOP=$(python3 -c "print(f'{float('$BASE')-90:.2f}')" 2>/dev/null)
LOG=$W/docs/compiler/COST_LOG.tsv
while true; do
  K=$(python3 -c "import re;s=open('/home/shaokai/.dsh/.credentials.yaml').read();m=re.search(r'^\s*DEEPSEEK_API_KEY:\s*(\S+)\s*\$',s,re.M);print(m.group(1).strip(chr(34)) if m else '')" 2>/dev/null)
  B=$(curl -s --max-time 15 https://api.deepseek.com/user/balance -H "Authorization: Bearer $K" | python3 -c "import sys,json;print(json.load(sys.stdin)['balance_infos'][0]['total_balance'])" 2>/dev/null)
  if [ -n "$B" ]; then
    printf '%s\t%s\tbudget_guard  (spent %.2f of 100)\n' "$(date '+%m-%d %H:%M')" "$B" "$(python3 -c "print(float('$BASE')-float('$B'))")" >> $LOG
    if python3 -c "import sys;sys.exit(0 if float('$B') <= float('$STOP') else 1)"; then
      touch $W/tools/.budget-stop
      echo "[$(date '+%m-%d %H:%M')] BUDGET-STOP 余额 ¥$B ≤ ¥$STOP（已花满 ¥90/100）" >> $LOG
    fi
  fi
  sleep 300
done
