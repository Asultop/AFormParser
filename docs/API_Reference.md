# API 参考

本文档提供 AFormParser SDK 的完整 API 参考。

---

## 命名空间

```cpp
namespace AFormParser {
    // 所有类和函数都在此命名空间下
}
```

---

## ParseError 结构

解析错误信息结构。

```cpp
struct ParseError {
    int line = -1;      // 错误行号
    int column = -1;    // 错误列号
    QString message;    // 错误消息

    bool hasError() const { return !message.isEmpty(); }
};
```

---

## Document 类

解析入口和核心 API。

### 创建与解析

```cpp
// 创建空文档
static Ptr create();

// 从文本解析
static Ptr from(const QString &formText, ParseError *error = nullptr);

// 解析文本（已在创建后）
bool parse(const QString &formText, ParseError *error = nullptr);
```

### 序列化

```cpp
// 导出为可读文本（dump 格式）
QString dump() const;

// 导出第一个表单为 CFG
QString toCFG() const;

// 导出所有表单为 CFG 列表
QVector<CFGExportItem> toCFGs() const;
```

### 元属性

```cpp
// 获取元属性值
QString metaValue(const QString &key) const;

// 设置元属性
void setMetaValue(const QString &key, const QString &value);

// 获取所有元属性
QVector<QPair<QString, QString>> metaEntries() const;

// 获取 Import 路径列表
QStringList importPaths() const;
```

### 节点访问

```cpp
// 获取所有节点
QVector<NodePtr> allNodes() const;

// 按 Id 查找节点
NodePtr findById(const QString &id) const;
```

### 扩展功能

```cpp
// 注册 Lua 函数
void registerFunction(const QString &name, LuaFunction func);

// 注册全局变量
void registerGlobalVariable(const QString &name, const QString &value);

// 批量设置全局变量
void setGlobalVariables(const QMap<QString, QString> &vars);

// 获取全局变量
QMap<QString, QString> globalVariables() const;

// 执行脚本函数
QString executeScriptFunction(const QString &fnName,
                              const QStringList &args,
                              QString *error = nullptr) const;
```

### 路径处理

```cpp
// 设置源文件路径（用于相对路径解析）
void setSourceFilePath(const QString &path);

// 获取源文件路径
QString sourceFilePath() const;

// 解析路径（静态方法）
static QString resolvePath(const QString &basePath, const QString &inputPath);
```

### 清理

```cpp
void clear();
```

### 成员变量

```cpp
QVector<FormNode::Ptr> forms;      // 表单列表
ScriptsNode::Ptr scripts;          // Lua 脚本
```

### LuaFunction 类型

```cpp
using LuaFunction = std::function<QString(QStringList)>;
```

---

## FormNode 类

表单根节点。

### 创建

```cpp
static Ptr create();
static Ptr from(const NodePtr &node);
```

### 类型转换

```cpp
std::shared_ptr<FormNode> toForm() override;
```

### 属性

```cpp
QString id;           // 表单标识
QString output;       // 输出路径
QString description;  // 表单描述
QVector<GroupNode::Ptr> groups;  // 分组列表
```

### 方法

```cpp
QString dump(int indent = 0) const override;
QString toCFG() const;
```

---

## GroupNode 类

分组容器节点。

### 创建

```cpp
static Ptr create();
static Ptr from(const NodePtr &node);
```

### 类型转换

```cpp
std::shared_ptr<GroupNode> toGroup() override;
```

### 属性

```cpp
QString title;                            // 分组标题
QVector<FieldNode::Ptr> fields;           // 字段列表
```

### 方法

```cpp
QString dump(int indent = 0) const override;
QString toCFG() const override;
```

---

## FieldNode 类

字段基类（抽象）。

### 创建

```cpp
static Ptr from(const NodePtr &node);
```

### 类型转换

```cpp
std::shared_ptr<FieldNode> toField() override;
```

### 公共属性

```cpp
QString id;                    // 字段标识
QString enabledExpression;      // 启用条件
QString description;            // 描述
QString subDescription;         // 子描述
QMap<QString, QString> extraProperties;  // 额外属性
```

### 方法

```cpp
QString dumpCommonProperties(int indent) const;
QString dump(int indent = 0) const override;
QString toCFG() const override;
```

---

## KeyBindNode 类

按键绑定字段。

### 创建

```cpp
static Ptr create();
static Ptr from(const NodePtr &node);
```

### 类型转换

```cpp
std::shared_ptr<KeyBindNode> toKeyBind() override;
```

### 属性

```cpp
QString command;   // 执行命令
QString bind;     // 绑定按键
```

### 方法

```cpp
QString dump(int indent = 0) const override;
QString toCFG() const override;
```

### CFG 输出格式

```
bind <按键> <命令>
```

---

## MustFieldNode 类

必须字段。

### 创建

```cpp
static Ptr create();
static Ptr from(const NodePtr &node);
```

### 类型转换

```cpp
std::shared_ptr<MustFieldNode> toMustField() override;
```

### 属性

```cpp
QString command;   // 执行命令
QString bind;     // 绑定按键
```

### 方法

```cpp
QString dump(int indent = 0) const override;
QString toCFG() const override;
```

---

## TextFieldNode 类

文本字段。

### 创建

```cpp
static Ptr create();
static Ptr from(const NodePtr &node);
```

### 类型转换

```cpp
std::shared_ptr<TextFieldNode> toTextField() override;
```

### 属性

```cpp
QString text;     // 默认文本
QString command;   // 执行命令
```

### 方法

```cpp
QString dump(int indent = 0) const override;
QString toCFG() const override;
```

---

## LineFieldNode 类

表达式字段。

### 创建

```cpp
static Ptr create();
static Ptr from(const NodePtr &node);
```

### 类型转换

```cpp
std::shared_ptr<LineFieldNode> toLineField() override;
```

### 属性

```cpp
QVector<ArgNode::Ptr> args;      // 参数列表
QString expression;              // 表达式
```

### 方法

```cpp
QString dump(int indent = 0) const override;
QString toCFG() const override;
```

---

## OptionFieldNode 类

选项字段。

### 创建

```cpp
static Ptr create();
static Ptr from(const NodePtr &node);
```

### 类型转换

```cpp
std::shared_ptr<OptionFieldNode> toOptionField() override;
```

### 属性

```cpp
QVector<OptionNode::Ptr> options;   // 选项列表
QString selected;                    // 选中项 Id
```

### 方法

```cpp
QString dump(int indent = 0) const override;
QString toCFG() const override;
```

---

## OptionNode 类

选项项。

### 创建

```cpp
static Ptr create();
static Ptr from(const NodePtr &node);
```

### 类型转换

```cpp
std::shared_ptr<OptionNode> toOption() override;
```

### 属性

```cpp
QString id;           // 选项标识
QString description;   // 选项描述
QString command;      // 执行命令
```

### 方法

```cpp
QString dump(int indent = 0) const override;
QString toCFG() const override;
```

---

## ArgNode 类

函数参数。

### 创建

```cpp
static Ptr create();
static Ptr from(const NodePtr &node);
```

### 类型转换

```cpp
std::shared_ptr<ArgNode> toArg() override;
```

### 属性

```cpp
QString id;           // 参数标识
QString description;   // 参数描述
QString value;        // 参数值
```

### 方法

```cpp
QString dump(int indent = 0) const override;
QString toCFG() const override;
```

---

## ScriptsNode 类

Lua 脚本块。

### 创建

```cpp
static Ptr create();
static Ptr from(const NodePtr &node);
```

### 类型转换

```cpp
std::shared_ptr<ScriptsNode> toScripts() override;
```

### 属性

```cpp
QString rawCode;   // 原始代码
```

### 方法

```cpp
QString dump(int indent = 0) const override;
QString toCFG() const override;
```

---

## Node 基类

所有节点的基类。

### 类型转换方法

```cpp
virtual std::shared_ptr<FormNode> toForm();
virtual std::shared_ptr<GroupNode> toGroup();
virtual std::shared_ptr<FieldNode> toField();
virtual std::shared_ptr<KeyBindNode> toKeyBind();
virtual std::shared_ptr<MustFieldNode> toMustField();
virtual std::shared_ptr<TextFieldNode> toTextField();
virtual std::shared_ptr<LineFieldNode> toLineField();
virtual std::shared_ptr<ArgNode> toArg();
virtual std::shared_ptr<OptionFieldNode> toOptionField();
virtual std::shared_ptr<OptionNode> toOption();
virtual std::shared_ptr<ScriptsNode> toScripts();
```

### 模板方法

```cpp
template <typename T>
std::shared_ptr<T> to() {
    return std::dynamic_pointer_cast<T>(shared_from_this());
}

template <typename T>
static std::shared_ptr<T> from(const NodePtr &node) {
    return std::dynamic_pointer_cast<T>(node);
}
```

### 属性

```cpp
QMap<QString, QString> extraProperties;  // 额外属性
```

### 方法

```cpp
NodeKind kind() const;
bool is(NodeKind target) const;
QString dump(int indent = 0) const override;
QString toCFG() const override;
```

---

## CFGExportItem 结构

导出项数据结构。

```cpp
struct CFGExportItem {
    QString output;         // Output 属性值（不含后缀）
    QString content;        // CFG 内容
    QString relativePath;   // 相对路径
    QString absolutePath;    // 绝对路径
    QString fileName;        // 文件名（含 .cfg 后缀）
    QString sourceFormId;   // Form Id
    QString description;     // Form Description
};
```

---

## NodeKind 枚举

节点类型枚举。

```cpp
enum class NodeKind {
    Unknown,
    Form,
    Group,
    Field,
    KeyBind,
    MustField,
    TextField,
    LineField,
    Arg,
    OptionField,
    Option,
    Scripts
};
```

---

## LuaRuntime 类

Lua 运行时封装（一般通过 Document 使用）。

```cpp
class LuaRuntime {
public:
    using LuaFunction = std::function<QString(QStringList)>;

    LuaRuntime();
    ~LuaRuntime();

    bool loadScript(const QString &script, QString *error = nullptr);
    bool executeFunction(const QString &fnName,
                         const QStringList &args,
                         QString *result = nullptr,
                         QString *error = nullptr);

    void registerFunction(const QString &name, LuaFunction func);
    void unregisterFunction(const QString &name);

    void registerGlobalVariable(const QString &name, const QString &value);
    void unregisterGlobalVariable(const QString &name);
    void setGlobalVariables(const QMap<QString, QString> &vars);

    void clear();
};
```