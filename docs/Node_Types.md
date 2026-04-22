# 节点类型详解

本文档详细介绍 AFormParser 中的所有节点类型。

---

## 节点类型总览

| 类型 | 描述 | 父类 |
|------|------|------|
| `FormNode` | 表单根节点 | `Node` |
| `GroupNode` | 分组容器 | `Node` |
| `FieldNode` | 字段基类 | `Node` |
| `KeyBindNode` | 按键绑定 | `FieldNode` |
| `MustFieldNode` | 必须字段 | `FieldNode` |
| `TextFieldNode` | 文本字段 | `FieldNode` |
| `LineFieldNode` | 表达式字段 | `FieldNode` |
| `OptionFieldNode` | 选项字段 | `FieldNode` |
| `OptionNode` | 选项项 | `Node` |
| `ArgNode` | 函数参数 | `Node` |
| `ScriptsNode` | Lua 脚本 | `Node` |

---

## FormNode (表单根节点)

表单的顶级容器，一个 Document 可以包含多个 Form。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `.Id` | QString | 表单唯一标识 |
| `.Output` | QString | 输出文件路径（不含后缀） |
| `.Description` | QString | 表单描述（UI 显示用） |

### 示例

```asul
Form{
    .Id = "MainForm"
    .Output = "scripts/Config"
    .Description = "主配置表单"
    Group{ ... }
}
```

### dump() 输出

```
Form{
    .Id = "MainForm"
    .Output = "scripts/Config"
    .Description = "主配置表单"
    Group{
        ...
    }
}
```

---

## GroupNode (分组容器)

用于组织和分组相关字段。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `.Title` | QString | 分组标题 |

### 子节点

- `KeyBindNode`
- `MustFieldNode`
- `TextFieldNode`
- `LineFieldNode`
- `OptionFieldNode`

### 示例

```asul
Group{
    .Title = "战斗设置"
    KeyBind{ ... }
    KeyBind{ ... }
}
```

---

## FieldNode (字段基类)

所有表单字段的基类，提供公共属性。

### 公共属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `.Id` | QString | 字段唯一标识 |
| `.Enabled` | QString | 启用条件表达式，默认 `"true"` |
| `.Description` | QString | 字段描述 |
| `.SubDescription` | QString | 子描述 |

### 额外属性

通过 `extraProperties` 存储非标准属性，如自定义扩展属性。

---

## KeyBindNode (按键绑定)

用于绑定按键和命令。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `.Command` | QString | 按下按键执行的命令 |
| `.Bind(...)` | QString | 绑定的按键，默认为 `"None"` |

### CFG 输出格式

```
bind <按键> <命令>
```

### 示例

```asul
KeyBind{
    .Id = "AttackKey"
    .Description = "攻击按键"
    .Command = "+attack"
    .Bind("mouse1")
}
```

### CFG 输出

```
bind mouse1 +attack
```

---

## MustFieldNode (必须字段)

始终执行的命令字段。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `.Command` | QString | 执行的命令 |
| `.Bind(...)` | QString | 绑定的按键 |

### 与 KeyBind 的区别

- `KeyBind`: 根据 `.Enabled` 条件决定是否执行
- `MustField`: 无条件执行

---

## TextFieldNode (文本字段)

文本输入字段。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `.Text` | QString | 默认文本内容 |
| `.Command` | QString | 执行的命令 |

### 示例

```asul
TextField{
    .Id = "ChatInput"
    .Description = "聊天内容"
    .Text = "Hello World"
    .Command = "say ${ChatInput}"
}
```

---

## LineFieldNode (表达式字段)

通过表达式生成命令的字段。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `.Args{}` | QVector<ArgNode> | 参数列表 |
| `.Expression` | QString | 表达式 |

### 子节点

#### ArgNode

| 属性 | 类型 | 说明 |
|------|------|------|
| `.Id` | QString | 参数标识 |
| `.Description` | QString | 参数描述 |
| `.Value` | QString | 参数值 |

### 示例

```asul
LineField{
    .Id = "CustomCmd"
    .Description = "自定义命令"
    .Args{
        .Arg{
            .Id = "arg1"
            .Value = "param1"
        }
        .Arg{
            .Id = "arg2"
            .Value = "param2"
        }
    }
    .Expression = $BuildCommand(arg1, arg2)
}
```

---

## OptionFieldNode (选项字段)

多选项选择字段。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `.Selected` | QString | 默认选中的选项 Id |
| `Options{}` | QVector<OptionNode> | 选项列表 |

### 子节点

#### OptionNode

| 属性 | 类型 | 说明 |
|------|------|------|
| `.Id` | QString | 选项标识 |
| `.Description` | QString | 选项描述 |
| `.Command` | QString | 选中时执行的命令 |

### 示例

```asul
OptionField{
    .Id = "GameMode"
    .Description = "游戏模式"
    Options{
        Option{
            .Id = "Easy"
            .Description = "简单模式"
            .Command = "difficulty 1"
        }
        Option{
            .Id = "Normal"
            .Description = "普通模式"
            .Command = "difficulty 2"
        }
    }
    .Selected = "Normal"
}
```

### CFG 输出

如果 `Normal` 被选中，输出：

```
difficulty 2
```

---

## ScriptsNode (Lua 脚本)

定义 Lua 函数供表达式调用。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `rawCode` | QString | Lua 代码 |

### 语法

支持标准 Lua 函数语法，自动归一化处理：

- `function name(arg1, arg2)` → `function name(arg1, arg2)`
- `var x = value` → `local x = value`
- `if (cond) { }` → `if cond then end`

### 示例

```asul
Scripts{
    function BuildCommand(arg1, arg2)
        return "custom " .. arg1 .. " " .. arg2
    end
}
```

---

## CFGExportItem (导出项)

`toCFGs()` 返回的导出数据结构。

```cpp
struct CFGExportItem {
    QString output;         // Output 属性值（不含后缀）
    QString content;       // 生成的 CFG 内容
    QString relativePath;  // 相对路径
    QString absolutePath;   // 绝对路径（基于源文件位置解析）
    QString fileName;      // 完整文件名 (output + ".cfg")
    QString sourceFormId;   // 来源 Form 的 Id
    QString description;   // 来源 Form 的 Description
};
```

### 路径解析示例

假设 `.asul` 文件位于 `C:/game/config/ui.asul`：

| .Output 值 | fileName | absolutePath |
|------------|----------|--------------|
| `scripts/Test` | `scripts/Test.cfg` | `C:/game/config/scripts/Test.cfg` |
| `D:/export/Out` | `D:/export/Out.cfg` | `D:/export/Out.cfg` |

---

## 节点类型判断

```cpp
// 在 C++ 中判断节点类型
if (auto keyNode = node->toKeyBind()) {
    // 是 KeyBind 节点
} else if (auto textNode = node->toTextField()) {
    // 是 TextField 节点
} else if (auto optionField = node->toOptionField()) {
    // 是 OptionField 节点
}
```