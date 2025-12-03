/*

# 访问说明符
## 分类
`public`:共有的，任何外部代码都能访问
`private`:私有的，仅类自身的成员函数、友元可访问
`protected`:保护的，仅类自身、子类的成员函数、友元能访问

## 默认权限
- 类默认权限是private
- 结构体默认权限是public

# 友元
友元是类外部的函数或类，可以访问类的私有成员和保护成员。通过在类定义中使用`friend`关键字声明。
*/

#include<iostream>
class BaseFriend;

class Base{
    private:
        friend void friendPrint();
        friend class BaseFriend;
        int privateVar;
    protected:
        int protectedVar;
        void BasePrintPrivate(){
            std::cout << privateVar << std::endl;
        }
    public:
        int publicVar;
        Base(): privateVar(1), protectedVar(2), publicVar(3) {}
};

class BaseFriend{
    public:
        void printBasePrivate(Base& b){
            std::cout << b.privateVar << std::endl;
        }
};

void friendPrint(){
    Base b;
    std::cout << b.privateVar << std::endl;
}

class Derived : public Base{
    public:
        Derived() = default;
        ~Derived() = default;
        void printPrivate(){
            // std::cout << privateVar << std::endl;
            BasePrintPrivate();
        }
        void printProtected(){
            std::cout << protectedVar << std::endl;
        }
        void printPublic(){
            std::cout << publicVar << std::endl;
        }
        void callFriendPrint(){
            friendPrint();
        }
};


int main(){
    Derived d;
    d.printPrivate();
    d.printProtected();
    d.printPublic();

    friendPrint();
    d.callFriendPrint();

    BaseFriend bf;
    bf.printBasePrivate(d);
}