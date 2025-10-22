package main

import "fmt"

var c chan int
var multic = make(chan int, 10)

func init() {
	c = make(chan int)
	select {
	case <-c:
		fmt.Println("c")
	case <-multic:
		fmt.Println("multic")
	}
}
