//g++ *.cpp -o main && ./main

#include "star.h"

#include <iomanip>


int main()
{
    const char* output = "simulation.bin";
    const char* input = "input.txt";

    ifstream in(input);

    std::cout << setprecision(17);

    int number_of_celestial_objects;
    double max_time, time_step;

    if (!(in >> number_of_celestial_objects >> max_time >> time_step)
        || number_of_celestial_objects <= 0
        || max_time < 0
        || time_step <= 0)
    {
        cerr << "invalid simulation settings\n";
        return 1;
    }

    ofstream out(output, ios::binary | ios::trunc);

    if (!out)
    {
        cerr << "could not open " << output << endl;
        return 1;
    }

    out.write(
        reinterpret_cast<const char*>(&number_of_celestial_objects),
        sizeof(number_of_celestial_objects)
    );

    out.write(
        reinterpret_cast<const char*>(&max_time),
        sizeof(max_time)
    );

    out.write(
        reinterpret_cast<const char*>(&time_step),
        sizeof(time_step)
    );

    celestial_object* celestial_objects =
        new celestial_object[number_of_celestial_objects];

    for (int i = 0; i < number_of_celestial_objects; i++)
    {
        if (!(in >> celestial_objects[i]))
        {
            cerr << "could not read object " << i << '\n';
            delete[] celestial_objects;
            return 1;
        }
    }

    in.close();

    vector_2d* forces = new vector_2d[number_of_celestial_objects];

    const double initial_energy =
        total_energy(celestial_objects, number_of_celestial_objects);

    double max_energy = initial_energy;
    double max_error = 0.0;

    for (double time = 0; time <= max_time; time += time_step)
    {
        set_force(
            forces, celestial_objects, number_of_celestial_objects
        );

        double current_energy =
            total_energy(celestial_objects, number_of_celestial_objects);

        if (current_energy > max_energy)
            max_energy = current_energy;

        if (abs(current_energy - initial_energy) > max_error)
            max_error = abs(current_energy - initial_energy);

        // Save time and current body count.
        out.write(
            reinterpret_cast<const char*>(&time),
            sizeof(time)
        );

        out.write(
            reinterpret_cast<const char*>(&number_of_celestial_objects),
            sizeof(number_of_celestial_objects)
        );

        // Save positions before updating any bodies.
        for (int j = 0; j < number_of_celestial_objects; j++)
        {
            const vector_2d position =
                celestial_objects[j].get_position();

            out.write(
                reinterpret_cast<const char*>(&position.x),
                sizeof(double)
            );

            out.write(
                reinterpret_cast<const char*>(&position.y),
                sizeof(double)
            );
        }

        if (!out)
        {
            cerr << "could not write simulation step\n";
            delete[] celestial_objects;
            delete[] forces;
            return 1;
        }

        // First velocity half-step.
        for (int j = 0; j < number_of_celestial_objects; j++)
        {
            update_velocity(
                forces[j], celestial_objects[j], time_step / 2
            );
        }

        // Position full-step.
        for (int j = 0; j < number_of_celestial_objects; j++)
        {
            update_position(celestial_objects[j], time_step);
        }

        set_force(
            forces, celestial_objects, number_of_celestial_objects
        );

        // Second velocity half-step.
        for (int j = 0; j < number_of_celestial_objects; j++)
        {
            update_velocity(
                forces[j], celestial_objects[j], time_step / 2
            );
        }

        if ((int)time % 100000 == 0)
        {
            std::cout << 100 * time / max_time
                      << "% completed" << endl;
        }
    }

    delete[] celestial_objects;
    delete[] forces;

    out.close();

    if (!out)
    {
        cerr << "could not finish writing output\n";
        return 1;
    }

    std::cout << "initial energy: " << initial_energy
              << " maximum energy: " << max_energy << endl;

    std::cout << "energy error: "
              << abs(initial_energy - max_energy) << endl;

    std::cout << "max energy error: " << max_error << endl;

    return 1;
}