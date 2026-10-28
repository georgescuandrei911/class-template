//g++ *.cpp -o main && ./main

#include "star.h"

int main()
{

    const char *output = "simulation.bin";

    ofstream(output, ios::binary | ios::trunc).close();

    for(int time = 0; time <= 100; time++)
    {

        celestial_object planet(time * 1.9, time * 0.5, time * 2.6, time * 5.1, time * 7, time * 2.3);

        write(time, planet, output);

    }

}