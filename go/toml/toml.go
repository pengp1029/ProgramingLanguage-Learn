/*
自定义类型序列化与反序列化
*/
package main

import (
	"bytes"
	"fmt"
	"log"

	"github.com/BurntSushi/toml"
)

// 自定义类型
type MyCustomType string

// 实现 encoding.TextMarshaler 接口
// 序列化
func (m MyCustomType) MarshalText() ([]byte, error) {
	return []byte(fmt.Sprintf("CUSTOM:%s", m)), nil
}

// 实现 encoding.TextUnmarshaler 接口
// 反序列化
func (m *MyCustomType) UnmarshalText(text []byte) error {
	prefix := []byte("CUSTOM:")
	if len(text) < len(prefix) || !equalFold(text[:len(prefix)], prefix) {
		return fmt.Errorf("invalid MyCustomType format")
	}
	*m = MyCustomType(text[len(prefix):])
	return nil
}

// equalFold 是用来比较两个字节切片是否不区分大小写相等的简单实现
func equalFold(a, b []byte) bool {
	for i := range a {
		if lower(a[i]) != lower(b[i]) {
			return false
		}
	}
	return true
}

func lower(b byte) byte {
	if b >= 'A' && b <= 'Z' {
		return b + ('a' - 'A')
	}
	return b
}

// 配置结构体
type Config struct {
	Name   string
	Custom MyCustomType
}

func main() {
	// 示例配置数据
	configData := `
Name = "example"
Custom = "CUSTOM:myvalue"
`

	var config Config

	// 反序列化 TOML 数据到结构体
	if _, err := toml.Decode(configData, &config); err != nil {
		log.Fatalf("Error decoding TOML: %v", err)
	}

	fmt.Printf("Decoded config: %+v\n", config)

	// 序列化结构体到 TOML 数据
	var encodedConfig bytes.Buffer
	if err := toml.NewEncoder(&encodedConfig).Encode(config); err != nil {
		log.Fatalf("Error encoding TOML: %v", err)
	}

	fmt.Printf("Encoded config:\n%s\n", encodedConfig.String())
}
