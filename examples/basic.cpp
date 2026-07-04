#include <structural_interface.hpp>
#include <vector>
#include <print>

// interface
struct Shape {
    float area() const;
};

// unified function that accepts any type satisfying the Shape interface
float area(const si::satisfies<Shape> auto& shape) {
    return shape.area();
}

// custom types that satisfy the Shape interface
struct Circle {
    float radius{ 0.0f };

    float area() const {
        return 3.14159f * radius * radius;
    }
};
struct Rectangle {
    float width{ 0.0f };
    float height{ 0.0f };

    float area() const {
        return width * height;
    }
};

int main() {
    Circle circle{ 5.0f };
    std::println("Circle area: {}", area(Circle{ 5.0f })); // static dispatch

    std::vector<si::existential<Shape>> shapes{ Circle{ 3.0f }, Rectangle{ 4.0f, 6.0f } };
    for (const auto& shape : shapes) {
        std::println("Shape area: {}", area(shape)); // dynamic dispatch
    }
}
