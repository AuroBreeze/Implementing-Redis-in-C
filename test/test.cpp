#include <iostream>
#include <vector>
using namespace std;


struct mode{
    int n = 3;
    int arr[2] = {0,1};
 } ;

static void qwe(std::string& s, mode* mode_1){
    s += "2345";
    cout << s << endl;
    cout<< "struct qwe: " <<mode_1->n <<endl;
    cout<< "struct qwe: " <<&mode_1->arr << endl;
}

static void send(std::string& s, mode& mode_1){
    s += "123";
    cout << s << endl;
    cout << &s << endl;
    qwe(s,&mode_1); // 这个对还是错？

    cout << "struct send: " << &mode_1 << endl;
    mode_1.n = 0;
}


int main() {
    int a = 10;
    int* p = &a;
    cout << "p (value of a): " << p << endl;
    std::string s = "123";
    cout << s << endl;
    mode mode_1;
    send(s,mode_1);

    cout << mode_1.n << endl;

    uint32_t len =1 ;
    cout << sizeof(len) << endl;
    // int p = *&a;      // 普通整数
    // int* n = &a;      // 指针，存的是 a 的地址

    // vector<int> v = {1, 2, 7, 4, 5};
    // int q = *&v[0];   // 普通整数
    // int* r = &v[0];   // 指针，指向数组第一个元素

    // cout << "p (value of a): " << p << endl;
    // cout << "p + 4 (int add): " << p + 4 << endl; // 普通值加4

    // cout << "n (pointer to a): " << n << endl;
    // cout << "n + 1 (pointer move by 1 int): " << n + 1 << endl; // 地址往后移动 sizeof(int)
    // cout << "*n: " << *n << endl;          // 解引用 -> 10
    // cout << "*(n + 1): " << *(n + 1) << " (next int in memory,未定义)" << endl;

    // cout << "q (first element of vector): " << q << endl;
    // cout << "q + 2 (int add): " << q + 2 << endl; // 值加2

    // cout << "r (pointer to first element): " << r << endl;
    // cout << "r + 2 (pointer move by 2 ints): " << r + 2 << endl;
    // cout << "*(r + 2): " << *(r + 2) << endl; // 数组第三个元素

    // std::string s = "hello world";
    // std::string &vn = s;
    // cout << vn.length() << endl;

    return 0;
}
