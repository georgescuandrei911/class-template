#pragma once

#include <iostream>

class Vector2
{

    private:

        //x and y
        
        double x = 0;
        double y = 0;

    public:

        Vector2();
        Vector2(double x);
        Vector2(double y);
        Vector2(double x, double y);

};

class Math
{

    public:

        //constant

        static const double pi;
        static const double e;
        static const double G;

};