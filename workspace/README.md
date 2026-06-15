# embed-hello

ARM64 嵌入式 Hello World。

## 构建

```bash
make clean && make ARCH=arm64
```

## 测试

```bash
make test ARCH=arm64
```

## 部署

`git push gitea main` → Jenkins 自动构建 → 部署到鲁班猫 `/home/cat/deploy/`
