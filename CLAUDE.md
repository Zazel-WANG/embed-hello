# embed-hello

> ⚠️ **核心规则（最高优先级，不可跳过）**
>
> 每次完成用户任务后，必须在返回结果前检查是否有新知识需要写入 `skills/` 对应 SKILL.md。
> 更新后必须在回复末尾报告。

本文件为 Claude Code 提供项目上下文和操作指南。

## 用户背景

详见全局 Skill `user-profile`（SessionStart hook 自动加载）。

## 项目概述

- **名称**：embed-hello
- **领域**：embedded-basics
- **描述**：ARM64 嵌入式 Hello World，用于学习 CI/CD 辅助下的嵌入式开发流程和自动化工具链
- **关联 Skill 域**：[embedded-cross-compile, ci-cd-pipeline, deployment, shell-safety]
- **输出**：ARM64 二进制 + Makefile + Jenkinsfile
- **团队**：个人
- **创建日期**：2026.6.15

## 硬件环境

- 开发板：鲁班猫 RK3588（ARM64，8核，NPU）— 192.168.137.100
- 笔记本：Windows 11（开发 + 部署中转）
- 主机：Ubuntu Server 22.04（Docker + Gitea + Jenkins）— 10.0.0.1

## 🛡️ 安全规则（最高优先级，不可覆盖）

> 这些规则通过 SessionStart hook 自动注入，此处再次强调。

1. **NOT-FOUND 熔断** — 工具返回 NOT FOUND → 停手确认
2. **观测矛盾停手** — 两个以上观测通道矛盾 → 停手复核
3. **写前必读** — 向 SKILL.md 写入前必读当前文件
4. **审计用 node** — 读 JSONL 必须逐行 JSON.parse

## 目录结构

```
embed-hello/
├── README.md
├── CLAUDE.md                  # 本文件
├── CHANGELOG.md
├── .gitignore
├── Jenkinsfile                # CI/CD 流水线（根目录）
├── .claude/settings.json      # Hook 配置
├── .hot/recovery.md           # 会话接力卡
├── references/                # 参考资料
└── workspace/                 # 源代码
    ├── Makefile
    ├── src/main.c
    ├── include/hello.h
    └── tests/test_main.c
```

## Skills 系统

**全局 Skill 池**：`E:\AI-helper\skills\` —— 所有项目共享。
**本项目关联的 Skill 域**：[embedded-cross-compile, ci-cd-pipeline, deployment, shell-safety]

### AI 检索流程

1. **SessionStart** — hook 自动注入 GLOBAL_RULES + 项目上下文 + 会话恢复
2. **UserPromptSubmit** — hook 自动扫描全局 skills/，注入匹配索引
3. **AI 主动检索** — AI 看到索引后自行 Read 相关 SKILL.md
4. **AI 自动归档** — 每次回答末尾反思 → 更新 Skill → 报告用户

## 会话恢复

SessionStart hook 自动读取 `.hot/recovery.md`。关闭时 Stop hook 自动覆写。

## 冷记忆（COLD）

遇到终结性结论（事故根因、架构决策、可迁移原则）时，AI 提议、用户审批后写入 `E:\AI-helper\memory\COLD\`。
文件名格式：`{YYYY-MM-DD}--{项目名}--{标题}.md`

## 关键配置速查

| 项目 | 值 |
|------|-----|
| 鲁班猫 IP | 192.168.137.100 |
| 主机 IP | 10.0.0.1 |
| Gitea | http://10.0.0.1:3000 |
| Jenkins | http://10.0.0.1:8080 |
| 主机 SSH 用户 | `embedsys`@10.0.0.1 |
| 鲁班猫 SSH 用户 | `root`@192.168.137.100 |
| 交叉编译 | `aarch64-linux-gnu-gcc -static` |
| 主分支 | `main` |

### Git Remote

| Remote | URL | 用途 |
|--------|-----|------|
| `origin` | `git@github.com:Zazel-WANG/embed-hello.git` | GitHub public 面试展示 |
| `gitea` | `ssh://git@10.0.0.1:2222/wangzhongqi/embed-hello.git` | Gitea 触发 Jenkins CI/CD |

## 工作规范

- **语言**：中文
- **CI/CD 原则**：可复现、可回滚
- **变更记录**：重要变更记录到 CHANGELOG.md
