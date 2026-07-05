# Structural Interface (SI)

## Design Specification

---

# 1. Overview

Structural Interface（简称 **SI**）是一种基于 **结构化成员匹配（Structural Member Matching）** 和 **C++26 静态反射** 构建的统一接口机制。

一个接口定义同时派生出两种视图：

* **Compile-time View**

  * `si::satisfies<T, I>`
  * 用于泛型约束
  * 零运行时开销

* **Runtime View**

  * `si::existential<I>`
  * `si::existential_move_only<I>`
  * `si::existential_ref<I>`
  * 类型擦除动态多态

接口只定义一次，其余表示均由编译器基于反射自动生成。

整个系统不依赖宏，不依赖外部代码生成器，仅依赖 C++26 静态反射。

---

# 2. Design Philosophy

SI 描述的是 **句法接口（Syntactic Interface）**，而不是语义接口（Semantic Interface）。

接口只规定：

> 一个类型必须公开提供哪些成员。

而不规定：

> 这些成员应当具有怎样的行为或语义。

例如：

```cpp
struct Drawable {
    void draw() const;
};
```

表示一个类型必须拥有：

```cpp
void draw() const;
```

至于：

* `draw` 是否真正执行绘图；
* 是否修改对象状态；
* 是否线程安全；
* 是否满足其他语义约束；

均不属于 SI 的职责。

这一设计遵循 C++ Concepts 的术语：

* **satisfies**：满足句法要求；
* **models**：同时满足句法与语义要求。

SI 仅判断 **satisfies**。

---

# 3. Interface Definition

接口使用普通 `struct` 定义。

```cpp
struct Drawable {
    void draw() const;
};

struct Geom {
    int width;
    int height;
};
```

接口无需继承任何基类。

无需任何宏。

未来可支持：

```cpp
[[si::interface]]
```

作为可选标记。

---

# 4. Allowed Members

接口允许包含：

* 非静态成员函数声明（无函数体）；
* 非静态数据成员；
* 嵌套接口；
* 接口组合（继承其他接口）。

接口禁止包含：

* 构造函数；
* 析构函数；
* 静态成员；
* friend 声明；
* using declaration；
* 类型别名；
* 成员函数实现；
* 虚基类。

接口成员必须是 public。

具体类型中的匹配成员也必须在当前上下文可访问。默认情况下，private / protected 成员不参与匹配，也不能被 runtime thunk 绕过访问控制。

---

# 5. Structural Satisfaction

系统自动生成：

```cpp
namespace si {

template<class T, class I>
concept satisfies = /* compiler generated */;

}
```

当且仅当接口中的每一个成员都能够在类型 `T` 中找到对应成员时：

```cpp
si::satisfies<T, I>
```

成立。

---

## 5.1 Function Matching

成员函数按照以下信息进行匹配：

* 名称；
* 参数类型；
* 返回类型；
* cv 限定；
* 引用限定（`&` / `&&`）；
* `noexcept`。

上述各项必须完全一致。

默认参数不参与匹配。

若接口存在多个重载：

```cpp
struct Drawable {
    void draw();
    void draw(int);
};
```

则每一个重载均独立匹配。

---

## 5.2 Data Member Matching

数据成员要求：

* 同名；
* 同类型。

例如：

```cpp
struct Geom {
    int width;
};
```

可由：

```cpp
struct Window {
    int width;
};
```

满足。

数据成员仅作为结构要求参与匹配。

其语义不属于 SI 的职责。

---

## 5.3 Function Templates

函数模板仅当能够实例化出完全匹配的成员函数时参与匹配。

例如：

```cpp
template<class T>
void draw(T);
```

可以满足：

```cpp
void draw(int);
```

但不能满足：

```cpp
void draw();
```

---

# 6. Interface Composition

接口可通过继承其他接口组合：

```cpp
struct Shape
    : Drawable,
      Transformable
{
};
```

表示：

```text
Shape = Drawable ∩ Transformable
```

即必须同时满足所有父接口。

组合接口仍然是普通接口定义。

不会产生任何运行时继承关系。

---

## 6.1 Conflict Detection

若多个父接口存在：

* 同名但不同类型的数据成员；
* 同名但不同函数类型的成员函数；

则组合非法。

编译器产生诊断。

---

# 7. Runtime Existential Types

对于每一个接口 `I`，系统自动生成：

```cpp
si::existential<I>

si::existential_move_only<I>

si::existential_ref<I>
```

三者仅所有权语义不同。

| 类型                      | 所有权   | Copy            | Move |
| ------------------------- | -------- | --------------- | ---- |
| `existential`             | 拥有对象 | 调用对象拷贝构造 | ✔    |
| `existential_move_only`   | 拥有对象 | ✘               | ✔    |
| `existential_ref`         | 非拥有引用 | 复制引用       | ✔    |

---

# 8. Runtime Representation

SI 使用统一的运行时接口元数据（Interface Metadata）。

对于每一个满足：

```cpp
si::satisfies<T, I>
```

的具体类型 `T`，系统自动生成唯一的静态元数据：

```cpp
template<class I>
struct interface_metadata {

    // 生命周期管理

    void (*destroy)(void* object);

    void (*copy_construct)(
        void* dst_storage,
        void** dst_object,
        const void* src_object
    );

    void (*move_construct)(
        void* dst_storage,
        void** dst_object,
        void* src_object
    );

    // 接口成员函数 thunk
    // ...

    // 字段偏移
    // ...
};
```

每个 `(Interface, Concrete Type)` 对应唯一一份静态 `constexpr` 元数据。

整个程序生命周期仅存在一份。

`interface_metadata` 不是单纯的函数表。它描述某个具体类型如何实现某个接口的完整运行时信息，包括生命周期管理、方法分派、字段访问以及 SBO / heap 对象管理策略。

---

# 9. Object Representation

## 9.1 Owning Existential

拥有型 existential 使用 Small Buffer Optimization（SBO）：

```cpp
template<class I>
class existential {
    /* generated interface member proxies */

    _si_existential_details<I> _si_details_;
};

template<class I>
struct _si_existential_details {
    alignas(ImplementationDefinedAlignment)
    std::byte storage[ImplementationDefinedStorage];

    void* object;

    const interface_metadata<I>* metadata;
};
```

运行时布局为：

```text
+----------------------------------+
| inline storage (SBO)             |
| object pointer                   |
| interface_metadata pointer       |
+----------------------------------+
```

小对象：

* `object == storage`
* 对象 placement new 到 `storage`

大对象：

* `object = new T(...)`
* `storage` 未使用

因此方法 thunk 永远只需要：

```cpp
static_cast<T*>(object)
```

不需要判断对象是否使用 SBO。

## 9.2 Reference Existential

引用型 existential 保存对象地址：

```cpp
template<class I>
class existential_ref {
    /* generated interface member proxies */

    _si_ref_details<I> _si_details_;
};

template<class I>
struct _si_ref_details {

    void* object;

    const interface_metadata<I>* metadata;
};
```

不拥有对象生命周期。

`existential<I>` 和 `existential_move_only<I>` 是值语义对象，不提供 public 的空状态 API。除特殊成员函数与接口生成成员之外，它们不应新增普通 public 成员函数，例如 `has_value()`、`reset()`、`raw_object()` 或 `metadata()`。这些实现细节均位于 `_si_details_` 中，并仅供库实现使用。

---

# 10. Construction

例如：

```cpp
Circle c;

si::existential<Drawable> e = c;
```

小对象等价于：

```cpp
new(storage) Circle(c);

object = storage;
metadata = &metadata_for<Drawable, Circle>;
```

大对象等价于：

```cpp
object = new Circle(c);

metadata = &metadata_for<Drawable, Circle>;
```

因此：

* 左值调用对象拷贝构造函数；
* 右值调用对象移动构造函数；
* 小对象使用本地存储；
* 大对象使用堆分配。

---

# 11. Copy / Move

复制：

```cpp
existential a = Circle{};
existential b = a;
```

执行：

```cpp
a.metadata->copy_construct(
    b.storage,
    &b.object,
    a.object
);

b.metadata = a.metadata;
```

移动：

```cpp
existential b = std::move(a);
```

执行：

```cpp
a.metadata->move_construct(
    b.storage,
    &b.object,
    a.object
);

b.metadata = a.metadata;
```

最终调用底层对象的拷贝或移动构造函数。

小对象版本可生成：

```cpp
template<class T>
void copy_construct_small(
    void* dst_storage,
    void** dst_object,
    const void* src_object
) {
    new(dst_storage) T(*static_cast<const T*>(src_object));
    *dst_object = dst_storage;
}
```

大对象版本可生成：

```cpp
template<class T>
void copy_construct_large(
    void*,
    void** dst_object,
    const void* src_object
) {
    *dst_object = new T(*static_cast<const T*>(src_object));
}
```

`existential` 自己不判断对象大小。

小对象与大对象的差异隐藏在 `interface_metadata` 指向的生命周期函数中。

因此：

`existential<I>` 要求底层对象满足：

```cpp
std::copy_constructible
```

`existential_move_only<I>` 要求底层对象满足：

```cpp
std::move_constructible
```

---

# 12. Destruction

析构：

```cpp
~existential()
```

执行：

```cpp
metadata->destroy(object);
```

小对象的 destroy thunk 调用析构函数。

大对象的 destroy thunk 调用析构函数并释放堆内存。

`existential` 自己不需要知道对象是否位于堆上。

---

# 13. Method Dispatch

接口成员函数的值语义调用由两层共同完成：

* `existential<I>` / `existential_ref<I>` 注入与接口成员同名的成员变量；
* 这些成员变量的类型是编译器生成的 callable proxy，并通过 `interface_metadata` 中的 thunk 调用真实对象。

也就是说，对于：

```cpp
struct Drawable {
    void draw() const;
};
```

系统会为 `interface_metadata<Drawable>` 生成对应槽位：

```cpp
template<>
struct interface_metadata<Drawable> {
    void (*destroy)(void* object);
    void (*copy_construct)(void* dst_storage, void** dst_object, const void* src_object);
    void (*move_construct)(void* dst_storage, void** dst_object, void* src_object);

    void (*draw)(const void* object);
};
```

并为每个满足 `Drawable` 的具体类型生成 thunk：

```cpp
template<class T>
void draw_thunk(const void* object) {
    static_cast<const T*>(object)->draw();
}
```

系统会为 `si::existential<Drawable>` 注入等价于下面的成员变量：

```cpp
si_member_function_proxy<
    existential<Drawable>,
    &interface_metadata<Drawable>::draw
> draw;
```

因此用户代码可以直接写：

```cpp
si::existential<Drawable> obj = Circle{};

obj.draw();
```

这里的 `obj.draw()` 不是成员函数调用，而是成员对象调用：

```cpp
obj.draw.operator()();
```

也就是说，C++26 静态反射只需要注入成员变量；成员变量的类型提供 `operator()`，从而保留 `obj.draw()` 的值语义写法。

接口成员函数通过 `interface_metadata` 中的 thunk 调用：

```text
callable member variable
        │
        ▼
owner existential object
        │
        ▼
interface_metadata
        │
        ▼
generated thunk
        │
        ▼
T::member(...)
```

每次调用仅发生一次函数指针间接。

性能与传统虚函数调用相当。

---

## 13.1 Injected Callable Member

为了让 `obj.draw()` 成立，系统为每个接口成员函数生成一个 callable 成员变量类型。

概念上：

```cpp
template<class Owner, auto Slot>
struct si_member_function_proxy {
    void** object;
    /* thunk slot */

    decltype(auto) operator()() const {
        return /* thunk */(*object);
    }
};
```

然后编译器把该 proxy 作为成员变量注入到 existential family：

```cpp
template<class I>
class existential {
    _si_existential_details<I> _si_details_;

public:
    si_member_function_proxy<
        existential<I>,
        /* slot for I::draw */
    > draw;
};
```

proxy 由库实现绑定到 `_si_details_` 中的 object 指针槽和对应 thunk。

实现可以采用以下任一策略：

* proxy 成员保存一个指向 `_si_details_` object 槽位的指针；
* proxy 成员保存一个指向 owner 的指针；
* proxy 成员通过编译器已知的成员偏移从自身地址恢复 owner；
* 编译器把 proxy 视作特殊注入成员，允许它访问 enclosing object。

第一种策略最普通，但每个接口成员会增加一个指针大小的存储。

后两种策略可以做到零额外存储，并可使用 `[[no_unique_address]]`，但依赖实现对注入成员的支持。

在 C++26 静态反射模型中，生成器可以枚举接口 `I` 的成员声明，并把对应 callable 成员变量注入到 `existential<I>`、`existential_move_only<I>` 和 `existential_ref<I>` 中。反射负责“读接口形状”，成员变量注入负责“让 wrapper 拥有同名调用入口”。

关键要求是：

* 对接口中的每个成员函数生成同名 callable 成员变量；
* 保留参数类型、返回类型、cv/ref 限定和 `noexcept`；
* 支持重载时生成可重载的 callable proxy；
* `const` 成员通过 `const void*` thunk 分派；
* `&` 成员通过左值对象分派；
* `&&` 成员通过 `std::move(*static_cast<T*>(object))` 分派；
* `existential` 的 public API 看起来像接口 `I` 的值语义对象。

例如：

```cpp
struct Consumable {
    void consume() &&;
};
```

生成的 `existential<Consumable>` 支持：

```cpp
si::existential<Consumable> value = Token{};

std::move(value).consume();
```

对应 thunk 内部对真实对象执行：

```cpp
std::move(*static_cast<T*>(object)).consume();
```

这使得 SI 的 runtime view 不只是“可被显式 dispatch 的盒子”，而是一个拥有接口同名成员入口的值语义对象。

---

# 14. Field Access

接口数据成员同样通过成员变量注入实现，但 runtime wrapper 暴露的是原始字段指针。

```cpp
obj.width
```

当前库实现中，数据成员注入到 `existential` 本体的生成基类中。`obj.width` 的类型是字段的原始指针类型，例如 `int*` 或 `std::string*`。

因此用户显式解引用访问字段：

```cpp
*(obj.width) = 10;
int value = *(obj.width);
```

字段指针在 wrapper 构造时绑定到具体类型 `T` 的同名成员地址。copy / move 之后会重新绑定到新 wrapper 持有的对象字段。成员函数也通过同一个点访问视图暴露，因此函数调用写作 `obj.draw()`。

访问过程：

```text
obj.width
        │
        ▼
T::field
```

等价于：

```cpp
constexpr auto member = /* reflected T::width */;
obj.width = std::addressof(static_cast<T*>(object)->*member);
```

这种设计保持表达式语义明确：字段名对应字段指针，库不额外模拟引用成员。

---

# 15. Storage Requirements

Owning existential 使用实现定义的 SBO 本地存储。

实现定义：

* `ImplementationDefinedStorage`
* `ImplementationDefinedAlignment`

若满足：

```cpp
sizeof(T) <= ImplementationDefinedStorage

alignof(T) <= ImplementationDefinedAlignment
```

则对象存入本地 SBO storage。

否则对象存入堆上。

SI 自动选择小对象或大对象对应的生命周期函数。

---

# 16. Reflection-Based Generation

系统完全依赖 C++26 静态反射。

编译器自动生成：

* `si::satisfies`
* `interface_metadata`
* callable 成员变量 proxy
* field 指针成员变量
* 生命周期管理函数
* 成员函数 thunk
* 字段偏移
* `existential`
* `existential_move_only`
* `existential_ref`
* 诊断信息

无需宏。

无需外部代码生成器。

---

# 17. Diagnostics

当结构匹配失败时：

```text
error:
'Circle' does not satisfy
'si::satisfies<Drawable>'

missing member:

void draw() const
```

接口组合冲突：

```text
error:

interface 'Shape'

contains conflicting member:

width
```

诊断应直接指出无法满足的成员。

---

# 18. Performance Guarantees

| 操作 | 开销 |
| ---- | ---- |
| `si::satisfies` | 编译期 |
| 静态调用 | 零运行时开销 |
| existential 方法调用 | 一次函数指针间接 |
| existential 字段访问 | 一次固定偏移 |
| existential 小对象构造 | 调用对象构造函数 |
| existential 大对象构造 | 一次堆分配 + 调用对象构造函数 |
| existential 拷贝 | 调用对象拷贝构造函数 |
| existential 移动 | 调用对象移动构造函数 |
| existential 析构 | 调用对象析构函数，必要时释放堆内存 |

小对象路径不发生堆分配。

大对象路径采用与 `std::function`、`std::any` 等现代 C++ 类型擦除设施一致的 SBO 策略。

---

# 19. Non-goals

SI 不试图：

* 描述接口语义；
* 验证 models；
* 替代类继承体系；
* 实现虚继承；
* 依赖 RTTI；
* 提供完整反射框架；
* 自动推导接口。

---

# 20. Example

```cpp
#include <structural_interface.hpp>

#include <vector>

struct Drawable {
    void draw() const;
};

struct Circle {
    void draw() const {
        // ...
    }
};

struct Rectangle {
    void draw() const {
        // ...
    }
};

void render(si::satisfies<Drawable> auto&& obj) {
    obj.draw();
}

int main() {

    render(Circle{});

    std::vector<si::existential<Drawable>> objects;

    objects.emplace_back(Circle{});
    objects.emplace_back(Rectangle{});

    for (auto& object : objects)
        render(object);
}
```

在上述示例中：

* `render(Circle{})` 使用静态分发；
* `render(object)` 使用 `existential` 动态分发；
* 两者共享同一接口定义，无需重复声明 Concept、抽象基类或类型擦除包装器。

---

# 21. Architecture Summary

SI 的整体架构为：

```text
                Interface Definition
                        │
            Reflection generates
                        │
        ┌───────────────┴───────────────┐
        │                               │
   satisfies<T,I>              Runtime View
        │                               │
 Static Dispatch        ┌──────────────┴──────────────┐
                        │                             │
              interface_metadata         injected member proxies
                        │                             │
                        └──────────────┬──────────────┘
                                       │
                              existential family
```

运行时对象统一为：

```text
+----------------------------------+
| inline storage (SBO)             |
| object pointer                   |
| interface_metadata pointer       |
+----------------------------------+
```

其中：

* 小对象：`object` 指向 `storage`
* 大对象：`object` 指向堆对象

`interface_metadata` 是每个 `(Interface, ConcreteType)` 唯一的一份静态 `constexpr` 元数据，负责生命周期管理、方法分派和字段访问。

这套模型兼顾性能、泛用性和实现复杂度，并与现代 C++ 类型擦除设计保持一致。
