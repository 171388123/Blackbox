# GitHub 仓库整理建议

本文档用于整理 Blackbox 项目的 GitHub 仓库结构、忽略规则、图片命名和提交建议。

---

## 1. 推荐目录结构

```text
Blackbox/
├── README.md
├── .gitignore
├── hc05_monitor.py
├── Project.uvprojx
├── docs/
│   ├── PROTOCOL.md
│   ├── HARDWARE.md
│   ├── DEBUG_NOTES.md
│   ├── RESUME.md
│   ├── GITHUB_GUIDE.md
│   └── images/
├── User/
├── Hardware/
├── Library/
├── Start/
└── System/
```

原则：

- README 放根目录。
- docs 放详细文档。
- Python 上位机可以放根目录，方便运行。
- Keil 工程目录不要为了美观大改，否则容易路径失效。

---

## 2. docs 目录说明

| 文件 | 作用 |
|---|---|
| `PROTOCOL.md` | 串口协议说明 |
| `HARDWARE.md` | 硬件接线与供电 |
| `DEBUG_NOTES.md` | 调试记录与困难复盘 |
| `RESUME.md` | 简历与面试表达 |
| `GITHUB_GUIDE.md` | 仓库整理建议 |
| `images/` | 放项目图片和截图 |

---

## 3. 图片命名建议

建议使用：

```text
hardware_overview.jpg
wiring_overview.jpg
pc_monitor_get.png
pc_monitor_evt.png
pc_monitor_last.png
flash_log_debug.png
```

不要使用：

```text
截图1.png
微信图片_20260422.png
新建图片.png
```

原因：

- 不专业。
- 不便于维护。
- GitHub 页面引用时不清晰。

---

## 4. README 图片引用示例

```markdown
![硬件整体图](docs/images/hardware_overview.jpg)

![Python 上位机状态显示](docs/images/pc_monitor_get.png)
```

---

## 5. 不建议提交的文件

Keil 常见中间文件：

```text
Objects/
Listings/
*.o
*.d
*.crf
*.axf
*.hex
*.map
*.lst
*.lnp
*.htm
*.build_log.htm
*.dep
```

Python 缓存：

```text
__pycache__/
*.pyc
```

系统文件：

```text
.DS_Store
Thumbs.db
```

---

## 6. 提交建议

### 第 1 次：README

```bash
git add README.md
git commit -m "docs: 完善项目 README"
```

### 第 2 次：docs 文档

```bash
git add docs/
git commit -m "docs: 补充协议硬件调试与简历文档"
```

### 第 3 次：.gitignore

```bash
git add .gitignore
git commit -m "chore: 添加 Keil 与 Python 忽略规则"
```

### 第 4 次：图片

```bash
git add docs/images/
git commit -m "docs: 添加项目运行截图"
```

---

## 7. 最后检查

提交前执行：

```bash
git status
```

推送前检查：

```bash
git log --oneline -5
```

推送：

```bash
git push origin main
```

GitHub 页面检查：

- README 第一屏是否清楚
- 图片是否能显示
- docs 链接是否能打开
- 是否误提交 Objects / Listings
- 是否包含无意义截图
- 项目描述是否没有过度夸大
