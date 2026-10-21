#pragma once

#include <iostream>
#include <cstdio>
#include <fstream>

using namespace std;

class Vector2
{

    public:

        //x and y
        
        double x = 0;
        double y = 0;

        //constructors

        Vector2();
        Vector2(double x, double y);

        //calcul operators
        Vector2 operator+(const Vector2& AUX) const;
        Vector2 operator*(const Vector2& AUX) const;
        Vector2 operator*(const double AUX) const;

        //IO operators
        friend istream& operator>>(istream& input, Vector2& vector);
        friend ostream& operator<<(ostream& output, const Vector2& vector);
        
};

class Math
{

    public:

        //constant

        static const double pi;
        static const double e;
        static const double G;

        static double dot_product(const Vector2& v1, const Vector2& v2);

};

