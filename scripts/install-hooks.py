#!/usr/bin/env python3
"""install-hooks.py — 安装红绿灯 Claude Code hooks 到用户设置

将 PreToolUse / PostToolUse / PostToolUseFailure hooks
合并到 ~/.claude/settings.json，不覆盖已有配置。

用法:
    python3 scripts/install-hooks.py
"""

import json
import os
import sys

CLAUDE_SETTINGS = os.path.expanduser("~/.claude/settings.json")
HOOK_SCRIPT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), "..", "traflight-hook.sh"
))

HOOKS = {
    "PermissionRequest": [
        {
            "matcher": "Bash(*)",
            "hooks": [
                {"type": "command", "command": f"bash {HOOK_SCRIPT} permission",
                 "async": True, "timeout": 5}
            ],
        }
    ],
    "PermissionDenied": [
        {
            "matcher": "Bash(*)",
            "hooks": [
                {"type": "command", "command": f"bash {HOOK_SCRIPT} denied",
                 "async": True, "timeout": 5}
            ],
        }
    ],
}

PERMISSIONS = [
    f"Bash(bash {HOOK_SCRIPT} before)",
    f"Bash(bash {HOOK_SCRIPT} success)",
    f"Bash(bash {HOOK_SCRIPT} failure)",
    f"Bash(bash {HOOK_SCRIPT} permission)",
    f"Bash(bash {HOOK_SCRIPT} denied)",
]


def main():
    # 确保 hook 脚本存在
    if not os.path.exists(HOOK_SCRIPT):
        print(f"❌ Hook script not found: {HOOK_SCRIPT}")
        sys.exit(1)

    # 确保目录存在
    os.makedirs(os.path.dirname(CLAUDE_SETTINGS), exist_ok=True)

    # 读取已有配置
    if os.path.exists(CLAUDE_SETTINGS):
        with open(CLAUDE_SETTINGS) as f:
            config = json.load(f)
    else:
        config = {}

    # 合并 hooks
    if "hooks" not in config:
        config["hooks"] = {}

    for event, entries in HOOKS.items():
        if event not in config["hooks"]:
            config["hooks"][event] = []
        for entry in entries:
            # 去重: 检查是否已有相同 matcher 的 hook 条目
            existing_matchers = [
                e.get("matcher") for e in config["hooks"][event]
            ]
            if entry["matcher"] not in existing_matchers:
                config["hooks"][event].append(entry)
                print(f"  + Added {event} hook ({entry['matcher']})")

    # 合并 permissions
    if "permissions" not in config:
        config["permissions"] = {}
    if "allow" not in config["permissions"]:
        config["permissions"]["allow"] = []

    for perm in PERMISSIONS:
        if perm not in config["permissions"]["allow"]:
            config["permissions"]["allow"].append(perm)
            print(f"  + Added permission: {perm}")

    # 写入
    with open(CLAUDE_SETTINGS, "w") as f:
        json.dump(config, f, indent=2)
        f.write("\n")

    print(f"✅ Hooks installed to {CLAUDE_SETTINGS}")


if __name__ == "__main__":
    main()
