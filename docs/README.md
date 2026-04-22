# AFormParser SDK 文档

AFormParser 是一个基于 Qt 的 `.asul` 表单解析与导出 SDK，支持解析表单配置、模板求值、Lua 脚本执行和多表单导出。

---

## 核心特性

- **表单解析**：将 `.asul` 格式文本解析为结构化文档对象
- **模板引擎**：支持 `${}` 插值、`$(Id).Property` 引用、三元表达式
- **Lua 脚本**：内置 Lua 运行时，支持在表单中定义函数
- **多表单导出**：`toCFGs()` 支持一个文件定义多个表单，输出到不同文件
- **可视化工具**：`form_tree_viewer` 提供图形化表单查看和编辑

---

## 快速导航

### 新手入门
- [快速开始](Getting_Started.md) - 环境搭建与最小示例
- [ASUL 格式](ASUL_Format.md) - 表单文件语法入门
- [工具使用](Tools.md) - 命令行工具简介

### 核心概念
- [架构设计](Architecture.md) - 系统架构与设计理念
- [节点类型](Node_Types.md) - 表单节点树结构
- [元属性](Meta_Attributes.md) - @Import 等元数据
- [表达式与模板](Expression_and_Templates.md) - 模板语法详解

### 高级功能
- [Lua 脚本](Lua_Scripts.md) - Scripts 块与 Lua 集成
- [函数注册](Register_Functions.md) - C++ 函数扩展
- [多表单输出](Form_Output.md) - toCFGs 与路径解析

### 参考
- [API 参考](API_Reference.md) - 完整 API 手册

---

## 最小示例

```cpp
#include "AFormParser/AFormParser.hpp"

int main() {
    const QString formText = R"(
Form{
    .Id = "TestForm"
    .Output = "output/Test"
    Group{
        .Title = "Settings"
        KeyBind{
            .Id = "AttackKey"
            .Description = "Attack"
            .Command = "+attack"
            .Bind("mouse1")
        }
    }
}
)";

    AFormParser::ParseError err;
    auto doc = AFormParser::Document::from(formText, &err);
    if (!doc) {
        qWarning() << "Parse failed:" << err.message;
        return 1;
    }

    // 导出为 CFG
    const QString cfg = doc->toCFG();
    qDebug().noquote() << cfg;

    // 多表单支持
    auto items = doc->toCFGs();
    for (const auto &item : items) {
        qDebug() << "Output:" << item.fileName;
    }

    return 0;
}
```

---

## 项目结构

```
AFormParser/
├── docs/                    # SDK 文档
│   ├── README.md           # 本文档
│   ├── Getting_Started.md
│   ├── Architecture.md
│   ├── Node_Types.md
│   ├── Meta_Attributes.md
│   ├── Expression_and_Templates.md
│   ├── Lua_Scripts.md
│   ├── Register_Functions.md
│   ├── Form_Output.md
│   ├── API_Reference.md
│   ├── Tools.md
│   └── ASUL_Format.md
├── include/AFormParser/
│   ├── AFormParser.hpp     # 核心解析 API
│   └── LuaRuntime.hpp      # Lua 运行时
├── src/
│   ├── AFormParser.cpp
│   └── LuaRuntime.cpp
├── tools/
│   ├── form_tree_viewer/   # 可视化工具
│   ├── form_to_cfg/        # 命令行导出
│   ├── legacy_to_form/      # 格式转换
│   └── register_function_demo/
├── samples/                 # 示例表单
└── CMakeLists.txt
```

---

## 版本要求

- **CMake**: >= 3.16
- **Qt**: 5.x 或 6.x (Core)
- **C++**: 17
- **编译器**: GCC, Clang, MSVC

详见 [快速开始](Getting_Started.md)。

---

## 许可与贡献

AFormParser SDK 使用 MIT 许可证。详细文档正在完善中。