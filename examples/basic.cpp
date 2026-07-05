#include <structural_interface.hpp>
#include <vector>
#include <print>

// interface
struct Shape {
    std::string name;   // field member
    float area() const; // function member
};

// unified function that accepts any type satisfying the Shape interface
float area(const si::satisfies<Shape> auto& shape) {
    return shape.area();
}

// custom types that satisfy the Shape interface
struct Circle {
    std::string name;
    float radius{ 0.0f };

    float area() const {
        return 3.14159f * radius * radius;
    }
};
struct Rectangle {
    std::string name;
    float width{ 0.0f };
    float height{ 0.0f };

    float area() const {
        return width * height;
    }
};

int main() {
    Circle circle{ "Circle", 5.0f };
    std::println("Circle area: {}", area(circle)); // static dispatch

    std::vector<si::existential<Shape>> shapes{ Circle{ "Circle", 3.0f }, Rectangle{ "Rectangle", 4.0f, 6.0f } };
    for (const auto& shape : shapes) {
        std::println("Shape {} area: {}", *(shape.name), shape.area()); // dynamic dispatch
    }

    *(shapes[0].name) = "Updated Circle"; // modify field
    std::println("Updated shape name: {}", *(shapes[0].name));
}
