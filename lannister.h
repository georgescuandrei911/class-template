#pragma once

#include <iostream>

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

        Vector2 operator+(const Vector2& AUX) const;

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