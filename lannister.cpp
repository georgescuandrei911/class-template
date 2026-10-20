#include "lannister.h"

//vector2

Vector2 :: Vector2() {}
Vector2 :: Vector2(double x){ this->x = x; }
Vector2 :: Vector2(double y){ this->y = y; }
Vector2 :: Vector2(double x, double y){ this->x = x; this->y = y; }



//math

//constant

const double Math::pi = 3.141592653589793;
const double Math::e  = 2.718281828459045;
const double Math::G  = 6.67430e-11;