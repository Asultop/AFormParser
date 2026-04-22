# Lua 脚本

本文档介绍 AFormParser 中 Lua 脚本的使用。

---

## Scripts 块

`Scripts{}` 块用于定义 Lua 函数，供表单表达式调用。

### 基本语法

```asul
Scripts{
    function FunctionName(arg1, arg2)
        -- 函数体
        return value
    end
}
```

### 示例

```asul
Scripts{
    function Greet(name)
        return "Hello, " .. name
    end
}
```

然后在表达式中调用：

```asul
TextField{
    .Command = "echo " + $Greet("World")
}
```

---

## 支持的 Lua 语法

AFormParser 自动将多种语法归一化为标准 Lua：

### 1. 函数定义

```asul
Scripts{
    // 标准 Lua 语法
    function add(a, b)
        return a + b
    end
}
```

### 2. 变量声明

```asul
Scripts{
    // JavaScript 风格 → 自动转换
    var x = 10

    // 转换后等同于:
    local x = 10
}
```

### 3. if 语句

```asul
Scripts{
    // JavaScript 风格 → 自动转换
    if (condition) {
        return 1
    }

    // 转换后等同于:
    if condition then
        return 1
    end
}
```

### 4. 赋值与运算

```asul
Scripts{
    // += 运算
    counter += 1

    // 转换后等同于:
    counter = counter .. 1
}
```

---

## 归一化转换规则

| 原始语法 | 转换后 |
|----------|--------|
| `var x = val` | `local x = val` |
| `function name() { }` | `function name() then end` |
| `if (cond) { }` | `if cond then end` |
| `}` | `end` |
| `return x` | `return x` |

### 详细转换示例

输入：
```asul
Scripts{
    function ProcessArgs(arg1, arg2, arg3)
        var n1 = tonumber(arg1) or 0
        var n2 = tonumber(arg2) or 0
        var n3 = tonumber(arg3) or 0
        var sum = n1 + n2 + n3
        return tostring(sum)
    end
}
```

归一化后：
```lua
function ProcessArgs(arg1, arg2, arg3)
    local n1 = tonumber(arg1) or 0
    local n2 = tonumber(arg2) or 0
    local n3 = tonumber(arg3) or 0
    local sum = n1 + n2 + n3
    return tostring(sum)
end
```

---

## 在表达式中调用 Lua 函数

### 语法

```
$FunctionName(arg1, arg2, ...)
```

### 示例

#### 基本调用

```asul
Scripts{
    function GetVersion()
        return "1.0.0"
    end
}

TextField{
    .Command = "echo " + $GetVersion()
}
```

#### 带参数调用

```asul
Scripts{
    function JoinPath(dir, file)
        return dir .. "/" .. file
    end
}

LineField{
    .Args{
        .Arg{ .Id = "dir" .Value = "scripts" }
        .Arg{ .Id = "file" .Value = "main.lua" }
    }
    .Expression = $JoinPath(dir, file)
}
```

#### 多参数

```asul
Scripts{
    function BuildCommand(cmd, arg1, arg2)
        return cmd .. " " .. arg1 .. " " .. arg2
    end
}

LineField{
    .Args{
        .Arg{ .Id = "c" .Value = "run" }
        .Arg{ .Id = "a" .Value = "arg1" }
        .Arg{ .Id = "b" .Value = "arg2" }
    }
    .Expression = $BuildCommand(c, a, b)
}
```

---

## Lua 标准库

AFormParser 内置 Lua 5.4 标准库：

- `string` - 字符串操作
- `table` - 表操作
- `math` - 数学函数
- `tonumber` / `tostring` - 类型转换
- `pairs` / `ipairs` - 迭代
- `pcall` - 保护调用

---

## C++ 注册函数

除 Scripts 中定义的函数外，还可以在 C++ 中注册函数：

```cpp
doc->registerFunction("GetUserName", [](const QStringList &args) -> QString {
    return "Player_" + args.value(0, "Unknown");
});
```

然后在表达式中调用：

```asul
TextField{
    .Command = "echo " + $GetUserName("12345")
}
```

---

## 函数执行流程

```
表达式求值
    │
    ▼
evaluateExpression()
    │
    ├── 解析 $FunctionName(args)
    │
    ├── 收集参数值
    │   ├── 从 .Args 中获取
    │   └── 从字段引用 $(Id).Property 获取
    │
    ├── 调用 Document::executeScriptFunction()
    │   │
    │   └── LuaRuntime::executeFunction()
    │       │
    │       └── lua_pcall() 执行函数
    │
    └── 返回结果字符串
```

---

## 错误处理

### 脚本语法错误

```cpp
AFormParser::ParseError err;
auto doc = AFormParser::Document::from(formText, &err);
if (err.hasError()) {
    qWarning() << "Script error at line" << err.line << err.message;
}
```

### 函数执行错误

```cpp
QString result;
QString error;
result = doc->executeScriptFunction("MyFunc", {"arg1", "arg2"}, &error);
if (!error.isEmpty()) {
    qWarning() << "Execution error:" << error;
}
```

---

## 完整示例

```asul
Scripts{
    function FormatCommand(action, target, value)
        return action .. " " .. target .. " " .. value
    end

    function GetDifficultyLevel(mode)
        if mode == "Easy" then
            return "1"
        end
        if mode == "Normal" then
            return "2"
        end
        return "3"
    end
}

Form{
    .Id = "CommandForm"

    Group{
        .Title = "命令生成"

        OptionField{
            .Id = "ActionMode"
            .Description = "动作模式"
            Options{
                Option{ .Id = "A1" .Description = "攻击" .Command = "attack" }
                Option{ .Id = "A2" .Description = "防御" .Command = "defend" }
                Option{ .Id = "A3" .Description = "逃跑" .Command = "flee" }
            }
            .Selected = "A1"
        }

        TextField{
            .Id = "TargetInput"
            .Description = "目标"
            .Text = "enemy1"
        }

        LineField{
            .Id = "CommandLine"
            .Description = "执行命令"
            .Args{
                .Arg{ .Id = "act" .Value = "attack" }
                .Arg{ .Id = "tgt" .Value = "enemy1" }
                .Arg{ .Id = "val" .Value = "100" }
            }
            .Expression = $FormatCommand(act, tgt, val)
        }
    }
}
```