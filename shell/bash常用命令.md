# bash脚本

shell是空格敏感性解析器，将空格当成命令分隔符和语法界定符

`set -x` 打印执行的命令，在日志中会出现`+ command`的输出

如果想要调用shell指令，需要用\`指令\`的方式调用，或者使用$(指令)的方式调用，如：
```bash
current_dir=$(pwd)
current_dir=`pwd`
```
