#include <iostream>
#include <fstream>
#include <cmath>

#define G 6.67430e-11

using namespace std;

class vector_2d
{

    public:

    double x = 0;
    double y = 0;

    vector_2d() = default;

    vector_2d(double x, double y);

    //getters

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

    //mass, radius, position, velocity, acceleration:

    double mass = 0; //kg
    double radius = 0; //m

    vector_2d position{0, 0}; //m
    vector_2d velocity{0, 0}; //m/s
    vector_2d acceleration{0, 0}; //m/s^2

    public:

    celestial_object() = default;

    celestial_object(double mass, double radius, double px, double py, double vx, double vy);

    //getters
    double get_mass() const;
    double get_radius() const;

    vector_2d get_position() const;
    vector_2d get_velocity() const;
    vector_2d get_acceleration() const;

    //setters
    void set_mass(const double& AUX);
    void set_radius(const double& AUX);

    void set_position(const vector_2d& AUX);
    void set_velocity(const vector_2d& AUX);
    void set_acceleration(const vector_2d& AUX);

    //calcul operators

    celestial_object operator+(const celestial_object& AUX) const;

    //IO operators
    friend istream& operator>>(istream& input, celestial_object& AUX);

};

class force_ij
{

    private:

        vector_2d component{0, 0};

        double modulus = 0;
        double distance = 0;

    public:

    force_ij() = default;

    //getters

    vector_2d get_component();

    double get_modulus();
    double get_distance();

    //setters

    void set_component(const vector_2d& AUX);

    void set_modulus(const double& AUX);
    void set_distance(const double& AUX);

};

void set_force(vector_2d* AUX, const celestial_object* celestial_objects, int number_of_celestial_objects);

void update_velocity(const vector_2d AUX, celestial_object& object, double time_step);

void update_position(celestial_object& object, double time_step);

int write(double time, const celestial_object& A, ostream& output);