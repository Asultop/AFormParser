# 元属性

本文档介绍 AFormParser 中的元属性（Meta Attributes）。

---

## 概述

元属性是以 `@` 开头的全局属性，定义在表单文件顶部，作用于整个文档。

### 语法

```asul
@Key: Value
```

### 示例

```asul
@Version: 1.0
@Author: Asul
@Description: 主配置表单
@Import: ["../lua/utils.lua", "../lua/helpers.lua"]
```

---

## 内置元属性

### @Version

表单版本号。

```asul
@Version: 1.0
```

### @Author

表单作者。

```asul
@Author: Asul
```

### @Description

表单描述信息。

```asul
@Description: 用于游戏设置的配置表单
```

---

## @Import 元属性

`@Import` 用于指定 Lua 模块路径列表，供外部解析使用。

### 单路径格式

```asul
@Import: "../lua/utils.lua"
```

### 多路径格式 (JSON 数组)

```asul
@Import: ["../lua/utils.lua", "../lua/helpers.lua", "D:/scripts/base.lua"]
```

### C++ 获取路径

```cpp
QStringList paths = doc->importPaths();
// 返回: ["../lua/utils.lua", "../lua/helpers.lua", "D:/scripts/base.lua"]
```

### 用途

`@Import` 主要用于告知外部工具需要加载哪些 Lua 文件。解析器本身不自动加载这些文件，但会存储路径供应用程序使用。

---

## 自定义元属性

除内置元属性外，可以添加任意自定义元属性。

```asul
@GameName: MyGame
@ConfigVersion: 2.1
@LastModified: 2024-01-15
```

### C++ 访问自定义元属性

```cpp
QString gameName = doc->metaValue("GameName");
QString version = doc->metaValue("ConfigVersion");
```

### 遍历所有元属性

```cpp
for (const auto &[key, value] : doc->metaEntries()) {
    qDebug() << key << ":" << value;
}
```

### 设置元属性

```cpp
doc->setMetaValue("Version", "2.0");
doc->setMetaValue("Author", "NewAuthor");
```

---

## 元属性与解析

### 解析规则

1. 元属性必须在文件顶部（任何 `Form{}` 或 `Scripts{}` 之前）
2. 每行一个元属性
3. 支持引号包裹的值
4. 键名不区分大小写 (`@Version` 等于 `@version`)

### 错误处理

```cpp
AFormParser::ParseError err;
auto doc = AFormParser::Document::from(formText, &err);
if (err.hasError()) {
    qWarning() << "Error at" << err.line << err.column << err.message;
}
```

---

## 元属性示例

```asul
@Version: 1.0
@Author: GameDev
@Description: 游戏按键和命令配置
@Import: ["scripts/lua/utils.lua", "scripts/lua/helpers.lua"]
@GameTitle: MyAwesomeGame
@SupportedLanguages: ["en", "zh-CN", "ja"]
```

### C++ 访问

```cpp
// 获取内置元属性
QString version = doc->metaValue("Version");
QString author = doc->metaValue("Author");
QString desc = doc->metaValue("Description");

// 获取 Import 路径列表
QStringList imports = doc->importPaths();

// 获取自定义元属性
QString gameTitle = doc->metaValue("GameTitle");
QString langs = doc->metaValue("SupportedLanguages");
```