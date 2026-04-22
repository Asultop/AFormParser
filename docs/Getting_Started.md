# 快速开始

本文档帮助你快速搭建 AFormParser SDK 开发环境并运行第一个示例。

---

## 环境要求

| 组件 | 要求 |
|------|------|
| CMake | >= 3.16 |
| Qt | 5.x 或 6.x (Core) |
| C++ 标准 | C++17 |
| 编译器 | GCC (MinGW), Clang, MSVC 2019+ |

---

## 构建步骤

### 1. 克隆项目

```bash
git clone <repository-url>
cd AFormParser
```

### 2. 配置 CMake

```bash
cmake -B build -G "Ninja"
```

或使用 Qt Creator 打开 `CMakeLists.txt`，CMake 会自动检测 Qt 版本。

### 3. 编译

```bash
cmake --build build
```

Qt Creator 用户：直接点击 "Build" 或按 `Ctrl+B`。

---

## 运行工具程序

构建完成后，可执行文件位于 `build/` 目录：

| 工具 | 路径 | 说明 |
|------|------|------|
| form_tree_viewer | `build/form_tree_viewer.exe` | GUI 可视化工具 |
| form_to_cfg | `build/form_to_cfg.exe` | 命令行导出 |
| legacy_to_form | `build/legacy_to_form.exe` | 格式转换 |
| verify_form_features | `build/verify_form_features.exe` | 功能验证 |

### form_to_cfg 用法

```bash
# 基本用法
./build/form_to_cfg.exe samples/Form.asul

# 指定输出目录
./build/form_to_cfg.exe samples/Form.asul output/

# 设置源文件路径（用于相对路径解析）
./build/form_to_cfg.exe input.asul --source=input.asul
```

### form_tree_viewer 用法

```bash
./build/form_tree_viewer.exe samples/Form.asul
```

---

## 作为 SDK 使用

### 添加为子项目

在你的 `CMakeLists.txt` 中：

```cmake
add_subdirectory(path/to/AFormParser)
target_link_libraries(your_target PRIVATE aformparser)
```

### 最小示例

```cpp
#include "AFormParser/AFormParser.hpp"
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    const QString formText = R"(
Form{
    .Id = "DemoForm"
    Group{
        .Title = "Demo"
        KeyBind{
            .Id = "Key1"
            .Description = "Test Key"
            .Command = "+test"
            .Bind("space")
        }
    }
}
)";

    AFormParser::ParseError err;
    auto doc = AFormParser::Document::from(formText, &err);
    if (!doc) {
        qWarning() << "Parse error at" << err.line << err.column << err.message;
        return 1;
    }

    qDebug().noquote() << doc->toCFG();
    return 0;
}
```

---

## 示例表单文件

项目提供多个示例文件位于 `samples/` 目录：

| 文件 | 说明 |
|------|------|
| `Form.asul` | 完整示例，包含所有控件类型 |
| `MForm.asul` | 多表单示例 |
| `Function_Preference.asul` | 函数和表达式示例 |
| `Key_Preference.asul` | 按键配置示例 |

---

## 下一步

- 了解 [ASUL 格式](ASUL_Format.md) 语法
- 学习 [架构设计](Architecture.md)
- 查看 [API 参考](API_Reference.md)