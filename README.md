# embed-hello

ARM64 嵌入式 Hello World 项目——学习 CI/CD 自动化开发流程。

## 技术栈

- **目标平台**：鲁班猫 RK3588（ARM64, aarch64）
- **交叉编译**：`aarch64-linux-gnu-gcc -static`
- **CI/CD**：Jenkins Multibranch Pipeline + Gitea + Docker
- **测试**：QEMU user mode 模拟 ARM64 环境

## 项目结构

```
├── Jenkinsfile      ← CI/CD 流水线定义
├── CLAUDE.md        ← AI 助手项目上下文
└── workspace/       ← 源代码和构建文件
    ├── Makefile
    ├── src/
    ├── include/
    └── tests/
```

## 快速开始

```bash
# 本地交叉编译
cd workspace && make clean && make ARCH=arm64

# 运行测试
make test ARCH=arm64

# 推送触发 CI/CD
git push gitea main
```
