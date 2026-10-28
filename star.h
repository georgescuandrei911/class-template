#include <iostream>

using namespace std;

class vector_2d
{

    private:

    double x;
    double y;

    public:

    vector_2d(double x, double y);

    //calcul operators
    vector_2d operator+(const vector_2d& AUX) const;
    vector_2d operator*(const vector_2d& AUX) const;
    vector_2d operator*(const double AUX) const;

    //IO operators
    friend istream& operator>>(istream& input, vector_2d& vector);
    friend ostream& operator<<(ostream& output, const vector_2d& vector);

};

class celestial_object
{

    private:

    //mass, position, velocity, acceleration:

    double mass = 0; //kg

    vector_2d position{0, 0}; //m
    vector_2d velocity{0, 0}; //m/s
    vector_2d acceleration{0, 0}; //m/s^2

};