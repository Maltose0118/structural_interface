# Structural Interface

Structural Interface (SI) is a C++26 static-reflection-based, header-only experimental library that lets ordinary `struct`s serve as both generic constraint interfaces and runtime type-erased interfaces — without macros, inheritance from special base classes, or external code generators.

## Quick Example

### Step 1: Non-intrusive dynamic dispatch

Define an interface as a plain `struct`. Any type satisfying it can be type-erased into an `existential` and dispatched dynamically:

```cpp
#include <structural_interface.hpp>

// Define an interface: a struct with the required members
struct Drawable {
    void draw() const;
};

// Concrete types that satisfy the Drawable interface
struct Circle {
    void draw() const { std::println("Circle"); }
};

struct Rectangle {
    void draw() const { std::println("Rectangle"); }
};

static_assert(si::satisfies<Circle, Drawable>);
static_assert(si::satisfies<Rectangle, Drawable>);

int main() {
    // Type-erased container: any type satisfying Drawable is accepted
    std::vector<si::existential<Drawable>> shapes{ Circle{}, Rectangle{} };

    for (const auto& shape : shapes) {
        shape.draw(); // dynamic dispatch
    }
}
```

No macros, no base class inheritance, no code generators required — just C++26.

### Step 2: One algorithm, both static and dynamic dispatch

Write a single generic function that satisfies `si::satisfies<Drawable> auto` — it works with concrete types (static dispatch) and existential wrappers (dynamic dispatch) alike:

```cpp
// A single algorithm that works with both static and dynamic dispatch
void render(const si::satisfies<Drawable> auto& obj) {
    obj.draw();
}

int main() {
    Circle circle{};
    render(circle); // static dispatch, zero overhead

    si::existential<Drawable> obj = Rectangle{};
    render(obj);    // dynamic dispatch, same function
}
```

One interface, one algorithm, two dispatch modes.

### Step 3: Interface composition

Interfaces can inherit multiple parent interfaces. A composed interface requires satisfying all of them simultaneously (`Widget = Drawable ∩ Scalable`):

```cpp
struct Drawable {
    void draw() const;
};

struct Scalable {
    void scale(float factor);
};

// Composed interface = Drawable + Scalable
struct Widget : Drawable, Scalable {};

// A concrete type that satisfies both Drawable and Scalable
struct Button {
    void draw() const { std::println("Button"); }
    void scale(float factor) { std::println("  scaled by {}", factor); }
};

static_assert(si::satisfies<Button, Widget>);

int main() {
    std::vector<si::existential<Widget>> widgets{ Button{} };
    for (const auto& w : widgets) {
        w.draw();
        w.scale(2.0f);
    }
}
```

### Step 4: Interfaces with data members

Interfaces can declare data members too. Extend `Drawable` with a `name` field, and runtime wrappers expose it as a raw field pointer (`*(obj.field)`):

```cpp
struct Drawable {
    std::string name; // data member
    void draw() const; // function member
};

struct Circle {
    std::string name;
    void draw() const { std::println("{}", name); }
};

struct Rectangle {
    std::string name;
    void draw() const { std::println("{}", name); }
};

int main() {
    std::vector<si::existential<Drawable>> shapes{ Circle{ "circle" }, Rectangle{ "rect" } };
    for (const auto& shape : shapes) {
        // shape.name has type std::string*, not std::string
        std::println("{}:", *(shape.name));
        shape.draw();
    }
}
```

### Step 5: Runtime type inspection

Runtime wrappers provide the free functions `si::is<T>` and `si::get<T>` for exact concrete-type inspection. `get<T>` returns a reference and does not transfer ownership; a mismatch throws `std::bad_cast`.

```cpp
si::existential<Drawable> shape = Circle{ "circle" };

if (si::is<Circle>(shape)) {
    Circle& circle = si::get<Circle>(shape);
    circle.draw();
}

const auto& const_shape = shape;
const Circle& const_circle = si::get<Circle>(const_shape);

Circle circle{ "borrowed" };
si::existential_ref<Drawable> reference = circle;
Circle& borrowed = si::get<Circle>(reference);
borrowed.name = "updated";
```

`si::existential_ref<const I>` provides a read-only view and `get<T>` returns `const T&`.

```cpp
const Circle circle{ "read-only" };
si::existential_ref<const Drawable> reference = circle;
const Circle& value = si::get<Circle>(reference);
```

The full example can be found at [examples/basic.cpp](examples/basic.cpp).

## Goals

SI describes **syntactic interfaces**: what members a type publicly exposes, without constraining their semantics.

```cpp
struct Drawable {
    void draw() const;
};

struct Circle {
    void draw() const { std::println("Circle"); }
};

static_assert(si::satisfies<Circle, Drawable>);
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
