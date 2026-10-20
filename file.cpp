#include "file.h"

//constructors

Vector2D :: Vector2D(){}

Vector2D :: Vector2D(double x, double y){ this->x = x; this->y = y; }

//getters

double Vector2D :: GetX(){ return x; }
double Vector2D :: GetY(){ return y; }

//setters

void Vector2D :: Update(double x, double y){ this->x = x; this->y = y; }

void Vector2D :: UpdateX(double x){ this->x = x; }
void Vector2D :: UpdateY(double y){ this->y = y; }

Vector2D Vector2D :: operator+(const Vector2D& other) const { return Vector2D(x + other.x, y + other.y); }
Vector2D Vector2D :: operator+(const double& other) const { return Vector2D(x + other, y + other); }

Vector2D Vector2D :: operator-(const Vector2D& other) const { return Vector2D(x - other.x, y - other.y); }
Vector2D Vector2D :: operator-(const double& other) const { return Vector2D(x - other, y - other); }

