# 架构设计

本文档介绍 AFormParser SDK 的整体架构和核心设计理念。

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                        应用层                                │
│  (form_tree_viewer / form_to_cfg / 你的应用程序)            │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     Document 类                               │
│  - 解析 .asul 文本                                           │
│  - 管理节点树                                                 │
│  - 导出 CFG                                                  │
│  - Lua 脚本执行                                               │
└─────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
        ┌──────────┐   ┌──────────┐   ┌──────────┐
        │ FormNode │   │ScriptsNode│   │ Meta信息  │
        └──────────┘   └──────────┘   └──────────┘
              │
              ▼
        ┌──────────┐
        │GroupNode │
        └──────────┘
              │
              ▼
        ┌──────────┐
        │FieldNode │
        └──────────┘
              │
    ┌─────────┼─────────┬──────────┬──────────┐
    ▼         ▼         ▼          ▼          ▼
KeyBind   MustField  TextField  LineField  OptionField
```

---

## 核心组件

### Document (文档根节点)

`Document` 是解析的入口和根容器：

- 管理所有 `FormNode` (多表单支持)
- 管理 `ScriptsNode` (Lua 脚本)
- 存储元属性 (`@Key: Value`)
- 提供 `parse()`、`dump()`、`toCFG()` 等核心方法

```cpp
class Document : public std::enable_shared_from_this<Document> {
public:
    QVector<FormNode::Ptr> forms;      // 表单列表
    ScriptsNode::Ptr scripts;          // Lua 脚本
    QVector<QPair<QString, QString>> meta_;  // 元属性

    static Ptr from(const QString &formText, ParseError *error = nullptr);
    bool parse(const QString &formText, ParseError *error = nullptr);
    QString dump() const;
    QString toCFG() const;
    QVector<CFGExportItem> toCFGs() const;
};
```

### Node (节点基类)

所有表单元素都继承自 `Node`：

```cpp
class Node : public std::enable_shared_from_this<Node> {
public:
    NodeKind kind() const;
    QString dump(int indent = 0) const;  // 序列化
    virtual QString toCFG() const;      // 导出为 CFG

    template<typename T>
    std::shared_ptr<T> to();            // 类型转换

    QMap<QString, QString> extraProperties;  // 额外属性
};
```

### NodeKind (节点类型枚举)

```cpp
enum class NodeKind {
    Unknown,
    Form,        // 表单根节点
    Group,       // 分组容器
    Field,       // 字段基类
    KeyBind,     // 按键绑定
    MustField,   // 必须字段
    TextField,   // 文本字段
    LineField,   // 表达式字段
    Arg,         // 函数参数
    OptionField, // 选项字段
    Option,      // 选项项
    Scripts      // Lua 脚本
};
```

---

## 解析流程

```
.asul 文本
    │
    ▼
Document::parse()
    │
    ├── 行解析 (makeParsedLine)
    │   ├── 去除注释 (stripInlineComment)
    │   └── 提取内容 (contentOffset)
    │
    ├── 块处理
    │   ├── 检测块起始 `{` 和结束 `}`
    │   ├── 栈式层级管理 (Frame stack)
    │   └── 字符串引号校验
    │
    ├── 属性解析
    │   ├── 赋值语句 `.Id = value`
    │   ├── 函数调用 `.Bind("x")`
    │   └── 额外属性存储
    │
    └── Scripts 特殊处理
        ├── 多行捕获
        └── Lua 语法归一化
    │
    ▼
Document (完整节点树)
```

---

## 导出流程

```
Document
    │
    ▼
Document::toCFGs()
    │
    ├── 遍历 forms
    │
    ├── buildExportContext()
    │   ├── 收集所有字段属性到 context
    │   └── 设置全局变量
    │
    ├── 遍历 groups
    │
    ├── evaluateEnabled()    // 检查 .Enabled 条件
    │
    ├── evaluateExpression() // 模板求值
    │   ├── ${} 插值
    │   ├── $(Id).Property 引用
    │   ├── 三元表达式
    │   └── Lua 函数调用
    │
    ├── applyTemplate()     // 应用模板
    │
    └── 生成 CFGExportItem
        ├── fileName  (.Output + ".cfg")
        ├── absolutePath (路径解析)
        └── content (CFG 内容)
```

---

## Lua 集成

### LuaRuntime

内置 Lua 5.4 运行时：

```cpp
class LuaRuntime {
public:
    bool loadScript(const QString &script, QString *error = nullptr);
    bool executeFunction(const QString &fnName, const QStringList &args,
                         QString *result = nullptr, QString *error = nullptr);
    void registerFunction(const QString &name, LuaFunction func);
    void registerGlobalVariable(const QString &name, const QString &value);
};
```

### 表达式求值

`evaluateExpression()` 支持：

1. **模板变量**: `${VarName}` 或 `${__GlobalVar}`
2. **属性引用**: `$(FieldId).Property` 或 `*(FieldId).Property`
3. **三元表达式**: `cond ? trueVal : falseVal`
4. **字符串拼接**: `"str1" + "str2"`
5. **Lua 函数**: `$LuaFunc(arg1, arg2)`

---

## 路径解析

`Document::resolvePath()` 实现：

- **相对路径**: 基于 `.asul` 文件位置解析
  - `C:/game/config/ui.asul` + `scripts/test` → `C:/game/config/scripts/test.cfg`
- **绝对路径**: 直接使用
  - `D:/export/test` → `D:/export/test.cfg`
- **后缀**: 自动添加 `.cfg`

---

## 扩展机制

### C++ 函数注册

```cpp
doc->registerFunction("MyFunc", [](const QStringList &args) -> QString {
    return args.join("-");
});
```

### 全局变量

```cpp
doc->registerGlobalVariable("__Prefix", "SDK");
doc->setGlobalVariables({{"__A", "1"}, {"__B", "2"}});
```

---

## 线程安全

- `Document` 对象**非线程安全**，应在单线程使用
- `LuaRuntime` 使用静态注册表，支持跨对象共享函数