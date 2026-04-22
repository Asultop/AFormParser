# 表达式与模板

本文档介绍 AFormParser 中的模板语法和表达式求值。

---

## 模板语法总览

| 语法 | 说明 | 示例 |
|------|------|------|
| `${expression}` | 模板插值 | `${var}` |
| `${__global}` | 全局变量引用 | `${__Prefix}` |
| `$(Id).Property` | 字段属性引用 | `$(KeyBind1).Bind` |
| `*(Id).Property` | 字段属性引用（别名） | `*(KeyBind1).Bind` |
| `? :` | 三元表达式 | `cond ? a : b` |
| `+` | 字符串拼接 | `"str1" + "str2"` |
| `$function(args)` | Lua 函数调用 | `$JoinWithDash(a, b)` |

---

## 模板插值 `${}`

### 基本用法

```asul
TextField{
    .Id = "ChatInput"
    .Text = "Hello"
    .Command = "say ${ChatInput}"
}
```

求值后 `Command` 变为 `say Hello`。

### 支持嵌套引用

```asul
TextField{
    .Id = "Prefix"
    .Text = "[PREFIX]"

    .Command = "echo ${Prefix} world"
}
```

---

## 全局变量 `${__VarName}`

以 `__` 开头的变量被视为全局变量。

### 定义全局变量

```cpp
doc->registerGlobalVariable("__Prefix", "[");
doc->registerGlobalVariable("__Suffix", "]");
doc->setGlobalVariables({{"__GameMode", "Easy"}, {"__Version", "1.0"}});
```

### 使用全局变量

```asul
TextField{
    .Command = "echo ${__Prefix}Game Start${__Suffix}"
}
```

输出: `echo [Game Start]`

---

## 字段属性引用

### 语法

- `$(FieldId).Property` - 获取指定字段的属性值
- `*(FieldId).Property` - 同上（别名）

### 示例

```asul
KeyBind{
    .Id = "AttackKey"
    .Description = "攻击"
    .Command = "+attack"
    .Bind("mouse1")
}

MustField{
    .Id = "AttackConfirm"
    .Enabled = $(AttackKey).Bind == "F" ? true : false
    .Description = "攻击确认"
    .Command = "+confirm"
    .Bind("F")
}
```

当 `AttackKey` 的 `Bind` 为 `"F"` 时，`AttackConfirm` 才启用。

### 支持的属性

| 节点类型 | 支持的属性 |
|----------|-----------|
| KeyBind / MustField | `.Bind`, `.Command`, `.Id`, `.Enabled` |
| TextField | `.Text`, `.Command`, `.Id`, `.Enabled` |
| LineField | `.Expression`, `.Id`, `.Enabled` |
| OptionField | `.Selected`, `.Id`, `.Enabled` |

---

## 三元表达式

### 语法

```
condition ? trueValue : falseValue
```

### 示例

```asul
MustField{
    .Enabled = $(AttackKey).Bind == "F" ? true : false
}
```

### 嵌套三元表达式

```asul
OptionField{
    .Selected = $(DifficultyKey).Bind == "1" ? "Easy" : $(DifficultyKey).Bind == "2" ? "Normal" : "Hard"
}
```

---

## 字符串拼接 `+`

### 基本用法

```asul
TextField{
    .Command = "echo " + $(KeyBind1).Command
}
```

### 多字符串拼接

```asul
LineField{
    .Expression = "echo " + $(Prefix).Text + " - " + $(Suffix).Text
}
```

---

## Lua 函数调用 `$FunctionName()`

### Scripts 中定义函数

```asul
Scripts{
    function JoinArgs(arg1, arg2)
        return arg1 .. "-" .. arg2
    end
}
```

### 表达式中调用

```asul
LineField{
    .Args{
        .Arg{ .Id = "a" .Value = "Hello" }
        .Arg{ .Id = "b" .Value = "World" }
    }
    .Expression = $JoinArgs(a, b)
}
```

输出: `Hello-World`

---

## 表达式求值顺序

1. **解析模板** - 提取 `${}` 中的内容
2. **替换全局变量** - `${__Var}` → 全局变量值
3. **替换字段引用** - `$(Id).Property` → 字段属性值
4. **求三元表达式** - `cond ? a : b`
5. **字符串拼接** - `"str1" + "str2"`
6. **Lua 函数调用** - `$func(args)`
7. **应用结果** - 替换原表达式

---

## 模板函数 `applyTemplate()`

对字符串应用模板求值：

```cpp
QString result = applyTemplate("${Prefix}-${Suffix}", doc, ctx);
// 如果 Prefix="A", Suffix="B" → "A-B"
```

---

## Enabled 条件表达式

`.Enabled` 属性支持完整的表达式语法：

```asul
KeyBind{
    .Id = "DebugKey"
    .Enabled = $(DeveloperMode).Selected == "Enabled" ? true : false
    .Command = "debug"
    .Bind("F10")
}
```

- `.Enabled = true` - 始终启用（默认）
- `.Enabled = false` - 始终禁用
- `.Enabled = $(OtherField).Selected == "X"` - 条件启用

---

## 完整示例

```asul
@GlobalPrefix: "[CONFIG]"

Form{
    .Id = "ComplexForm"

    Group{
        .Title = "Settings"

        KeyBind{
            .Id = "MainKey"
            .Description = "主按键"
            .Command = "+main"
            .Bind("space")
        }

        TextField{
            .Id = "PrefixText"
            .Text = ${__GlobalPrefix}
            .Command = "echo ${PrefixText}"
        }

        MustField{
            .Enabled = $(MainKey).Bind == "space" ? true : false
            .Description = "确认执行"
            .Command = "+confirm"
            .Bind("enter")
        }

        OptionField{
            .Id = "ModeSelector"
            .Description = "模式选择"
            Options{
                Option{
                    .Id = "ModeA"
                    .Description = "模式A"
                    .Command = "set mode A"
                }
                Option{
                    .Id = "ModeB"
                    .Description = "模式B"
                    .Command = "set mode B"
                }
            }
            .Selected = "ModeA"
        }

        LineField{
            .Id = "CustomCmd"
            .Description = "自定义命令"
            .Args{
                .Arg{ .Id = "p1" .Value = "val1" }
                .Arg{ .Id = "p2" .Value = "val2" }
            }
            .Expression = $BuildCommand(p1, p2)
        }
    }
}

Scripts{
    function BuildCommand(a, b)
        return "custom " .. a .. " " .. b
    end
}
```