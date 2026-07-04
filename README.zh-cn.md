# Structural Interface

Structural Interface（SI）是一个基于 C++26 静态反射的 header-only 结构化接口实验库。它让普通 `struct` 同时作为泛型约束接口和运行时类型擦除接口使用。

## 基本用法

```cpp
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
    std::println("Circle area: {}", area(Circle{ 5.0f })); // static dispatch
    std::println("Rectangle area: {}", area(Rectangle{ 10.0f, 5.0f })); // static dispatch

    std::vector<si::existential<Shape>> shapes{ Circle{ 3.0f }, Rectangle{ 4.0f, 6.0f } };
    for (const auto& shape : shapes) {
        std::println("Shape area: {}", area(shape)); // dynamic dispatch
    }
}
```

完整示例在 [examples/basic.cpp](examples/basic.cpp)。

## 目标

SI 描述的是句法接口：一个类型公开提供哪些成员，而不约束这些成员的语义。

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

接口定义不需要宏、不需要继承特殊基类，也不需要外部代码生成器。

## 功能

- `si::satisfies<T, I>`：检查类型 `T` 是否满足接口 `I`。
- `si::satisfies<I> auto`：用于 abbreviated function template。
- `si::existential<I>`：拥有型、可拷贝的运行时接口对象。
- `si::existential_move_only<I>`：拥有型、仅移动的运行时接口对象。
- `si::existential_ref<I>`：非拥有引用型运行时接口对象。
- 支持成员函数的名称、参数、返回值、cv/ref 限定和 `noexcept` 精确匹配。
- 支持接口继承组合。
- private / protected 成员默认不匹配。

## 构建与测试

本项目使用 xmake 构建，测试框架为 boost.ut。

当前需要支持 C++26 静态反射的 GCC，并启用：

```text
-std=c++26 -freflection
```

构建测试：

```sh
xmake test -v
```

构建并运行示例：

```sh
xmake build -v structural_interface_example_basic
xmake run structural_interface_example_basic
```

## 当前限制

- runtime 同名 callable 成员已支持普通函数调用，但重载成员的 runtime proxy 仍需要进一步设计。
- 数据成员字段 proxy 尚未完成；当前重点是函数成员。

更完整的设计说明见 [design.md](design.md)。
