#include <iostream>

using namespace std;

class Vector2D
{

    private:

        double x = 0;
        double y = 0;
        double z = 0;
    
    public:

        //constructors

        Vector2D();
        Vector2D(double x, double y);

        //getters
        double GetX();
        double GetY();

        //setters
        void Update(double x, double y);
        void UpdateX(double x);
        void UpdateY(double y);
        
        //operator+
        Vector2D operator+(const Vector2D& other) const;
        Vector2D operator+(const double& other) const;

        //operator-
        Vector2D operator-(const Vector2D& other) const;
        Vector2D operator-(const double& other) const;


};