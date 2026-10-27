#include "lannister.h"

//vector2

Vector2 :: Vector2() {}
Vector2 :: Vector2(double x, double y){ this->x = x; this->y = y; }

//calcul operators
Vector2 Vector2 :: operator+(const Vector2& AUX) const { return Vector2(x + AUX.x, y + AUX.y); }
Vector2 Vector2 :: operator*(const Vector2& AUX) const { return Vector2(x * AUX.x, y * AUX.y); }
Vector2 Vector2 :: operator*(const double AUX) const  {return Vector2(x * AUX, y * AUX); }

//IO operators
istream& operator>>(istream& input, Vector2& vector){ input >> vector.x >> vector.y; return input; }
ostream& operator<<(ostream& output, const Vector2& vector){ output << vector.x << " " << vector.y; return output; }



//math

//constant

const double Math::pi = 3.141592653589793;
const double Math::e  = 2.718281828459045;
const double Math::G  = 6.67430e-11;

double Math :: dot_product(const Vector2& v1, const Vector2& v2){ return (v1.x * v2.x + v1.y * v2.y);}

void readFile(const char* filename) {
    std::ifstream file(filename);

    if (!file) {
        std::cerr << "Could not open file\n";
        return;
    }

    char line[256];

    while (file.getline(line, sizeof(line))) {
        char surname[50], name[50];
        int year;
        double value;

        int count = std::sscanf(
            line, " %49[^,], %d, %lf, %49s",
            surname, &year, &value, name
        );

        if (count == 4) {
            std::cout << surname << " "
                      << year << " "
                      << value << " "
                      << name << '\n';
        }
    }
}

//lab5

CorpCeresc::CorpCeresc(double m) : masa(m) {}

void CorpCeresc::afisare() {
    std::cout << "Masa: " << masa << '\n';
}

Stea::Stea(double m, double t)
    : CorpCeresc(m), temperatura(t) {}

void Stea::afisare() {
    CorpCeresc::afisare();
    std::cout << "Temperatura: " << temperatura << '\n';
}