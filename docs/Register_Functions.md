# 函数注册

本文档介绍如何在 C++ 中注册自定义函数和全局变量。

---

## 概述

AFormParser 支持通过 C++ 扩展功能：

- **注册自定义函数** - 在 C++ 中定义函数，在表达式中调用
- **注册全局变量** - 注入可在模板中引用的变量
- **设置批量全局变量** - 一次注入多个变量

---

## LuaFunction 类型

所有注册函数都使用 `LuaFunction` 类型：

```cpp
namespace AFormParser {
    using LuaFunction = std::function<QString(QStringList)>;
}
```

函数签名：
- **输入**: `QStringList` - 参数字符串列表
- **输出**: `QString` - 函数返回值

---

## 注册自定义函数

### 基本用法

```cpp
doc->registerFunction("FunctionName", [](const QStringList &args) -> QString {
    // 处理参数
    QString arg1 = args.value(0);
    QString arg2 = args.value(1);

    // 返回结果
    return result;
});
```

### 示例：字符串处理

```cpp
doc->registerFunction("JoinWithDash", [](const QStringList &args) -> QString {
    if (args.size() < 2) {
        return "Error: need 2 args";
    }
    return args.at(0) + "-" + args.at(1);
});
```

使用：

```asul
LineField{
    .Expression = $JoinWithDash("Hello", "World")
}
```

输出: `Hello-World`

### 示例：数值计算

```cpp
doc->registerFunction("Add", [](const QStringList &args) -> QString {
    double sum = 0;
    for (const QString &arg : args) {
        bool ok;
        double val = arg.toDouble(&ok);
        if (ok) sum += val;
    }
    return QString::number(sum);
});
```

使用：

```asul
LineField{
    .Args{
        .Arg{ .Id = "a" .Value = "10" }
        .Arg{ .Id = "b" .Value = "20" }
    }
    .Expression = $Add(a, b)
}
```

输出: `30`

### 示例：条件返回

```cpp
doc->registerFunction("GetMode", [](const QStringList &args) -> QString {
    QString mode = args.value(0, "Normal");
    if (mode == "Easy") return "1";
    if (mode == "Normal") return "2";
    if (mode == "Hard") return "3";
    return "2";
});
```

---

## 注册全局变量

### 单个注册

```cpp
doc->registerGlobalVariable("__Prefix", "[");
doc->registerGlobalVariable("__Suffix", "]");
doc->registerGlobalVariable("__GameName", "MyGame");
```

### 使用全局变量

```asul
TextField{
    .Command = "echo ${__Prefix}${__GameName}${__Suffix}"
}
```

输出: `echo [MyGame]`

---

## 批量设置全局变量

### setGlobalVariables()

```cpp
QMap<QString, QString> vars;
vars.insert("__Author", "Developer");
vars.insert("__Version", "1.0.0");
vars.insert("__Debug", "false");
vars.insert("__MaxPlayers", "8");

doc->setGlobalVariables(vars);
```

### 特点

- 覆盖已有的同名变量
- 不影响其他变量
- 适合批量初始化

---

## 全局变量 vs 局部变量

| 类型 | 语法 | 说明 |
|------|------|------|
| 全局变量 | `${__VarName}` | 以 `__` 前缀，可跨表单使用 |
| 局部变量 | `${FieldId}` | 通过字段引用获取属性值 |

---

## 在 Templates 中使用

### 模板插值

```cpp
doc->registerGlobalVariable("__Title", "Game Settings");
doc->registerGlobalVariable("__Author", "DevTeam");
```

```asul
TextField{
    .Command = "config.title = ${__Title}"
    .SubDescription = "Author: ${__Author}"
}
```

### 条件表达式

```cpp
doc->registerGlobalVariable("__DebugMode", "true");
```

```asul
KeyBind{
    .Enabled = ${__DebugMode} == "true" ? true : false
}
```

---

## 函数注册顺序

1. 创建 Document
2. 解析表单文本
3. 注册函数和全局变量
4. 调用 toCFG() 导出

```cpp
// 1. 创建
auto doc = AFormParser::Document::create();

// 2. 解析
doc->parse(formText, &err);

// 3. 注册扩展
doc->registerFunction("MyFunc", [](auto &args) { return args.join("-"); });
doc->registerGlobalVariable("__Var", "value");

// 4. 导出
QString cfg = doc->toCFG();
```

---

## 完整示例

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

        TextField{
            .Id = "Prefix"
            .Text = "[CONFIG]"
        }

        LineField{
            .Id = "Command"
            .Args{
                .Arg{ .Id = "a" .Value = "Hello" }
                .Arg{ .Id = "b" .Value = "World" }
            }
            .Expression = ${Prefix} + " " + $Join(a, b)
        }
    }
}
)";

    AFormParser::ParseError err;
    auto doc = AFormParser::Document::from(formText, &err);
    if (!doc) {
        qWarning() << "Parse error:" << err.message;
        return 1;
    }

    // 注册函数
    doc->registerFunction("Join", [](const QStringList &args) -> QString {
        return args.join("-");
    });

    // 注册全局变量
    doc->registerGlobalVariable("__Version", "1.0");

    // 导出
    qDebug().noquote() << doc->toCFG();

    return 0;
}
```

---

## 常见问题

### Q: 函数参数数量不固定怎么办？

```cpp
doc->registerFunction("Sum", [](const QStringList &args) -> QString {
    double total = 0;
    for (const QString &arg : args) {
        bool ok;
        total += arg.toDouble(&ok);
    }
    return QString::number(total);
});
```

### Q: 如何处理错误？

```cpp
doc->registerFunction("SafeDiv", [](const QStringList &args) -> QString {
    if (args.size() < 2) return "ERROR";
    double a = args[0].toDouble();
    double b = args[1].toDouble();
    if (b == 0) return "DIV_ZERO";
    return QString::number(a / b);
});
```

### Q: 可以在注册函数中访问 Document 吗？

可以。需要在注册前捕获 Document 指针或使用 lambda：

```cpp
auto doc = AFormParser::Document::from(...);
doc->registerFunction("GetFormId", [doc](auto &) {
    return doc->forms.first()->id;
});
```

---

## API 参考

### Document 方法

```cpp
// 注册函数
void registerFunction(const QString &name, LuaFunction func);

// 注册单个全局变量
void registerGlobalVariable(const QString &name, const QString &value);

// 批量设置全局变量
void setGlobalVariables(const QMap<QString, QString> &vars);
```