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

void merge_celestial_objects( celestial_object* objects, int* count, int A, int B)
{

    const double ma = objects[A].get_mass();
    const double mb = objects[B].get_mass();
    const double mass = ma + mb;

    const vector_2d position =
        (objects[A].get_position() * ma
       + objects[B].get_position() * mb) * (1.0 / mass);

    const vector_2d velocity =
        (objects[A].get_velocity() * ma
       + objects[B].get_velocity() * mb) * (1.0 / mass);

    const double ra = objects[A].get_radius();
    const double rb = objects[B].get_radius();

    const double radius =
        std::cbrt(ra * ra * ra + rb * rb * rb);

    objects[A] = celestial_object(
        mass, radius,
        position.x, position.y,
        velocity.x, velocity.y
    );

    for (int k = B; k < *count - 1; ++k)
        objects[k] = objects[k + 1];

    --(*count);
}

void resolve_collisions(celestial_object* objects, int& count)
{
    bool merged;

    do
    {
        merged = false;

        for (int i = 0; i < count - 1 && !merged; ++i)
        {
            for (int j = i + 1; j < count; ++j)
            {
                const vector_2d a = objects[i].get_position();
                const vector_2d b = objects[j].get_position();

                const double distance =
                    std::hypot(b.x - a.x, b.y - a.y);

                const double total_radius =
                    objects[i].get_radius() + objects[j].get_radius();

                if (distance <= total_radius)
                {
                    merge_celestial_objects(objects, &count, i, j);
                    merged = true;
                    break;
                }
            }
        }
    }
    while (merged);
}

void set_force( vector_2d* forces, celestial_object* objects, int& count)
{
    resolve_collisions(objects, count);

    for (int i = 0; i < count; ++i)
        forces[i] = vector_2d(0, 0);

    for (int i = 0; i < count - 1; ++i)
    {
        for (int j = i + 1; j < count; ++j)
        {
            const vector_2d a = objects[i].get_position();
            const vector_2d b = objects[j].get_position();

            const vector_2d displacement(b.x - a.x, b.y - a.y);
            const double distance =
                std::hypot(displacement.x, displacement.y);

            if (distance == 0.0)
                continue;

            const double magnitude =
                G * objects[i].get_mass() * objects[j].get_mass()
                / distance / distance;

            const vector_2d force =
                displacement * (magnitude / distance);

            forces[i] = forces[i] + force;
            forces[j] = forces[j] + force * (-1.0);
        }
    }
}

void update_velocity(const vector_2d AUX, celestial_object& object, double time_step)
{

    object.set_acceleration(vector_2d(AUX.x / object.get_mass(), AUX.y / object.get_mass()));

    object.set_velocity(vector_2d(object.get_velocity().x + object.get_acceleration().x * time_step,
                                  object.get_velocity().y + object.get_acceleration().y * time_step));

}

void update_position(celestial_object& object, double time_step)
{

    object.set_position(vector_2d(object.get_position().x + object.get_velocity().x * time_step,
                                  object.get_position().y + object.get_velocity().y * time_step));

}

double total_energy(const celestial_object* objects, int count)
{
    double energy = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const vector_2d velocity = objects[i].get_velocity();

        energy += 0.5 * objects[i].get_mass()
                * (velocity.x * velocity.x + velocity.y * velocity.y);

        for (int j = i + 1; j < count; ++j)
        {
            const vector_2d a = objects[i].get_position();
            const vector_2d b = objects[j].get_position();

            const double distance = std::hypot(b.x - a.x, b.y - a.y);

            // Requires distinct centers: distance > 0.
            energy -= G * objects[i].get_mass()
                        * objects[j].get_mass() / distance;
        }
    }

    return energy;
}

int write(double time, const celestial_object& A, ostream& output)
{
    static_assert(
        is_trivially_copyable_v<celestial_object>,
        "object requires custom serialization"
    );

    output.write(reinterpret_cast<const char*>(&time), sizeof(time));

    const vector_2d position = A.get_position();

    output.write(reinterpret_cast<const char*>(&position), sizeof(position));

    if (!output)
    {
        std::cerr << "could not write simulation record\n";
        return 1;
    }

    return 0;
}

