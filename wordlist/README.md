# 单词库说明

本目录包含用于代码混淆的单词库文件。

## 文件说明

| 文件 | 用途 | 示例 |
|------|------|------|
| `adjectives.txt` | 形容词，用于类名、属性名的修饰 | Swift, Core, Main |
| `nouns.txt` | 名词，用于类名、属性名 | Manager, Data, View |
| `verbs.txt` | 动词，用于方法名 | Fetch, Update, Handle |
| `parameters.txt` | 参数专用单词 | value, data, handler |

## 文件格式

- 每行一个单词
- `#` 开头的行为注释，会被忽略
- 空行会被忽略
- 单词区分大小写

## 配置方式

### 随机字母风格 (style: "random")

```json
{
  "obfuscation": {
    "namingRule": {
      "style": "random",
      "prefix": "OBF_",
      "charset": "alphanumeric",
      "randomLength": {
        "className": { "min": 6, "max": 12 },
        "methodName": { "min": 6, "max": 12 },
        "propertyName": { "min": 6, "max": 12 },
        "fileName": { "min": 8, "max": 16 },
        "folderName": { "min": 8, "max": 16 },
        "parameterName": { "min": 4, "max": 8 }
      }
    }
  }
}
```

### 单词库风格 (style: "words")

```json
{
  "obfuscation": {
    "namingRule": {
      "style": "words",
      "prefix": "OBF_",
      "wordListPath": "./wordlist",
      "wordCase": "camelCase",
      "wordCount": {
        "className": { "min": 1, "max": 2 },
        "methodName": { "min": 1, "max": 2 },
        "propertyName": { "min": 1, "max": 2 },
        "fileName": { "min": 2, "max": 3 },
        "folderName": { "min": 2, "max": 3 },
        "parameterName": { "min": 1, "max": 1 }
      }
    }
  }
}
```

## 单词组合规则

| 元素类型 | 单词来源 | 示例 |
|---------|---------|------|
| 类名 | 形容词 + 名词 | SwiftManager, CoreData |
| 方法名 | 动词 + 名词 | FetchData, UpdateInfo |
| 属性名 | 名词 (可能+形容词) | value, dataState |
| 文件名 | 形容词 + 名词 + [名词] | BlueManager.h, CoreDataHandler.m |
| 文件夹名 | 形容词 + 名词 + [名词] | SwiftManager/, CoreData/ |
| 参数名 | parameters.txt | value, handler |

## wordCase 选项

| 值 | 说明 | 示例 |
|----|------|------|
| `camelCase` | 首字母小写，后续单词首字母大写 | blueManager, coreData |
| `PascalCase` | 每个单词首字母大写 | BlueManager, CoreData |
| `snake_case` | 下划线分隔，全小写 | blue_manager, core_data |
| `kebab-case` | 连字符分隔，全小写 | blue-manager, core-data |
| `UPPER_CASE` | 下划线分隔，全大写 | BLUE_MANAGER, CORE_DATA |

## 自定义单词库

您可以：

1. **添加单词**：在对应文件末尾添加新单词，每行一个
2. **删除单词**：直接删除不需要的行
3. **创建新文件**：添加新的分类文件
