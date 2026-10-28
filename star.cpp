#include "star.h"

vector_2d :: vector_2d(double x, double y)
{

    this->x = x;
    this->y = y;

}

vector_2d vector_2d :: operator+(const vector_2d& AUX) const { return vector_2d(x + AUX.x, y + AUX.y); }
vector_2d vector_2d :: operator*(const vector_2d& AUX) const { return vector_2d(x * AUX.x, y * AUX.y); }
vector_2d vector_2d :: operator*(const double AUX) const  { return vector_2d(x * AUX, y * AUX); }

//IO operators
istream& operator>>(istream& input, vector_2d& vector){ input >> vector.x >> vector.y; return input; }
ostream& operator<<(ostream& output, const vector_2d& vector){ output << vector.x << " " << vector.y; return output; }

celestial_object :: celestial_object(double mass, double radius, double px, double py, double vx, double vy)
{

    this->mass = mass;
    this->radius = radius;

    position.x = px; position.y = py;
    velocity.x = vx; velocity.y = vy;

}

double celestial_object :: get_mass(){ return mass; }
double celestial_object :: get_radius(){ return radius; }

vector_2d celestial_object :: get_position(){ return position; }
vector_2d celestial_object :: get_velocity(){ return velocity; }
vector_2d celestial_object :: get_acceleration(){ return acceleration; }

force_ij :: force_ij() {}

vector_2d force_ij :: get_component(){ return vector_2d(component.x, component.y); }
double force_ij :: get_modulus(){ return modulus; }
double force_ij :: get_distance(){ return distance; }

void force_ij :: set_component(const vector_2d& AUX) { component = AUX; }
void force_ij :: set_modulus(const double& AUX) { modulus = AUX; }
void force_ij :: set_distance(const double& AUX) { distance = AUX; }

int write(double time, const celestial_object& A, const char* file)
{
    static_assert(is_trivially_copyable_v<celestial_object>, "object requires custom serialization");

    ofstream output(file, ios::binary | ios::app);

    if (!output) { cerr << "could not open " << file << '\n'; return 1; }

    output.write(reinterpret_cast<const char*>(&time), sizeof(time));

    output.write(reinterpret_cast<const char*>(&A), sizeof(A));

    output.close();

    if (!output) { cerr << "could not write " << file << endl; return 1; }

    return 0;

}