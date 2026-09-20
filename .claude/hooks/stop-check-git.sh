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
    git commit -m "自动提交：会话结束前保存工作进度" 2>/dev/null
    if [ $? -eq 0 ]; then
        echo "✅ 已自动提交"
    else
        echo "⚠️  提交失败，请手动处理"
    fi
    echo ""
fi
