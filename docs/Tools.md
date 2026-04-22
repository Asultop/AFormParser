# 工具程序

本文档介绍 AFormParser 提供的命令行工具和可视化工具。

---

## 工具列表

| 工具 | 类型 | 说明 |
|------|------|------|
| form_tree_viewer | GUI | 图形化表单查看和编辑 |
| form_to_cfg | CLI | 命令行 CFG 导出 |
| legacy_to_form | CLI | 旧格式转换 |
| verify_form_features | CLI | 功能回归测试 |
| register_function_demo | CLI | 函数注册演示 |

---

## form_tree_viewer

图形化表单查看和编辑工具。

### 界面布局

```
┌─────────────────────────────────────────────────────────────┐
│  [文件路径输入]  [浏览]  [重新加载]                           │
├─────────────────────────┬───────────────────────────────────┤
│                         │  [Form 选择下拉框]                   │
│    树视图               │  ┌─ Dump (Form) ─────────────┐   │
│                         │  │ [DumpAll]                  │   │
│    Form                  │  │ [Dump 内容...]             │   │
│      Group               │  └────────────────────────────┘   │
│        KeyBind          │  ┌─ toCFGs ──────────────────┐   │
│        TextField         │  │ [Output 路径]             │   │
│                         │  │ [CFG 内容...]              │   │
│                         │  └────────────────────────────┘   │
├─────────────────────────┴───────────────────────────────────┤
│  状态栏                                                    │
└─────────────────────────────────────────────────────────────┘
```

### 功能

1. **文件加载** - 输入路径或浏览选择 .asul 文件
2. **树视图** - 显示表单节点树，可编辑属性
3. **Form 选择** - 多表单时下拉选择
4. **Dump** - 显示当前选中 Form 的 dump 内容
5. **DumpAll** - 点击按钮弹窗显示完整 Document dump
6. **toCFGs** - 显示当前 Form 的 CFG 导出内容和 Output 路径

### 使用方法

```bash
./build/form_tree_viewer.exe
```

或指定文件：

```bash
./build/form_tree_viewer.exe samples/Form.asul
./build/form_tree_viewer.exe samples/MForm.asul
```

### 依赖

需要 Qt Widgets 模块。Windows 下会自动部署所需 DLL。

---

## form_to_cfg

命令行 CFG 导出工具。

### 基本用法

```bash
# 基本导出
./build/form_to_cfg.exe <input.asul>

# 指定输出目录
./build/form_to_cfg.exe <input.asul> <output_dir>

# 设置源文件路径
./build/form_to_cfg.exe input.asul --source=input.asul
```

### 参数说明

| 参数 | 说明 |
|------|------|
| `<input.asul>` | 输入的 .asul 文件路径 |
| `<output_dir>` | 可选，输出目录 |
| `--source=<path>` | 设置源文件路径，用于相对路径解析 |

### 示例

```bash
# 导出到输入文件同目录
./build/form_to_cfg.exe samples/Form.asul

# 导出到指定目录
./build/form_to_cfg.exe samples/MForm.asul output/

# 指定源路径
./build/form_to_cfg.exe input.asul --source=input.asul
```

### 输出

```
Written: output/scripts/KeyBindForm.cfg
Written: output/scripts/TextInputForm.cfg
Written: D:/export/AutoCompleteForm.cfg

Generated 3 of 3 CFG files
```

---

## legacy_to_form

旧格式表单转换工具。

### 基本用法

```bash
./build/legacy_to_form.exe <input.asul>
```

### 示例

```bash
./build/legacy_to_form.exe samples/Function_Preference.asul
```

### 输出

在输入文件同目录生成 `*_Form.asul` 文件。

---

## verify_form_features

功能回归验证工具。

### 基本用法

```bash
./build/verify_form_features.exe
```

### 测试内容

- 基础解析功能
- 节点类型转换
- CFG 导出
- 多表单支持
- Lua 脚本执行
- 元属性解析

### 输出示例

```
=== AFormParser 功能验证 ===
[OK] Document 解析成功
[OK] Form 节点存在
[OK] Group 节点存在
[OK] KeyBind 节点存在
...
=== 验证完成 ===
```

---

## register_function_demo

C++ 函数注册功能演示。

### 基本用法

```bash
./build/register_function_demo.exe
```

### 功能演示

1. **自定义函数注册** - 演示 `registerFunction`
2. **全局变量** - 演示 `registerGlobalVariable`
3. **批量设置** - 演示 `setGlobalVariables`
4. **模板中使用** - 演示在表达式中调用注册函数

---

## 工具依赖

| 工具 | Qt 模块 |
|------|----------|
| form_tree_viewer | Core, Widgets |
| form_to_cfg | Core |
| legacy_to_form | Core |
| verify_form_features | Core |
| register_function_demo | Core |

---

## Windows 部署

Qt Creator 或 CMake 构建时会自动：

1. 复制 Qt 运行时 DLL 到工具目录
2. 对 form_tree_viewer 运行 `windeployqt`

### 手动部署

```bash
# 复制 Qt 运行时
windeployqt --no-translations build/form_tree_viewer.exe

# 或使用 CMake
cmake --install . --prefix deploy
```

---

## 工具源码位置

| 工具 | 源码目录 |
|------|----------|
| form_tree_viewer | `tools/form_tree_viewer/` |
| form_to_cfg | `tools/form_to_cfg/` |
| legacy_to_form | `tools/legacy_to_form/` |
| verify_form_features | `tests/verify_form_features.cpp` |
| register_function_demo | `tools/register_function_demo/` |