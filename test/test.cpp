#include <iostream>
#include <vector>
using namespace std;

int main() {
    int a = 10;
    int* p = &a;
    cout << "p (value of a): " << p << endl;
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
