# register 自定义函数与全局值教程

本文演示如何在 AFormParser 中：

- 使用 `registerFunction` 注册自定义函数
- 使用 `registerGlobalVariable` 或 `setGlobalVariables` 注入全局值
- 在 `Scripts` 与表达式中消费这些能力

## 1. 关键 API

来自 `AFormParser::Document`：

- `void registerFunction(const QString &name, LuaFunction func);`
- `void registerGlobalVariable(const QString &name, const QString &value);`
- `void setGlobalVariables(const QMap<QString, QString> &vars);`
- `QString toCFG() const;`

其中 `LuaFunction` 类型为：

```cpp
using LuaFunction = std::function<QString(QStringList)>;
```

## 2. 最小可运行示例

下面示例中：

- `JoinWithDash` 是你在 C++ 里注册的函数
- `__Prefix` 是你注入的全局变量
- 表单 `LineField.Expression` 使用 `$JoinWithDash(a,b)` 调用函数
- `TextField.Command` 使用 `${__Prefix}` 使用全局值

```cpp
#include <QCoreApplication>
#include <QDebug>
#include "AFormParser/AFormParser.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString formText = QString::fromUtf8(R"FORM(
Form{
    .Id = "Demo"
    Group{
        .Title = "Register Demo"

        LineField{
            .Id = "Line_1"
            .Enabled = true
            .Description = "line"
            .SubDescription = "call register function"
            .Args{
                .Arg{ .Id = "a" .Value = "Hello" }
                .Arg{ .Id = "b" .Value = "World" }
            }
            .Expression = "echo " + $JoinWithDash(a,b)
        }

        TextField{
            .Id = "Text_1"
            .Enabled = true
            .Description = "text"
            .SubDescription = "use global variable"
            .Command = "echo ${__Prefix}"
        }
    }
}

Scripts{
    function JoinWithDash(x,y)
        return x .. "-" .. y
    end
}
)FORM");

    AFormParser::ParseError err;
    auto doc = AFormParser::Document::from(formText, &err);
    if (!doc) {
        qWarning() << "Parse failed:" << err.line << err.column << err.message;
        return 1;
    }

    // 1) 注册自定义函数
    doc->registerFunction(QStringLiteral("JoinWithDash"),
        [](const QStringList &args) -> QString {
            if (args.size() < 2) {
                return QStringLiteral("InvalidArgs");
            }
            return args.at(0) + QStringLiteral("-") + args.at(1);
        });

    // 2) 注入全局值（方式 A：逐个）
    doc->registerGlobalVariable(QStringLiteral("__Prefix"), QStringLiteral("AFormParser"));

    // 3) 导出 CFG
    const QString cfg = doc->toCFG();
    qDebug().noquote() << cfg;

    return 0;
}
```

## 3. 批量注入全局值

如果全局值较多，推荐使用 `setGlobalVariables`：

```cpp
QMap<QString, QString> vars;
vars.insert(QStringLiteral("__Prefix"), QStringLiteral("SDK"));
vars.insert(QStringLiteral("__Env"), QStringLiteral("Dev"));
vars.insert(QStringLiteral("__Version"), QStringLiteral("1.0.0"));

doc->setGlobalVariables(vars);
```

在 Form 字符串中可这样使用：

```text
.Command = "echo ${__Prefix}-${__Env}-${__Version}"
```

## 4. 使用建议

1. 全局值建议统一使用 `__` 前缀，避免与普通标识符冲突。
2. `registerFunction` 的函数名，保持与 Scripts/Expression 中调用名一致。
3. 当函数参数数量不固定时，在 lambda 中先检查 `args.size()` 再访问。
4. 若导出结果不符合预期，可先打印 `doc->dump()` 查看解析后结构。

## 5. 常见问题

### Q1: 什么时候注册函数和全局值？

建议在 `Document::from` 成功后、调用 `toCFG()` 之前注册。

### Q2: `registerGlobalVariable` 和 `setGlobalVariables` 有什么区别？

- `registerGlobalVariable` 适合单个增量设置。
- `setGlobalVariables` 适合批量初始化。

### Q3: 不写 `Scripts` 也能用 `registerFunction` 吗？

可以。你仍可在表达式中调用 `$函数名(...)`，前提是你已经在 C++ 里注册该函数。
