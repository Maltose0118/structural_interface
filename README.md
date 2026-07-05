# Structural Interface

Structural Interface (SI) is a C++26 static-reflection-based, header-only experimental library that lets ordinary `struct`s serve as both generic constraint interfaces and runtime type-erased interfaces — without macros, inheritance from special base classes, or external code generators.

## Quick Example

```cpp
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
```

The full example can be found at [examples/basic.cpp](examples/basic.cpp).

## Goals

SI describes **syntactic interfaces**: what members a type publicly exposes, without constraining their semantics.

```cpp
struct Shape {
    float area() const;
};

struct Circle {
    float radius{};

    float area() const {
        return 3.14159f * radius * radius;
    }
};

static_assert(si::satisfies<Circle, Shape>);
```

Interface definitions require **no macros, no inheritance from special base classes, and no external code generators**.

## Features

- `si::satisfies<T, I>` — checks whether type `T` satisfies interface `I` at compile time.
- `si::satisfies<I> auto` — abbreviated function template constraint.
- `si::existential<I>` — owning, copyable runtime interface object.
- `si::existential_move_only<I>` — owning, move-only runtime interface object.
- `si::existential_ref<I>` — non-owning reference runtime interface object.
- Exact matching of member function names, parameters, return types, cv/ref qualifiers, and `noexcept`.
- Runtime interface objects expose interface members through the dot operator: callable members can be written as `obj.draw()`, while data members are exposed as raw field pointers accessible via `*(obj.field)`.
- Interface inheritance composition.
- Private / protected members do not match by default.

## Build & Test

The project uses xmake for building and boost.ut as the test framework.

It currently requires a GCC that supports C++26 static reflection, with the following flags enabled:

```text
-std=c++26 -freflection
```

Build and run tests:

```sh
xmake test -v
```

Build and run the example:

```sh
xmake build -v structural_interface_example_basic
xmake run structural_interface_example_basic
```

## Current Limitations

- Runtime callable members with the same name as the interface but with multiple overloads are supported for ordinary function calls, but the runtime proxy for overloaded members still needs further design.
- Data members are currently exposed as raw field pointers, e.g. `obj.name` has type `std::string*`.

For a more detailed design description, see [doc/design.en.md](doc/design.en.md).
