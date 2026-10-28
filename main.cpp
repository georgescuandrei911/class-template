//g++ *.cpp -o main && ./main

#include "star.h"

int main()
{

    const char *output = "simulation.bin";
    const char *input = "input.txt";

    ifstream in(input);

    int number_of_celestial_objects;
    double max_time, time_step;

    in >> number_of_celestial_objects >> max_time >> time_step;
    ofstream(output, ios :: binary | ios :: trunc).close();

    ofstream out(output, ios :: binary | ios :: app);
    if (!out) { cerr << "could not open " << output << endl; return 1; }

    out.write(reinterpret_cast<const char*>(&number_of_celestial_objects), sizeof(number_of_celestial_objects));

    out.write(reinterpret_cast<const char*>(&max_time), sizeof(max_time));

    out.write(reinterpret_cast<const char*>(&time_step), sizeof(time_step));

    out.close();

    celestial_object* celestial_objects = 
    new celestial_object[number_of_celestial_objects];

    for(int i = 0; i < number_of_celestial_objects; i++) in >> celestial_objects[i];


    vector_2d* forces = new vector_2d[number_of_celestial_objects];

    for(double time = 0; time <= max_time; time += time_step)
    {

        set_force(forces, celestial_objects, number_of_celestial_objects);

        for(int j = 0; j < number_of_celestial_objects; j++)
        {

            write(time, celestial_objects[j], output);

            update(forces[j], celestial_objects[j], time_step);

        }

        if((int)time % 100 == 0) cout << 100 * time / max_time << "'%' completed" << endl;

    }

    delete[] celestial_objects;
    delete[] forces;

}