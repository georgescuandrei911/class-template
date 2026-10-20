#include "lannister.h"

//vector2

Vector2 :: Vector2() {}
Vector2 :: Vector2(double x, double y){ this->x = x; this->y = y; }



//math

//constant

const double Math::pi = 3.141592653589793;
const double Math::e  = 2.718281828459045;
const double Math::G  = 6.67430e-11;

double Math :: dot_product(const Vector2& v1, const Vector2& v2){ return (v1.x * v2.x + v1.y * v2.y);}