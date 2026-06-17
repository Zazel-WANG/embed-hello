# 变更日志

## 2026-06-15 — 项目初始化
- 创建项目骨架（解耦架构 + 全局 Skill 池）
- 配置 hook 护栏
- GitHub + Gitea 双 remote
- Jenkins Multibranch Pipeline（embed-hello）
- Hello World ARM64 交叉编译程序

## 2026-06-17 — AI CI/CD 全链路打通 + 摄像头对焦诊断

### CI/CD 基础设施修复（#1-#4）
- **#1 Docker CLI 持久化**：`docker-bin/docker` 静态二进制 COPY 进 `Jenkins.Dockerfile`，不再依赖有问题的 `docker.io` apt 包
- **#4 Docker Pipeline 插件**：安装 `docker-workflow` 插件 → Jenkinsfile 用 `agent { docker }` 替代三层引号转义的 `sh + docker run`
- **#3 AI 冒烟测试**：每次部署后自动 SSH 到板跑 `ai-query-cross`，检测 `SDK:` 输出判定 NPU 正常，SIGSEGV/SIGABRT 判定崩溃

### 摄像头对焦诊断 — 根因与修复
- **症状**：画面"像近焦拍远景"，远景模糊但近处清楚
- **根因链**：
  1. 传感器确认：Sony IMX415 (MIPI CSI)，非 USB UVC 摄像头
  2. V4L2 控件为空（只有只读 pixel_rate）→ 控制不走 V4L2
  3. 真正控制通路：Media Controller → RKAIQ 3A 引擎 (`rkaiq_3A_server`) → IQ 调优文件
  4. IQ 文件 `disable_algos: ["DISABLE_AF"]` — AF 被故意关闭
  5. 尝试打开 AF → 3A 管线崩溃（曝光/白平衡/颜色全乱）→ 立即回滚
  6. **最终根因**：镜头是 M12 螺纹固定焦距，非 VCM 电控。出厂时镜头未拧到无穷远位
- **修复**：手动旋转 M12 镜头至远景清晰 ✅
- **关键教训**：MIPI CSI 摄像头在 Rockchip 平台上所有控制都走 rkaiq IQ 文件，V4L2 controls 不可用。IQ 文件的 disable 配置是有意的——不要轻易改

### 摄像头硬件信息
| 项目 | 值 |
|------|-----|
| 传感器 | Sony IMX415 (MIPI CSI, 4 线) |
| 分辨率 | 3840×2160 (NV12) |
| 镜头 | M12 螺纹固定焦距, F/1.6, EFL 5.2mm |
| 对焦 | **手动**（旋转螺纹），无 VCM/自动对焦 |
| ISP | Rockchip rkisp_v6 |
| 3A 引擎 | rkaiq_3A_server (AIQ v5.0x1.3) |
| IQ 文件 | `/etc/iqfiles/imx415_CMK-OT2022-PX1_IR0147-50IRC-8M-F20.json` |
