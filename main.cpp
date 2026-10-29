//g++ *.cpp -o main && ./main

#include "star.h"

#include <cstdlib>
#include <string>

#include <iomanip>

int run_simulation(const char* file)
{
    const string command =
        string(".venv/bin/python \"graph.py\" \"") + file + "\"";

    return system(command.c_str());
}

int main()
{

    const char *output = "simulation.bin";
    const char *input = "input.txt";

    ifstream in(input);

    std :: cout << setprecision(17);

    int number_of_celestial_objects;
    double max_time, time_step;

    in >> number_of_celestial_objects >> max_time >> time_step;
    
    ofstream out(output, ios::binary | ios::trunc);

    if (!out) { cerr << "could not open " << output << endl; return 1; }

    out.write(reinterpret_cast<const char*>(&number_of_celestial_objects), sizeof(number_of_celestial_objects));

    out.write(reinterpret_cast<const char*>(&max_time), sizeof(max_time));

    out.write(reinterpret_cast<const char*>(&time_step), sizeof(time_step));

    celestial_object* celestial_objects = 
    new celestial_object[number_of_celestial_objects];

    for(int i = 0; i < number_of_celestial_objects; i++) in >> celestial_objects[i];

    in.close();

    vector_2d* forces = new vector_2d[number_of_celestial_objects];

    const double initial_energy = total_energy(celestial_objects, number_of_celestial_objects);

    double max_energy = initial_energy;

    double max_error = abs(initial_energy - max_energy);

    for(double time = 0; time <= max_time; time += time_step)
    {

        set_force(forces, celestial_objects, number_of_celestial_objects);

        double current_energy = total_energy(celestial_objects, number_of_celestial_objects);

        if(current_energy > max_energy)max_energy = current_energy;

        if(abs(current_energy - initial_energy) > max_error)max_error = abs(current_energy - initial_energy);

        out.write(reinterpret_cast<const char*>(&time), sizeof(time));
        out.write(reinterpret_cast<const char*>(&forces[0]), sizeof(forces[0]));

        //update velocity t = t / 2

        for(int j = 0; j < number_of_celestial_objects; j++)
        {

            if (write(time, celestial_objects[j], out) != 0)
            {
                delete[] celestial_objects;
                delete[] forces;
                return 1;
            }

            if(j != 0)update_velocity(forces[j], celestial_objects[j], time_step / 2);

        }

        //update position t = t

        for(int j = 0; j < number_of_celestial_objects; j++)
        {
            
            if(j != 0) update_position(celestial_objects[j], time_step);

        }

        //update velocity t = t

        set_force(forces, celestial_objects, number_of_celestial_objects);

        for(int j = 0; j < number_of_celestial_objects; j++)
        {

            if(j != 0) update_velocity(forces[j], celestial_objects[j], time_step / 2);

        }

        if((int)time % 10000000 == 0) std :: cout << 100 * time / max_time << "'%' completed" << endl;

    }

    delete[] celestial_objects;
    delete[] forces;

    out.close();

    std :: cout << "initial energy : " << initial_energy << " maximum energy: " << max_energy << endl;

    std :: cout << "energy error: " << abs(initial_energy - max_energy) << endl;

    std :: cout << "max energy error: " << max_error << endl;

    return 1;

}