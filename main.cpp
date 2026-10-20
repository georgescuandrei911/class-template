//g++ *.cpp -o main && ./main

#include "lannister.h"

int main()
{

    Vector2 A(2, 3);
    Vector2 B(5, 5);

    A = A + B;

    cout << A.x << " " << A.y << endl;

    return 0;

}