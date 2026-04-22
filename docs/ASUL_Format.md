# ASUL 格式规范

本文档详细说明 `.asul` 表单文件的格式规范。

---

## 概述

ASUL (Asul Form Definition Language) 是一种用于定义表单配置的文本格式，主要特点：

- 类似 JSON/INI 的键值对语法
- 块结构使用 `{}`
- 支持模板表达式
- 内置 Lua 脚本支持

---

## 基本语法

### 文件结构

```asul
@MetaKey: MetaValue

Form{
    .Id = "FormId"
    .Output = "path/to/output"
    .Description = "表单描述"

    Group{
        .Title = "分组标题"

        // 字段定义
    }
}

Scripts{
    function MyFunc(arg)
        return arg
    end
}
```

### 注释

单行注释使用 `//`：

```asul
// 这是注释
KeyBind{
    .Id = "Key1"  // 行尾注释
    .Command = "+test"
}
```

---

## 元属性

元属性以 `@` 开头，必须定义在文件顶部：

```asul
@Version: 1.0
@Author: AuthorName
@Description: 表单描述
@Import: ["path1.lua", "path2.lua"]
```

---

## 块类型

### Form

表单根节点，一个文件可以包含多个 Form：

```asul
Form{
    .Id = "MyForm"
    .Output = "output/Form"
    .Description = "我的表单"

    Group{ ... }
}
```

### Group

分组容器，用于组织字段：

```asul
Group{
    .Title = "分组标题"

    KeyBind{ ... }
    TextField{ ... }
}
```

### KeyBind

按键绑定字段：

```asul
KeyBind{
    .Id = "AttackKey"
    .Enabled = true
    .Description = "攻击按键"
    .SubDescription = "按下执行攻击"
    .Command = "+attack"
    .Bind("mouse1")
}
```

### MustField

必须字段（无条件执行）：

```asul
MustField{
    .Id = "ConfirmField"
    .Enabled = true
    .Description = "确认"
    .Command = "+confirm"
    .Bind("enter")
}
```

### TextField

文本输入字段：

```asul
TextField{
    .Id = "ChatInput"
    .Enabled = true
    .Description = "聊天输入"
    .SubDescription = "输入聊天内容"
    .Text = "Hello"
    .Command = "say ${ChatInput}"
}
```

### LineField

表达式字段：

```asul
LineField{
    .Id = "CustomCmd"
    .Enabled = true
    .Description = "自定义命令"
    .Args{
        .Arg{
            .Id = "arg1"
            .Description = "参数1"
            .Value = "value1"
        }
        .Arg{
            .Id = "arg2"
            .Description = "参数2"
            .Value = "value2"
        }
    }
    .Expression = $BuildCommand(arg1, arg2)
}
```

### OptionField

选项字段：

```asul
OptionField{
    .Id = "GameMode"
    .Enabled = true
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
        Option{
            .Id = "Hard"
            .Description = "困难模式"
            .Command = "difficulty 3"
        }
    }
    .Selected = "Normal"
}
```

### Scripts

Lua 脚本块：

```asul
Scripts{
    function JoinArgs(a, b)
        return a .. "-" .. b
    end

    function GetDifficulty(mode)
        if mode == "Easy" then
            return "1"
        end
        return "2"
    end
}
```

---

## 属性赋值

### 字符串值

```asul
.Id = "MyId"
.Description = "描述文本"
.Command = "echo hello"
```

### 布尔值

```asul
.Enabled = true
.Enabled = false
```

### 数值

```asul
.Value = 100
.Value = 3.14
```

### 数组 (Import)

```asul
@Import = ["path1.lua", "path2.lua"]
```

### 函数调用

```asul
.Bind("mouse1")
```

---

## 属性引用

### 字段属性引用

```asul
MustField{
    .Enabled = $(AttackKey).Bind == "F" ? true : false
}
```

### 全局变量

```asul
.Text = ${__Prefix}
```

### 模板插值

```asul
.Command = "echo ${ChatInput}"
```

---

## 表达式

### 三元表达式

```asul
.Enabled = $(KeyBind1).Bind == "F" ? true : false
```

### 字符串拼接

```asul
.Expression = "echo " + $JoinArgs(a, b)
```

### Lua 函数调用

```asul
.Expression = $BuildCommand(arg1, arg2)
```

---

## 完整示例

```asul
@Version: 1.0
@Author: GameDev
@Description: 游戏配置表单
@Import: ["scripts/lua/utils.lua", "scripts/lua/helpers.lua"]

Form{
    .Id = "MainConfig"
    .Output = "scripts/MainConfig"
    .Description = "主配置表单"

    Group{
        .Title = "战斗设置"

        KeyBind{
            .Id = "AttackKey"
            .Description = "攻击"
            .Command = "+attack"
            .Bind("mouse1")
        }

        KeyBind{
            .Id = "JumpKey"
            .Description = "跳跃"
            .Command = "+jump"
            .Bind("space")
        }

        MustField{
            .Id = "ConfirmAttack"
            .Enabled = $(AttackKey).Bind != "None" ? true : false
            .Description = "确认攻击"
            .Command = "+attack_confirm"
            .Bind("enter")
        }
    }

    Group{
        .Title = "聊天设置"

        TextField{
            .Id = "ChatPrefix"
            .Description = "聊天前缀"
            .Text = "/all "
            .Command = "say ${__Prefix}${ChatPrefix}${ChatInput}"
        }

        TextField{
            .Id = "ChatInput"
            .Description = "聊天内容"
            .Text = "Hello"
            .Command = "say ${ChatInput}"
        }
    }

    Group{
        .Title = "游戏模式"

        OptionField{
            .Id = "Difficulty"
            .Description = "难度选择"
            Options{
                Option{
                    .Id = "Easy"
                    .Description = "简单"
                    .Command = "difficulty 1"
                }
                Option{
                    .Id = "Normal"
                    .Description = "普通"
                    .Command = "difficulty 2"
                }
                Option{
                    .Id = "Hard"
                    .Description = "困难"
                    .Command = "difficulty 3"
                }
            }
            .Selected = "Normal"
        }
    }

    Group{
        .Title = "自定义命令"

        LineField{
            .Id = "CustomCommand"
            .Description = "自定义命令"
            .Args{
                .Arg{
                    .Id = "cmd_name"
                    .Value = "test"
                }
                .Arg{
                    .Id = "cmd_args"
                    .Value = "arg1 arg2"
                }
            }
            .Expression = $BuildCommand(cmd_name, cmd_args)
        }
    }
}

Scripts{
    function BuildCommand(name, args)
        return "custom " .. name .. " " .. args
    end
}
```

---

## 语法规则总结

| 元素 | 语法 |
|------|------|
| 注释 | `// comment` |
| 元属性 | `@Key: Value` |
| 块开始 | `BlockName{` |
| 块结束 | `}` |
| 属性赋值 | `.Name = Value` 或 `.Name(args)` |
| 字符串 | `"text"` 或 `'text'` |
| 布尔 | `true` / `false` |
| 模板 | `${expr}` |
| 引用 | `$(Id).Property` |
| 函数调用 | `$FuncName(args)` |
| 三元 | `cond ? a : b` |
| 拼接 | `str1 + str2` |