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

//getters

double celestial_object :: get_mass() const { return mass; }
double celestial_object :: get_radius() const { return radius; }

vector_2d celestial_object :: get_position() const { return position; }
vector_2d celestial_object :: get_velocity() const { return velocity; }
vector_2d celestial_object :: get_acceleration() const { return acceleration; }

//setters

void celestial_object :: set_mass(const double& AUX) { mass = AUX; }
void celestial_object :: set_radius(const double& AUX) { radius = AUX; }

void celestial_object :: set_position(const vector_2d& AUX) { position = AUX; }
void celestial_object :: set_velocity(const vector_2d& AUX) { velocity = AUX; }
void celestial_object :: set_acceleration(const vector_2d& AUX) { acceleration = AUX; }

celestial_object celestial_object :: operator+(const celestial_object& AUX) const 
{ 
    
    return celestial_object(mass + AUX.mass, radius + AUX.radius,
                            position.x + AUX.position.x, position.y + AUX.position.y,
                            velocity.x + AUX.velocity.x, velocity.y + AUX.velocity.y);
}

istream& operator>>(istream& input, celestial_object& AUX)
{ 
    
    input >> AUX.mass >> AUX.radius

    >> AUX.position.x >> AUX.position.y
    >> AUX.velocity.x >> AUX.velocity.y;

    AUX.set_acceleration(vector_2d(0, 0));

    return input;

}

vector_2d force_ij :: get_component(){ return vector_2d(component.x, component.y); }
double force_ij :: get_modulus(){ return modulus; }
double force_ij :: get_distance(){ return distance; }

void force_ij :: set_component(const vector_2d& AUX) { component = AUX; }
void force_ij :: set_modulus(const double& AUX) { modulus = AUX; }
void force_ij :: set_distance(const double& AUX) { distance = AUX; }

void set_force(vector_2d* AUX, const celestial_object* celestial_objects, int number_of_celestial_objects)
{

    for(int i = 0; i < number_of_celestial_objects; i++)AUX[i] = vector_2d(0, 0);

    for(int i = 0; i < number_of_celestial_objects - 1; i++)
    {

        for(int j = i + 1; j < number_of_celestial_objects; j++)
        {

            const vector_2d distance(celestial_objects[j].get_position().x - celestial_objects[i].get_position().x,
                                        celestial_objects[j].get_position().y - celestial_objects[i].get_position().y);

            const double hypot_value = hypot(distance.x, distance.y);

            if(hypot_value == 0.0)continue; 

            const double modulus = G * celestial_objects[i].get_mass() * celestial_objects[j].get_mass() / 
                             hypot_value / hypot_value;

            AUX[i].x = AUX[i].x + modulus * distance.x / hypot_value;
            AUX[i].y = AUX[i].y + modulus * distance.y / hypot_value;

            AUX[j].x = AUX[j].x - modulus * distance.x / hypot_value;
            AUX[j].y = AUX[j].y - modulus * distance.y / hypot_value;

        }

    }
}

void update(const vector_2d AUX, celestial_object& object, double time_step)
{

    object.set_acceleration(vector_2d(AUX.x / object.get_mass(), AUX.y / object.get_mass()));

    object.set_velocity(vector_2d(object.get_velocity().x + object.get_acceleration().x * time_step,
                                  object.get_velocity().y + object.get_acceleration().y * time_step));
                                  

    object.set_position(vector_2d(object.get_position().x + object.get_velocity().x * time_step,
                                  object.get_position().y + object.get_velocity().y * time_step));

}

int write(double time, const celestial_object& A, const char* file)
{
    static_assert(is_trivially_copyable_v<celestial_object>, "object requires custom serialization");

    ofstream output(file, ios :: binary | ios :: app);

    if (!output) { cerr << "could not open " << file << endl; return 1; }

    output.write(reinterpret_cast<const char*>(&time), sizeof(time));

    output.write(reinterpret_cast<const char*>(&A), sizeof(A));

    output.close();

    if (!output) { cerr << "could not write " << file << endl; return 1; }

    return 0;

}