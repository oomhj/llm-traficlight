#!/bin/bash
# install-hooks.sh — 安装红绿灯 Claude Code hooks 到用户设置
#
# 将 PreToolUse / PostToolUse / PostToolUseFailure hooks
# 添加到 ~/.claude/settings.json（不覆盖已有配置）

set -e

CLAUDE_SETTINGS="$HOME/.claude/settings.json"
HOOK_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOOK_SCRIPT="$HOOK_SCRIPT_DIR/traflight-hook.sh"

# 确认钩子脚本存在
if [ ! -f "$HOOK_SCRIPT" ]; then
    echo "❌ Hook script not found at $HOOK_SCRIPT"
    exit 1
fi

# 确保 ~/.claude 目录存在
mkdir -p "$HOME/.claude"

# 读取已有配置或创建空对象
if [ -f "$CLAUDE_SETTINGS" ]; then
    EXISTING=$(cat "$CLAUDE_SETTINGS")
else
    EXISTING="{}"
fi

# 要注入的 hooks 配置
HOOKS_JSON=$(cat <<EOF
{
  "PreToolUse": [
    {
      "matcher": "Bash(*)",
      "hooks": [
        {
          "type": "command",
          "command": "bash $HOOK_SCRIPT before",
          "async": true,
          "timeout": 5
        }
      ]
    }
  ],
  "PostToolUse": [
    {
      "matcher": "Bash(*)",
      "hooks": [
        {
          "type": "command",
          "command": "bash $HOOK_SCRIPT success",
          "async": true,
          "timeout": 5
        }
      ]
    }
  ],
  "PostToolUseFailure": [
    {
      "matcher": "Bash(*)",
      "hooks": [
        {
          "type": "command",
          "command": "bash $HOOK_SCRIPT failure",
          "async": true,
          "timeout": 5
        }
      ]
    }
  ]
}
EOF
)

# 合并 hooks 到已有配置
MERGE=$(python3 -c "
import json, sys

existing = json.loads('''$EXISTING''')
hooks = json.loads('''$HOOKS_JSON''')

if 'hooks' not in existing:
    existing['hooks'] = {}
if 'PreToolUse' not in existing['hooks']:
    existing['hooks']['PreToolUse'] = []
if 'PostToolUse' not in existing['hooks']:
    existing['hooks']['PostToolUse'] = []
if 'PostToolUseFailure' not in existing['hooks']:
    existing['hooks']['PostToolUseFailure'] = []

# 去重: 检查是否已有相同 matcher+command 的 hook
def already_exists(hooks_list, new_hook):
    for h in hooks_list:
        if h.get('matcher') == new_hook.get('matcher'):
            for existing_sub in h.get('hooks', []):
                for new_sub in new_hook.get('hooks', []):
                    if existing_sub.get('command') == new_sub.get('command'):
                        return True
    return False

for event in ['PreToolUse', 'PostToolUse', 'PostToolUseFailure']:
    for hook_entry in hooks[event]:
        if not already_exists(existing['hooks'][event], hook_entry):
            existing['hooks'][event].append(hook_entry)

# 确保 permissions 里有钩子的 bash 命令
PERMS = [
    'Bash(bash $HOOK_SCRIPT before)',
    'Bash(bash $HOOK_SCRIPT success)',
    'Bash(bash $HOOK_SCRIPT failure)',
]
if 'permissions' not in existing:
    existing['permissions'] = {}
if 'allow' not in existing['permissions']:
    existing['permissions']['allow'] = []
for p in PERMS:
    resolved = p.replace('\$HOOK_SCRIPT', '$HOOK_SCRIPT')
    if resolved not in existing['permissions']['allow']:
        existing['permissions']['allow'].append(resolved)

with open('$CLAUDE_SETTINGS', 'w') as f:
    json.dump(existing, f, indent=2)
    f.write('\n')

print('✅ Hooks installed to $CLAUDE_SETTINGS')
")

eval "$MERGE"
