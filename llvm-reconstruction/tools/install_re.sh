#!/bin/bash
# 无 root 安装 RE 工具：Temurin JDK 21 + Ghidra headless（放进工作区，不污染系统）
set -u
R=/home/shaokai/Desktop/work/tx82/tools/re
LOG=$R/install.log
cd $R
echo "=== [$(date -Is)] 开始 ===" >> $LOG
# 1) JDK 21（Temurin）
if [ ! -d $R/jdk ]; then
  echo "--- 下载 JDK 21" >> $LOG
  curl -sL -o jdk.tar.gz "https://api.adoptium.net/v3/binary/latest/21/ga/linux/x64/jdk/hotspot/normal/eclipse" >> $LOG 2>&1
  ls -la jdk.tar.gz >> $LOG 2>&1
  mkdir -p jdk && tar xzf jdk.tar.gz -C jdk --strip-components=1 >> $LOG 2>&1 && echo "jdk ok" >> $LOG
fi
export JAVA_HOME=$R/jdk; export PATH=$JAVA_HOME/bin:$PATH
java -version >> $LOG 2>&1
# 2) Ghidra 最新 release
if [ ! -d $R/ghidra ]; then
  URL=$(curl -s https://api.github.com/repos/NationalSecurityAgency/ghidra/releases/latest | python3 -c "import sys,json;d=json.load(sys.stdin);print([a['browser_download_url'] for a in d['assets'] if a['name'].endswith('.zip')][0])" 2>>$LOG)
  echo "--- Ghidra URL: $URL" >> $LOG
  curl -sL -o ghidra.zip "$URL" >> $LOG 2>&1; ls -la ghidra.zip >> $LOG 2>&1
  mkdir -p ghidra && unzip -q ghidra.zip -d ghidra >> $LOG 2>&1
  # 展平
  inner=$(find $R/ghidra -maxdepth 1 -mindepth 1 -type d | head -1)
  [ -n "$inner" ] && ( shopt -s dotglob; mv "$inner"/* $R/ghidra/ 2>/dev/null; rmdir "$inner" 2>/dev/null )
  echo "ghidra ok" >> $LOG
fi
ls $R/ghidra/support/analyzeHeadless >> $LOG 2>&1 && echo "analyzeHeadless 可用" >> $LOG
echo "=== [$(date -Is)] 结束 ===" >> $LOG
