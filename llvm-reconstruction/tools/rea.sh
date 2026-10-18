#!/bin/bash
# REA 包装器：自动带好 JDK / Ghidra / Node。用法与 rea 官方 CLI 一致。
export JAVA_HOME=/home/shaokai/Desktop/work/tx82/tools/re/jdk
export GHIDRA_INSTALL_DIR=/home/shaokai/Desktop/work/tx82/tools/re/ghidra
export PATH=/home/shaokai/Desktop/work/tx82/tools/node/bin:$JAVA_HOME/bin:$PATH
exec /home/shaokai/Desktop/work/tx82/tools/node/bin/node \
     /home/shaokai/Desktop/work/rea/scripts/rea.mjs "$@"
