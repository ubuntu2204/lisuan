#!/usr/bin/env bash
# 历史入口：Deepin Wine 8.16 + 1.56.6 链路已停用。
#
# 保留这个文件，是为了让任何残留的旧快捷方式也能进 Wine 11 主线
# （官方 2.32.6 + Steam Runtime 容器），而不是误入旧前缀触发
# `win_util.cc(544)` FATAL 或累积 running_exception 后弹出
# 「当前版本启动发生异常，是否进行修复？」的豆包修复程序。
#
# 不再运行 fix-doubao-wine.sh、不再注入旧 Mesa 环境、不再调用 deb run.sh。
# 需要做 1.56.6 历史对照时，直接用 deepin-wine8-stable 手动启动旧前缀。
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

# 旧桌面入口在普通启动时会留下裸 --uri，令客户端进入修复流程。
if [[ $# -eq 1 && ${1:-} == '--uri' ]]; then
    set --
fi

echo "旧豆包入口已停用，转交 Wine 11 启动器（官方 2.32.6）。" >&2
exec "$SCRIPT_DIR/run-doubao-wine11.sh" "$@"
