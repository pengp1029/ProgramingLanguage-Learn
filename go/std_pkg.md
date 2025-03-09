# Go标准库

## flag
1. 命令行解析参数
```go
// 可以在运行的时候通过 -flagename=123 来设置 flagname 的值，如果不设置则使用默认值
flag.IntVar(&flagname, "flagname", 123, "help message for flagname")
```



## regexp
1. 正则表达式替换
```go
var r = regexp.MustCompile("\\babc\\b")
content := "abc 1234 def"
content_new := r.ReplaceAllFunc([]byte(content), func(match []byte) []byte{
    return []byte("change")
})
```