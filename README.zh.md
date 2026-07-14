# Structural Interface

Structural Interface（SI）是一个基于 C++26 静态反射的 header-only 结构化接口实验库。它让普通 `struct` 同时作为泛型约束接口和运行时类型擦除接口使用。

## 基本用法

### 第一步：非侵入式动态分发

用普通 `struct` 定义接口。任何满足接口的类型都能擦除为 `existential`，在同一个容器里动态分发：

```cpp
// 定义接口：一个包含所需成员的 struct
struct Drawable {
    void draw() const;
};

// 满足 Drawable 接口的具体类型
struct Circle {
    void draw() const { std::println("Circle"); }
};
struct Rectangle {
    void draw() const { std::println("Rectangle"); }
};

static_assert(si::satisfies<Circle, Drawable>);
static_assert(si::satisfies<Rectangle, Drawable>);

int main() {
    // 类型擦除容器：任何满足 Drawable 接口的类型都可放入
    std::vector<si::existential<Drawable>> shapes{ Circle{}, Rectangle{} };

    for (const auto& shape : shapes) {
        shape.draw(); // 动态分发
    }
}
```

不需要宏、不需要继承基类、不需要代码生成器——只需要 C++26。

### 第二步：同一算法，静态分发和动态分发同时支持

只要写一个入参满足 `si::satisfies<Drawable>` 约束的泛型函数，它就能同时接受具体类型（静态分发）和 `existential`（动态分发）：

```cpp
// 同一算法函数，同时用于静态分发和动态分发
void render(const si::satisfies<Drawable> auto& obj) {
    obj.draw();
}

int main() {
    Circle circle{};
    render(circle); // 静态分发，零运行时开销

    si::existential<Drawable> obj = Rectangle{};
    render(obj);    // 动态分发，同一函数
}
```

一个接口、一个算法、两种分发模式。

### 第三步：接口组合

接口可以继承多个父接口，组合后的接口要求同时满足所有父接口（`Widget = Drawable ∩ Scalable`）：

```cpp
struct Drawable {
    void draw() const;
};

struct Scalable {
    void scale(float factor);
};

// 组合接口 = Drawable + Scalable
struct Widget : Drawable, Scalable {};

// 同时满足 Drawable 和 Scalable 的具体类型
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

### 第四步：接口还能声明数据成员

接口里的字段在运行时以原始字段指针暴露，用 `*(obj.field)` 访问：

```cpp
struct Drawable {
    std::string name; // 数据成员
    void draw() const; // 函数成员
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
        // shape.name 的类型是 std::string*，不是 std::string
        std::println("{}:", *(shape.name));
        shape.draw();
    }
}
```

### 第五步：运行时类型检查

运行时包装器提供自由函数 `si::is<T>` 和 `si::get<T>`，用于精确检查具体类型。`get<T>` 返回引用，不转移所有权；类型不匹配时抛出 `std::bad_cast`。

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

`si::existential_ref<const I>` 提供只读视图，`get<T>` 返回 `const T&`。

```cpp
const Circle circle{ "read-only" };
si::existential_ref<const Drawable> reference = circle;
const Circle& value = si::get<Circle>(reference);
```

完整示例在 [examples/basic.cpp](examples/basic.cpp)。

## 目标

SI 描述的是句法接口：一个类型公开提供哪些成员，而不约束这些成员的语义。

```cpp
struct Drawable {
    void draw() const;
};

struct Circle {
    void draw() const { std::println("Circle"); }
};

static_assert(si::satisfies<Circle, Drawable>);
```

接口定义不需要宏、不需要继承特殊基类，也不需要外部代码生成器。

## 功能

- `si::satisfies<T, I>`：检查类型 `T` 是否满足接口 `I`。
- `si::satisfies<I> auto`：用于 abbreviated function template。
- `si::existential<I>`：拥有型、可拷贝的运行时接口对象。
- `si::existential_move_only<I>`：拥有型、仅移动的运行时接口对象。
- `si::existential_ref<I>`：非拥有引用型运行时接口对象。
- 支持成员函数的名称、参数、返回值、cv/ref 限定和 `noexcept` 精确匹配。
- 运行时接口对象通过点运算符暴露接口成员：函数可写 `obj.draw()`，数据成员是原始字段指针，可写 `*(obj.field)`。
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
- 数据成员当前以原始字段指针形式暴露，例如 `obj.name` 的类型为 `std::string*`。

更完整的设计说明见 [doc/design.zh.md](doc/design.zh.md)。
