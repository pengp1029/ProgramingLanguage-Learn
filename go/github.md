# Github开源

## validate
github.com/go-playground/validator 支持数据验证
```go
type User struct {
	Name  string `validate:"required"`
	Email string `validate:"required,email"`
	Age   int    `validate:"gte=0,lte=130"`
}
func main(){
    u1 := User{Name: "111", Email: "abc@abc.com", Age: 140}
    v := validator.New()
    err := v.Struct(u1)
    if err != nil {
        fmt.Println(err)
    }
}
```