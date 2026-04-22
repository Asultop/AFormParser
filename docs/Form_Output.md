# 多表单输出支持

本文档介绍 AFormParser 的多表单支持和 `toCFGs()` 功能。

---

## 概述

AFormParser 支持在单个 `.asul` 文件中定义多个 `Form{}` 节点，每个 Form 可以有独立的：
- `.Id` - 表单标识
- `.Output` - 输出文件路径
- `.Description` - 表单描述

---

## 多表单文件示例

```asul
@Version: 1.0
@Description: Multi-Form Test

Form{
    .Id = "Form1"
    .Output = "scripts/KeyBindForm"
    .Description = "按键绑定表单"

    Group{
        .Title = "战斗设置"
        KeyBind{
            .Id = "AttackKey"
            .Description = "攻击"
            .Command = "+attack"
            .Bind("mouse1")
        }
    }
}

Form{
    .Id = "Form2"
    .Output = "scripts/TextInputForm"
    .Description = "文本输入表单"

    Group{
        .Title = "聊天设置"
        TextField{
            .Id = "ChatInput"
            .Description = "聊天内容"
            .Text = "Hello"
            .Command = "say ${ChatInput}"
        }
    }
}

Form{
    .Id = "Form3"
    .Output = "D:/export/AutoCompleteForm"
    .Description = "选项配置表单"

    Group{
        .Title = "模式选择"
        OptionField{
            .Id = "GameMode"
            .Description = "游戏模式"
            Options{
                Option{ .Id = "Easy" .Description = "简单" .Command = "difficulty 1" }
                Option{ .Id = "Normal" .Description = "普通" .Command = "difficulty 2" }
            }
            .Selected = "Normal"
        }
    }
}
```

---

## Form 属性

### .Output

指定表单的输出文件路径。

```asul
Form{
    .Output = "scripts/MyForm"
}
```

- 可以是相对路径或绝对路径
- 不需要加 `.cfg` 后缀（自动添加）
- 相对路径基于 `.asul` 文件位置解析

### .Description

表单描述，用于 UI 显示。

```asul
Form{
    .Description = "游戏设置表单"
}
```

### .Id

表单唯一标识。

```asul
Form{
    .Id = "MainSettings"
}
```

---

## toCFGs() 方法

### 返回类型

```cpp
QVector<CFGExportItem> toCFGs() const;
```

### CFGExportItem 结构

```cpp
struct CFGExportItem {
    QString output;         // Output 属性值（不含后缀）
    QString content;       // 生成的 CFG 内容
    QString relativePath;  // 相对路径 (如 "scripts/Test")
    QString absolutePath;   // 绝对路径（基于源文件解析）
    QString fileName;      // 完整文件名 (output + ".cfg")
    QString sourceFormId;   // 来源 Form 的 Id
    QString description;   // 来源 Form 的 Description
};
```

---

## 路径解析规则

### 相对路径

`.asul` 文件位于 `C:/game/config/ui.asul`：

| .Output 值 | fileName | absolutePath |
|------------|----------|--------------|
| `scripts/Test` | `scripts/Test.cfg` | `C:/game/config/scripts/Test.cfg` |
| `output/forms/KeyBind` | `output/forms/KeyBind.cfg` | `C:/game/config/output/forms/KeyBind.cfg` |

### 绝对路径

| .Output 值 | fileName | absolutePath |
|------------|----------|--------------|
| `D:/export/Form` | `D:/export/Form.cfg` | `D:/export/Form.cfg` |
| `/usr/local/cfg` | `/usr/local/cfg.cfg` | `/usr/local/cfg.cfg` |

### 路径分隔符

- 输入可以使用 `\` 或 `/`
- 输出统一使用 `/`
- 尾部斜杠自动移除

---

## 设置源文件路径

在调用 `toCFGs()` 前，设置源文件路径用于相对路径解析：

```cpp
doc->setSourceFilePath("C:/game/config/ui.asul");

auto items = doc->toCFGs();
for (const auto &item : items) {
    qDebug() << "Output:" << item.fileName;
    qDebug() << "Path:" << item.absolutePath;
}
```

---

## 使用示例

### C++ 导出多表单

```cpp
const QString asulPath = "C:/game/config/forms.asul";
QFile file(asulPath);
file.open(QIODevice::ReadOnly);
QString formText = QString::fromUtf8(file.readAll());
file.close();

auto doc = AFormParser::Document::from(formText);
if (!doc) return;

doc->setSourceFilePath(asulPath);

auto items = doc->toCFGs();
for (const auto &item : items) {
    qDebug() << "=== Form:" << item.sourceFormId;
    qDebug() << "Description:" << item.description;
    qDebug() << "Output:" << item.fileName;
    qDebug() << "Absolute:" << item.absolutePath;
    qDebug() << "Content:\n" << item.content;

    // 写入文件
    QFile out(item.absolutePath + ".cfg");
    out.open(QIODevice::WriteOnly);
    out.write(item.content.toUtf8());
    out.close();
}
```

### 向后兼容 toCFG()

```cpp
QString singleCfg = doc->toCFG();  // 返回第一个 Form 的 CFG
```

---

## form_to_cfg 工具

命令行工具支持多表单导出：

```bash
# 基本用法
./form_to_cfg.exe forms.asul

# 指定输出目录
./form_to_cfg.exe forms.asul output/

# 设置源文件路径
./form_to_cfg.exe input.asul --source=input.asul
```

---

## 多表单与 form_tree_viewer

form_tree_viewer 工具支持多表单选择：

1. **Form 选择器** - 当存在多个 Form 时显示下拉框
2. **下拉框内容** - 显示各 Form 的 `.Description`
3. **切换刷新** - 选择不同 Form 时刷新树视图和输出

### 界面布局

```
┌─────────────────────────────────────────────────────────────┐
│  [文件路径输入]  [浏览]                                      │
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

---

## 最佳实践

### 1. 为每个 Form 设置唯一的 Output

```asul
Form{
    .Id = "KeySettings"
    .Output = "output/Keys"
    .Description = "按键设置"
}

Form{
    .Id = "TextSettings"
    .Output = "output/Texts"
    .Description = "文本设置"
}
```

### 2. 使用 Description 便于识别

```asul
Form{
    .Id = "Combat"
    .Output = "scripts/Combat"
    .Description = "【战斗】按键和命令配置"
}

Form{
    .Id = "Chat"
    .Output = "scripts/Chat"
    .Description = "【聊天】聊天和命令配置"
}
```

### 3. 组织文件结构

```
game/
├── config/
│   ├── main.asul        # 包含多个 Form
│   ├── output/          # 输出目录
│   │   ├── Keys.cfg
│   │   ├── Texts.cfg
│   │   └── Modes.cfg
```

`.asul` 中使用相对路径：
```asul
Form{
    .Output = "output/Keys"
}

Form{
    .Output = "output/Texts"
}
```