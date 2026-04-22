# AFormParser SDK

AFormParser 是一个基于 Qt 的 `.asul` 表单解析与导出 SDK，支持：

- 解析 Form 文本为文档对象树
- Dump 回文本格式
- 导出 CFG
- 解析并执行 `Scripts`（Lua）
- 内置多个工具程序用于转换、导出和可视化

## 项目结构

```text
AFormParser/
├─ docs/
│  └─ register_custom_function_and_global_values.md
├─ include/
│  └─ AFormParser/
│     ├─ AFormParser.hpp
│     └─ LuaRuntime.hpp
├─ src/
│  ├─ AFormParser.cpp
│  └─ LuaRuntime.cpp
├─ tools/
│  ├─ legacy_to_form/
│  │  └─ main.cpp
│  ├─ form_to_cfg/
│  │  └─ form_to_cfg.cpp
│  └─ form_tree_viewer/
│     └─ form_tree_viewer.cpp
├─ tests/
│  └─ verify_form_features.cpp
├─ samples/
│  ├─ Function_Preference.asul
│  ├─ Function_Preference_Form.asul
│  ├─ Function_Preference.cfg
│  ├─ Key_Preference.asul
│  ├─ Key_Preference.cfg
│  ├─ Form.asul
│  ├─ Test.asul
│  └─ tmp_user_case.asul
├─ thirdparty/
│  └─ lua/
└─ CMakeLists.txt
```

## 构建

要求：

- CMake >= 3.16
- Qt5 或 Qt6（Core，工具界面还需要 Widgets）
- 支持 C++17 的编译器

```powershell
cmake -S . -B build
cmake --build build
```

## 主要目标

- `aformparser`：静态库（SDK 核心）
- `legacy_to_form`：legacy `.asul` 转新 Form
- `form_to_cfg`：将 Form `.asul` 导出为 `.cfg`
- `form_tree_viewer`：Form 节点树可视化工具
- `verify_form_features`：功能回归验证

## 工具使用示例

### 1) legacy_to_form

```powershell
./build/legacy_to_form.exe samples/Function_Preference.asul
```

默认会在输入文件同目录生成 `*_Form.asul`。

### 2) form_to_cfg

```powershell
./build/form_to_cfg.exe samples/Function_Preference_Form.asul
```

可选第二个参数指定输出文件路径。

### 3) form_tree_viewer

```powershell
./build/form_tree_viewer.exe samples/Function_Preference_Form.asul
```

### 4) verify_form_features

```powershell
./build/verify_form_features.exe
```

## 作为 SDK 使用

文档：

- [register 自定义函数与全局值教程](docs/register_custom_function_and_global_values.md)

头文件入口：

- `AFormParser/AFormParser.hpp`
- `AFormParser/LuaRuntime.hpp`

最小示例：

```cpp
#include "AFormParser/AFormParser.hpp"

AFormParser::ParseError err;
auto doc = AFormParser::Document::from(formText, &err);
if (!doc) {
    // 处理 err.line / err.column / err.message
}
QString cfg = doc->toCFG();
```

## 说明

- Lua 运行时使用仓库内置源码（`thirdparty/lua`）。
- Windows 下工具运行依赖 Qt 运行时库，CMake 已对目标添加必要的拷贝逻辑。
