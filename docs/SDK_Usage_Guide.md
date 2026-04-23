# AFormParser SDK 使用指南

本文档详细介绍如何使用 AFormParser SDK 解析 Form 文档、遍历节点树、以及通过 NodePtr 修改节点值。

---

## 目录

1. [基本概念](#基本概念)
2. [解析文档](#解析文档)
3. [Document 类](#document-类)
4. [节点类型](#节点类型)
5. [遍历节点树](#遍历节点树)
6. [NodePtr 类型转换](#nodeptr-类型转换)
7. [修改节点值](#修改节点值)
8. [字段启用表达式求值](#字段启用表达式求值)
9. [实时预览与表单联动](#实时预览与表单联动)
10. [完整示例](#完整示例)

---

## 基本概念

### NodeKind 枚举

```cpp
enum class NodeKind {
    Unknown,
    Form,       // 表单根节点
    Group,      // 分组容器
    Field,      // 字段基类
    KeyBind,    // 按键绑定
    MustField,  // 必须字段
    TextField,  // 文本字段
    LineField,  // 表达式字段（含参数）
    Arg,        // 函数参数
    OptionField,// 选项字段
    Option,     // 选项
    Scripts     // Lua 脚本
};
```

### NodePtr 类型

```cpp
using NodePtr = std::shared_ptr<Node>;
```

所有节点都使用 `std::shared_ptr` 管理内存。

---

## 解析文档

### 基本用法

```cpp
#include "AFormParser/AFormParser.hpp"

QString formText = R"(
Form{
    .Id = "MyForm"
    Group{
        .Title = "测试"
        KeyBind{
            .Id = "TestKey"
            .Description = "测试按键"
        }
    }
}
)";

AFormParser::ParseError err;
auto doc = AFormParser::Document::from(formText, &err);

if (!doc) {
    qDebug() << "解析失败:" << err.message << "行" << err.line << "列" << err.column;
    return;
}

qDebug() << "解析成功，共" << doc->forms.size() << "个表单";
```

### 从文件加载

```cpp
QFile file("path/to/form.asul");
if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
}

QTextStream stream(&file);
QString text = stream.readAll();
file.close();

auto doc = AFormParser::Document::from(text);
if (doc) {
    // 处理文档
}
```

---

## Document 类

### 主要成员

```cpp
class Document {
public:
    using Ptr = std::shared_ptr<Document>;

    // 静态工厂方法
    static Ptr from(const QString &formText, ParseError *error = nullptr);

    // 解析相关
    bool parse(const QString &formText, ParseError *error = nullptr);

    // 输出相关
    QString dump() const;                    // 导出为 ASUL 格式文本
    QString toCFG() const;                   // 转换为 CFG（单个 Form）
    QVector<CFGExportItem> toCFGs() const;   // 转换为 CFG（多个 Form）

    // 节点访问
    QVector<FormNode::Ptr> forms;            // 所有 Form 节点
    ScriptsNode::Ptr scripts;                 // Scripts 节点

    // 节点查询
    QVector<NodePtr> allNodes() const;       // 获取所有节点
    NodePtr findById(const QString &id) const;  // 通过 ID 查找节点

    // 元数据
    QString metaValue(const QString &key) const;
    void setMetaValue(const QString &key, const QString &value);

    // Lua 支持
    void registerFunction(const QString &name, LuaFunction func);
    void registerGlobalVariable(const QString &name, const QString &value);
    void setGlobalVariables(const QMap<QString, QString> &vars);

    // 路径解析
    void setSourceFilePath(const QString &path);
    QString sourceFilePath() const;
    static QString resolvePath(const QString &basePath, const QString &inputPath);
};
```

---

## 节点类型

### FormNode

表单根节点。

```cpp
class FormNode : public Node {
public:
    using Ptr = std::shared_ptr<FormNode>;

    QString id;              // 表单 ID
    QString output;           // 输出路径
    QString description;      // 描述
    QVector<GroupNode::Ptr> groups;  // 所有分组
};
```

### GroupNode

分组容器。

```cpp
class GroupNode : public Node {
public:
    using Ptr = std::shared_ptr<GroupNode>;

    QString title;                           // 分组标题
    QVector<FieldNode::Ptr> fields;         // 所有字段
    QMap<QString, QString> extraProperties; // 额外属性
};
```

### FieldNode

字段基类，所有字段类型都继承自它。

```cpp
class FieldNode : public Node {
public:
    using Ptr = std::shared_ptr<FieldNode>;

    QString id;                    // 字段 ID
    QString enabledExpression;      // 启用表达式
    QString description;           // 描述
    QString subDescription;        // 子描述
    QMap<QString, QString> extraProperties;  // 额外属性
};
```

### KeyBindNode

按键绑定字段。

```cpp
class KeyBindNode : public FieldNode {
public:
    using Ptr = std::shared_ptr<KeyBindNode>;

    QString command;    // 命令
    QString bind;      // 绑定键
};
```

### MustFieldNode

必须字段。

```cpp
class MustFieldNode : public FieldNode {
public:
    using Ptr = std::shared_ptr<MustFieldNode>;

    QString command;    // 命令
    QString bind;       // 绑定键（只读）
};
```

### TextFieldNode

文本字段。

```cpp
class TextFieldNode : public FieldNode {
public:
    using Ptr = std::shared_ptr<TextFieldNode>;

    QString text;       // 文本内容
    QString command;    // 命令
};
```

### LineFieldNode

表达式字段（含参数）。

```cpp
class LineFieldNode : public FieldNode {
public:
    using Ptr = std::shared_ptr<LineFieldNode>;

    QString expression;                    // 表达式
    QVector<ArgNode::Ptr> args;            // 参数列表
};
```

### ArgNode

函数参数。

```cpp
class ArgNode : public Node {
public:
    using Ptr = std::shared_ptr<ArgNode>;

    QString id;           // 参数 ID
    QString description;  // 参数描述
    QString value;        // 参数值
};
```

### OptionFieldNode

选项字段。

```cpp
class OptionFieldNode : public FieldNode {
public:
    using Ptr = std::shared_ptr<OptionFieldNode>;

    QVector<OptionNode::Ptr> options;  // 选项列表
    QString selected;                   // 当前选中项
};
```

### OptionNode

单个选项。

```cpp
class OptionNode : public Node {
public:
    using Ptr = std::shared_ptr<OptionNode>;

    QString id;          // 选项 ID
    QString description; // 选项描述
    QString command;     // 选项命令
};
```

---

## 遍历节点树

### 遍历所有 Form

```cpp
for (const auto &form : doc->forms) {
    qDebug() << "Form:" << form->id << form->description;
}
```

### 遍历 Form 下的 Group

```cpp
for (const auto &form : doc->forms) {
    for (const auto &group : form->groups) {
        qDebug() << "Group:" << group->title;
    }
}
```

### 遍历 Group 下的 Field

```cpp
for (const auto &form : doc->forms) {
    for (const auto &group : form->groups) {
        for (const auto &field : group->fields) {
            qDebug() << "Field:" << field->id << field->description;
        }
    }
}
```

### 遍历 LineField 的 Args

```cpp
if (auto line = field->toLineField()) {
    for (const auto &arg : line->args) {
        qDebug() << "Arg:" << arg->id << arg->description << arg->value;
    }
}
```

### 遍历 OptionField 的 Options

```cpp
if (auto optField = field->toOptionField()) {
    for (const auto &opt : optField->options) {
        qDebug() << "Option:" << opt->id << opt->description << opt->command;
    }
}
```

### 使用 allNodes() 遍历所有节点

```cpp
for (const auto &node : doc->allNodes()) {
    qDebug() << "NodeKind:" << static_cast<int>(node->kind());
}
```

### 使用 findById() 查找节点

```cpp
NodePtr node = doc->findById("TestKey");
if (node) {
    if (auto key = node->toKeyBind()) {
        qDebug() << "找到 KeyBind:" << key->command << key->bind;
    }
}
```

---

## NodePtr 类型转换

### 方法一：使用 to* 系列方法

```cpp
NodePtr node = /* 获取节点 */;

// 尝试转换为具体类型
if (auto key = node->toKeyBind()) {
    // 成功转换为 KeyBindNode
    qDebug() << key->bind;
}

// 尝试其他类型
if (auto text = node->toTextField()) {
    qDebug() << text->text;
}

if (auto line = node->toLineField()) {
    qDebug() << line->expression;
}

if (auto optField = node->toOptionField()) {
    qDebug() << optField->selected;
}

if (auto group = node->toGroup()) {
    qDebug() << group->title;
}

if (auto form = node->toForm()) {
    qDebug() << form->id;
}
```

### 方法二：使用模板 from() 方法

```cpp
NodePtr node = /* 获取节点 */;

// 使用静态 from 方法
if (auto key = KeyBindNode::from(node)) {
    qDebug() << key->bind;
}

// 使用通用模板
if (auto key = Node::from<KeyBindNode>(node)) {
    qDebug() << key->bind;
}
```

### 方法三：使用模板 to() 方法

```cpp
NodePtr node = /* 获取节点 */;

// 必须在节点支持 shared_from_this() 的情况下使用
if (auto key = node->to<KeyBindNode>()) {
    qDebug() << key->bind;
}
```

---

## 修改节点值

### 修改 Field 通用属性

```cpp
for (const auto &field : group->fields) {
    field->id = "new_id";
    field->description = "新描述";
    field->subDescription = "新子描述";
}
```

### 修改 KeyBind

```cpp
if (auto key = field->toKeyBind()) {
    key->command = "new_command";
    key->bind = "new_bind";
}
```

### 修改 MustField

```cpp
if (auto must = field->toMustField()) {
    must->command = "new_command";
    // must->bind 是只读的
}
```

### 修改 TextField

```cpp
if (auto text = field->toTextField()) {
    text->text = "new_text";
    text->command = "new_command";
}
```

### 修改 LineField

```cpp
if (auto line = field->toLineField()) {
    line->expression = "new_expression";
}
```

### 修改 LineField 的 Args

```cpp
if (auto line = field->toLineField()) {
    for (const auto &arg : line->args) {
        arg->value = "new_value";
    }
}
```

### 修改 OptionField

```cpp
if (auto optField = field->toOptionField()) {
    optField->selected = "option_id_2";
}
```

### 修改 OptionField 的 Option

```cpp
if (auto optField = field->toOptionField()) {
    for (const auto &opt : optField->options) {
        opt->id = "new_option_id";
        opt->description = "新选项描述";
        opt->command = "new_option_command";
    }
}
```

### 修改 Form 属性

```cpp
for (const auto &form : doc->forms) {
    form->id = "new_form_id";
    form->output = "new/output/path";
    form->description = "新描述";
}
```

### 修改 Group 属性

```cpp
for (const auto &group : form->groups) {
    group->title = "新分组标题";
}
```

---

## 字段启用表达式求值

### 概述

AFormParser 支持在字段的 `.Enabled` 属性中使用表达式来控制字段的启用/禁用状态。表达式可以引用其他字段的值，实现表单联动。

### 表达式语法

```cpp
.Enabled = true                              // 简单布尔值
.Enabled = $(FieldId).selected == "yes"     // 引用 OptionField 的选中值
.Enabled = $(FieldA).text == "Show"         // 引用 TextField 的文本值
.Enabled = $(MainSwitch).selected == "Enabled" && $(ModeSelect).selected == "ModeB"
```

### 求值 API

Document 类提供了两个求值方法：

```cpp
class Document {
public:
    // 求值单个字段的启用状态
    bool evaluateFieldEnabled(const QString &fieldId) const;

    // 批量求值所有字段的启用状态
    QVector<QPair<QString, bool>> evaluateAllFieldsEnabled() const;
};
```

### 单字段求值

```cpp
// 判断某个字段当前是否启用
bool isEnabled = doc->evaluateFieldEnabled("MyFieldId");
if (isEnabled) {
    // 字段可见且可用
} else {
    // 字段被隐藏或禁用
}
```

### 批量求值

```cpp
// 一次性获取所有字段的启用状态
auto results = doc->evaluateAllFieldsEnabled();
for (const auto &pair : results) {
    QString fieldId = pair.first;
    bool enabled = pair.second;
    qDebug() << fieldId << ":" << enabled;
}
```

---

## 实时预览与表单联动

### 表单联动场景

表单联动指一个字段的值变化时，影响其他字段的可见性或可用性。例如：

- **主开关控制**：一个 OptionField 控制下方所有字段的启用状态
- **模式切换**：选择不同模式显示不同的配置项
- **条件显示**：某些字段需要满足特定条件才显示

### 示例：主开关控制

```asul
Form{
    Group{
        OptionField{
            .Id = "MainSwitch"
            Options{
                Option{ .Id = "Enabled", .Description = "开启" }
                Option{ .Id = "Disabled", .Description = "关闭" }
            }
            .Selected = "Enabled"
        }

        TextField{
            .Id = "DetailText"
            .Enabled = $(MainSwitch).selected == "Enabled"
            .Description = "详细设置"
        }
    }
}
```

### 实时预览实现

使用 SDK 实现实时预览界面时，需要：

1. **建立字段到控件的映射**
2. **监听数据变化信号**
3. **变化时重新求值所有相关字段**

```cpp
#include <QTimer>

class RealTimePreviewForm : public QWidget {
    Q_OBJECT

public:
    explicit RealTimePreviewForm(AFormParser::Document::Ptr doc, QWidget *parent = nullptr)
        : QWidget(parent)
        , doc_(doc)
    {
        setupUi();
        buildFieldMapping();
    }

private:
    void setupUi()
    {
        // 创建预览布局
        previewLayout_ = new QVBoxLayout(this);

        // 为每个字段创建对应的编辑控件
        for (const auto &form : doc_->forms) {
            for (const auto &group : form->groups) {
                for (const auto &field : group->fields) {
                    QWidget *widget = createFieldWidget(field);
                    fieldToWidget_.insert(field, widget);
                    previewLayout_->addWidget(widget);
                }
            }
        }
    }

    void buildFieldMapping()
    {
        // 初始求值所有字段可见性
        updateAllFieldVisibility();
    }

    QWidget* createFieldWidget(AFormParser::FieldNode::Ptr field)
    {
        // 根据字段类型创建对应控件
        if (auto optField = field->toOptionField()) {
            auto *combo = new QComboBox();
            for (const auto &opt : optField->options) {
                combo->addItem(opt->description, opt->id);
            }

            int idx = combo->findData(optField->selected);
            if (idx >= 0) combo->setCurrentIndex(idx);

            // 关键：连接信号实时更新
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this, field, combo](int i) {
                        if (rebuilding_) return;
                        field->toOptionField()->selected = combo->itemData(i).toString();
                        updateAllFieldVisibility();  // 更新所有字段可见性
                    });

            return combo;
        }

        if (auto textField = field->toTextField()) {
            auto *edit = new QLineEdit(textField->text);

            // 文本输入使用防抖机制
            connect(edit, &QLineEdit::textChanged, this,
                    [this, field, edit](const QString &text) {
                        if (rebuilding_) return;
                        field->toTextField()->text = text;
                        updateAllFieldVisibility();
                    });

            return edit;
        }

        // ... 其他字段类型
        return new QWidget();
    }

    void updateAllFieldVisibility()
    {
        for (const auto &form : doc_->forms) {
            for (const auto &group : form->groups) {
                for (const auto &field : group->fields) {
                    bool enabled = doc_->evaluateFieldEnabled(field->id);

                    QWidget *widget = fieldToWidget_.value(field);
                    if (widget) {
                        widget->setVisible(enabled);
                        widget->setEnabled(enabled);
                    }
                }
            }
        }
    }

private:
    AFormParser::Document::Ptr doc_;
    QVBoxLayout *previewLayout_ = nullptr;
    QHash<AFormParser::FieldNode::Ptr, QWidget*> fieldToWidget_;
    bool rebuilding_ = false;
};
```

### 防抖机制优化

对于文本输入等高频事件，使用防抖避免频繁计算：

```cpp
class RealTimePreviewForm : public QWidget {
    Q_OBJECT

public:
    explicit RealTimePreviewForm(AFormParser::Document::Ptr doc, QWidget *parent = nullptr)
        : QWidget(parent)
        , doc_(doc)
    {
        // 创建防抖定时器
        refreshDebounceTimer_ = new QTimer(this);
        refreshDebounceTimer_->setSingleShot(true);
        refreshDebounceTimer_->setInterval(150);  // 150ms 延迟

        connect(refreshDebounceTimer_, &QTimer::timeout, this, [this]() {
            updateAllFieldVisibility();
        });
    }

    void onTextChanged(AFormParser::FieldNode::Ptr field, const QString &text)
    {
        if (auto textField = field->toTextField()) {
            textField->text = text;
        }
        refreshDebounceTimer_->start();  // 重启定时器
    }

    void onOptionChanged(AFormParser::FieldNode::Ptr field, const QString &selected)
    {
        if (auto optField = field->toOptionField()) {
            optField->selected = selected;
        }
        updateAllFieldVisibility();  // 离散事件立即更新
    }

private:
    QTimer *refreshDebounceTimer_ = nullptr;
};
```

### 关键要点

| 要点 | 说明 |
|------|------|
| **及时性** | ComboBox 选择变化时立即调用 `updateAllFieldVisibility()` |
| **防抖优化** | 文本输入使用 150ms 防抖，避免频繁刷新 |
| **全量求值** | 任一字段变化时，重新求值**所有**字段的 Enabled 状态（因为表达式可能引用任何字段） |
| **局部更新** | 只更新控件的 `setVisible/setEnabled`，不重建整个视图 |

### 获取最新数据导出

数据变化后，需要时可获取最新的 Dump 和 CFG：

```cpp
// 获取当前表单的 Dump
QString dump = form->dump();

// 获取所有表单的 CFG
QVector<CFGExportItem> items = doc_->toCFGs();
```

---

## 完整示例

### 解析并遍历所有控件

```cpp
#include "AFormParser/AFormParser.hpp"
#include <QDebug>

void processDocument(AFormParser::Document::Ptr doc)
{
    for (const auto &form : doc->forms) {
        qDebug() << "=== Form:" << form->id << "===";

        for (const auto &group : form->groups) {
            qDebug() << "  Group:" << group->title;

            for (const auto &field : group->fields) {
                QString typeName;
                if (field->toKeyBind()) typeName = "KeyBind";
                else if (field->toMustField()) typeName = "MustField";
                else if (field->toTextField()) typeName = "TextField";
                else if (field->toLineField()) typeName = "LineField";
                else if (field->toOptionField()) typeName = "OptionField";
                else typeName = "Field";

                qDebug() << "    [" << typeName << "]" << field->id << "-" << field->description;

                // 处理 LineField 的参数
                if (auto line = field->toLineField()) {
                    for (const auto &arg : line->args) {
                        qDebug() << "      Arg:" << arg->id << "=" << arg->value;
                    }
                }

                // 处理 OptionField 的选项
                if (auto optField = field->toOptionField()) {
                    qDebug() << "      Selected:" << optField->selected;
                    for (const auto &opt : optField->options) {
                        qDebug() << "      Option:" << opt->id << "-" << opt->description;
                    }
                }
            }
        }
    }
}
```

### 通过 ID 查找并修改

```cpp
void modifyById(AFormParser::Document::Ptr doc)
{
    // 查找并修改 KeyBind
    NodePtr keyNode = doc->findById("AttackKey");
    if (auto key = keyNode->toKeyBind()) {
        key->bind = "mouse2";
        key->command = "attack_secondary";
    }

    // 查找并修改 TextField
    NodePtr textNode = doc->findById("PlayerName");
    if (auto text = textNode->toTextField()) {
        text->text = "NewPlayerName";
    }

    // 查找并修改 LineField 参数
    NodePtr lineNode = doc->findById("EchoCommand");
    if (auto line = lineNode->toLineField()) {
        if (!line->args.isEmpty()) {
            line->args.first()->value = "modified_value";
        }
    }

    // 查找并修改 OptionField 选中项
    NodePtr optNode = doc->findById("GameMode");
    if (auto optField = optNode->toOptionField()) {
        optField->selected = "Hard";
    }
}
```

### 生成预览控件表

```cpp
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>

class FieldWidgetFactory {
public:
    static QWidget* createWidget(const AFormParser::FieldNode::Ptr &field, QWidget *parent)
    {
        if (auto key = field->toKeyBind()) {
            return createKeyBindWidget(key, parent);
        }
        if (auto text = field->toTextField()) {
            return createTextFieldWidget(text, parent);
        }
        if (auto line = field->toLineField()) {
            return createLineFieldWidget(line, parent);
        }
        if (auto optField = field->toOptionField()) {
            return createOptionFieldWidget(optField, parent);
        }
        return new QLabel("Unknown field type", parent);
    }

private:
    static QWidget* createKeyBindWidget(const AFormParser::KeyBindNode::Ptr &key, QWidget *parent)
    {
        auto *container = new QWidget(parent);
        auto *layout = new QHBoxLayout(container);

        auto *bindEdit = new QLineEdit(key->bind, container);
        auto *cmdEdit = new QLineEdit(key->command, container);

        layout->addWidget(new QLabel("Bind:"));
        layout->addWidget(bindEdit);
        layout->addWidget(new QLabel("Command:"));
        layout->addWidget(cmdEdit);

        QObject::connect(bindEdit, &QLineEdit::textChanged, [key](const QString &text) {
            key->bind = text;
        });
        QObject::connect(cmdEdit, &QLineEdit::textChanged, [key](const QString &text) {
            key->command = text;
        });

        return container;
    }

    static QWidget* createTextFieldWidget(const AFormParser::TextFieldNode::Ptr &text, QWidget *parent)
    {
        auto *container = new QWidget(parent);
        auto *layout = new QVBoxLayout(container);

        auto *textEdit = new QLineEdit(text->text, container);
        auto *cmdEdit = new QLineEdit(text->command, container);

        layout->addWidget(new QLabel("Text:"));
        layout->addWidget(textEdit);
        layout->addWidget(new QLabel("Command:"));
        layout->addWidget(cmdEdit);

        QObject::connect(textEdit, &QLineEdit::textChanged, [text](const QString &val) {
            text->text = val;
        });
        QObject::connect(cmdEdit, &QLineEdit::textChanged, [text](const QString &val) {
            text->command = val;
        });

        return container;
    }

    static QWidget* createLineFieldWidget(const AFormParser::LineFieldNode::Ptr &line, QWidget *parent)
    {
        auto *container = new QWidget(parent);
        auto *layout = new QVBoxLayout(container);

        auto *exprEdit = new QLineEdit(line->expression, container);
        layout->addWidget(new QLabel("Expression:"));
        layout->addWidget(exprEdit);

        QObject::connect(exprEdit, &QLineEdit::textChanged, [line](const QString &expr) {
            line->expression = expr;
        });

        for (const auto &arg : line->args) {
            auto *argLayout = new QHBoxLayout();
            auto *argEdit = new QLineEdit(arg->value, container);
            argLayout->addWidget(new QLabel(arg->description + ":"));
            argLayout->addWidget(argEdit, 1);
            layout->addLayout(argLayout);

            QObject::connect(argEdit, &QLineEdit::textChanged, [arg](const QString &val) {
                arg->value = val;
            });
        }

        return container;
    }

    static QWidget* createOptionFieldWidget(const AFormParser::OptionFieldNode::Ptr &optField, QWidget *parent)
    {
        auto *container = new QWidget(parent);
        auto *layout = new QHBoxLayout(container);

        auto *combo = new QComboBox(container);
        for (const auto &opt : optField->options) {
            combo->addItem(opt->description, opt->id);
        }

        int idx = combo->findData(optField->selected);
        if (idx >= 0) combo->setCurrentIndex(idx);

        layout->addWidget(new QLabel("Selected:"));
        layout->addWidget(combo, 1);

        QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         [optField, combo](int i) {
                             optField->selected = combo->itemData(i).toString();
                         });

        return container;
    }
};
```

### 使用示例

```cpp
// 解析
auto doc = AFormParser::Document::from(formText);

// 遍历
processDocument(doc);

// 修改
modifyById(doc);

// 导出
QString cfg = doc->toCFG();
QVector<AFormParser::CFGExportItem> items = doc->toCFGs();
for (const auto &item : items) {
    qDebug() << "Output:" << item.absolutePath;
    qDebug() << "Content:" << item.content;
}
```
