#!/bin/bash
# 内存/负载调速器 v2：空闲放行 / 紧张暂停 / 危急杀掉我方构建释放内存
W=/home/shaokai/Desktop/work/tx82
SENT=$W/tools/.builds-paused
LOG=$W/docs/compiler/GOVERNOR.log
PAUSE_LOAD=${PAUSE_LOAD:-8.0}; RESUME_LOAD=${RESUME_LOAD:-4.0}
PAUSE_MEM_MB=${PAUSE_MEM_MB:-2500}; KILL_MEM_MB=${KILL_MEM_MB:-1000}
OUR_NAMES="cc1plus cc1 clang-21 clang ld lld ninja cmake c++ g++ collect2 as lto1"
state=run
log(){ echo "[$(date '+%m-%d %H:%M:%S')] $*" >> $LOG; }
our_pids(){
  for n in $OUR_NAMES; do
    for p in $(pgrep -x "$n" 2>/dev/null); do
      cwd=$(readlink /proc/$p/cwd 2>/dev/null) || continue
      case "$cwd" in $W/*|$W) echo $p;; esac
    done
  done | sort -u
}
log "governor v2 启动 (pause load=$PAUSE_LOAD mem=${PAUSE_MEM_MB}MB / kill mem=${KILL_MEM_MB}MB)"
while true; do
  load=$(cut -d' ' -f1 /proc/loadavg); mem=$(awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo)
  if [ "$mem" -lt "$KILL_MEM_MB" ]; then
    n=0; for p in $(our_pids); do kill -KILL $p 2>/dev/null && n=$((n+1)); done
    touch $SENT; state=kill; log "KILL mem=${mem}MB<${KILL_MEM_MB}MB -> killed $n build procs"
  else
    busy=0
    awk -v l="$load" -v p="$PAUSE_LOAD" 'BEGIN{exit !(l>p)}' && busy=1
    [ "$mem" -lt "$PAUSE_MEM_MB" ] && busy=1
    if [ $busy = 1 ] && [ "$state" = run ]; then
      n=0; for p in $(our_pids); do kill -STOP $p 2>/dev/null && n=$((n+1)); done
      touch $SENT; state=pause; log "PAUSE load=$load mem=${mem}MB -> stopped $n procs"
    elif [ "$state" != run ]; then
      ok=1
      awk -v l="$load" -v r="$RESUME_LOAD" 'BEGIN{exit !(l<r)}' || ok=0
      [ "$mem" -lt "$PAUSE_MEM_MB" ] && ok=0
      if [ $ok = 1 ]; then
        n=0; for p in $(our_pids); do kill -CONT $p 2>/dev/null && n=$((n+1)); done
        rm -f $SENT; state=run; log "RESUME load=$load mem=${mem}MB -> continued $n procs"
      fi
    fi
  fi
  sleep 15
done
