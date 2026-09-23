#!/bin/bash
# Stop hook: auto-commit uncommitted changes at session end
# Project-local fallback for 物联网创客项目 (global stop-commit.sh also runs)

PROJECT_ROOT=$(git rev-parse --show-toplevel 2>/dev/null)
if [ -z "$PROJECT_ROOT" ]; then
    echo "⚠️  Not in a git repository"
    exit 0
fi

cd "$PROJECT_ROOT"

# Filter out noise files
changes=$(git status --porcelain 2>/dev/null | grep -v '^?? .omc/' | grep -v '^??.*~\$' | grep -v '^.omc/' | grep -v '^.claude/settings.local.json')

if [ -n "$changes" ]; then
    echo ""
    echo "📦 自动提交未提交的变更..."
    git add -A 2>/dev/null
    git reset HEAD .omc/ design/.omc/ 2>/dev/null
    git reset HEAD '~$*' 2>/dev/null

    # ── 自动提交绝不记录「删除」────────────────────────────
    # 有人（AI 或用户）可能刻意 git restore --staged 把某个删除撤出暂存，
    # 意思是「这个删除先别提交，等确认」。无脑 git add -A 会把那次撤回
    # 抵消掉、删除被悄悄提交。2026-09-23 实测发生过两次。
    DELETED=$(git -c core.quotepath=false diff --cached --diff-filter=D --name-only 2>/dev/null)
    if [ -n "$DELETED" ]; then
        git diff --cached --diff-filter=D --name-only -z 2>/dev/null \
            | xargs -0 -r git restore --staged -- 2>/dev/null
        echo "ℹ️  跳过未提交的删除（自动提交不记录删除，需人工确认）："
        echo "$DELETED" | sed 's/^/     /'
    fi

    if [ -z "$(git diff --cached --name-only 2>/dev/null)" ]; then
        echo "     （暂无新增/修改，本次不提交）"
    else
        git commit -m "自动提交：会话结束前保存工作进度" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "✅ 已自动提交"
        else
            echo "⚠️  提交失败，请手动处理"
        fi
    fi
    echo ""
fi
