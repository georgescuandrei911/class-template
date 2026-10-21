//g++ *.cpp -o main && ./main

#include "lannister.h"

int main()
{

    Vector2 A(2, 3);
    int a;

    cin >> A >> a;
    cout << Math :: dot_product(Vector2(2, 3), Vector2(4, 6)) << endl;
    cout << A * a;

    return 0;

}