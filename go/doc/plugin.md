# plugin 构建模式
类似c++中的动态链接

## 使用场景
插件SDK、业务代码中频繁修改的部分
## 使用方式
1. 插件代码
插件必须声明package main，并且不定义main函数
```go
package main
import "fmt"
var Version = "1.0.0"

func HelloWorld(){
    fmt.Println("hello world!")
}

func init(){
    fmt.Println("init plugin")
}
```

2. 编译插件
使用go build并指定--buildmode=plugin编译为插件文件（扩展名为.so）
```bash
go build -buildmode=plugin -o plugin.so plugin.go
```

3. 主程序加载插件
```go
package main

import (
    "fmt"
    "plugin"
)

func main(){
    p, _ := plugin.Open("plugin.so")
    versionSym, _ = p.Lookup("Version")
    version := *versionSym.(*string)
    helloSym, _ = p.Lookup("HelloWorld")
    hello := helloSym.(func())
    hello()
}
```

4. 运行主程序
主程序编译无需编译插件，会动态加载
```bash
go build -o main main.go
./main
```