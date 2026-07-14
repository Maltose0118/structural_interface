#include <boost/ut.hpp>
#include <structural_interface.hpp>

#include <array>
#include <concepts>
#include <memory>
#include <memory_resource>
#include <utility>

struct Drawable {
    void draw() const;
};

struct MutableCounter {
    int value;
    int bump(int delta) & noexcept;
};

struct Shape : Drawable, MutableCounter {};

struct Circle {
    int value = 0;
    void draw() const {}
    int bump(int delta) & noexcept {
        value += delta;
        return value;
    }
};

struct MissingDraw {
    int value = 0;
    int bump(int) & noexcept {
        return 0;
    }
};

struct WrongNoexcept {
    int value = 0;
    void draw() const {}
    int bump(int) & {
        return 0;
    }
};

class PrivateDrawable {
    void draw() const {}
};

struct MoveOnlyDrawable {
    std::unique_ptr<int> value = std::make_unique<int>(42);
    void draw() const {}
};

struct LargeDrawable {
    std::array<std::byte, 64> payload{};
    void draw() const {}
};

template <class T, bool PropagateCopy = false, bool PropagateMove = false>
struct counting_allocator {
    using value_type = T;
    using propagate_on_container_copy_assignment = std::bool_constant<PropagateCopy>;
    using propagate_on_container_move_assignment = std::bool_constant<PropagateMove>;
    using is_always_equal = std::false_type;

    int* allocations = nullptr;
    int* deallocations = nullptr;
    int id = 0;

    counting_allocator() = default;

    counting_allocator(int* allocations, int* deallocations, int id)
        : allocations(allocations), deallocations(deallocations), id(id) {}

    template <class U>
    counting_allocator(const counting_allocator<U, PropagateCopy, PropagateMove>& other)
        : allocations(other.allocations), deallocations(other.deallocations), id(other.id) {}

    T* allocate(std::size_t count) {
        ++*allocations;
        return std::allocator<T>{}.allocate(count);
    }

    void deallocate(T* object, std::size_t count) noexcept {
        ++*deallocations;
        std::allocator<T>{}.deallocate(object, count);
    }

    counting_allocator select_on_container_copy_construction() const {
        auto selected = *this;
        ++selected.id;
        return selected;
    }

    template <class U>
    struct rebind {
        using other = counting_allocator<U, PropagateCopy, PropagateMove>;
    };

    template <class U, bool OtherCopy, bool OtherMove>
    friend struct counting_allocator;

    template <class U>
    friend bool operator==(const counting_allocator& left,
                           const counting_allocator<U, PropagateCopy, PropagateMove>& right) {
        return left.id == right.id;
    }
};

struct counting_resource : std::pmr::memory_resource {
    int allocations = 0;
    int deallocations = 0;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        ++allocations;
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }

    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        ++deallocations;
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

struct Consumable {
    int consume() && noexcept;
};

struct Token {
    int value = 11;
    int consume() && noexcept {
        return value;
    }
};

struct Calculator {
    int add(int left, int right) const noexcept;
    int mix(int base, int factor, int bias) &;
};

struct Arithmetic {
    int total = 0;

    int add(int left, int right) const noexcept {
        return left + right;
    }

    int mix(int base, int factor, int bias) & {
        total = base * factor + bias;
        return total;
    }
};

struct WrongArity {
    int add(int left) const noexcept {
        return left;
    }

    int mix(int, int, int) & {
        return 0;
    }
};

struct Overloaded {
    int call(int value) const;
    int call(int left, int right) const noexcept;
};

struct OverloadedImpl {
    int call(int value) const {
        return value + 1;
    }

    int call(int left, int right) const noexcept {
        return left + right;
    }
};

struct MissingOneOverload {
    int call(int value) const {
        return value + 1;
    }
};

template <class Reference, class Value>
concept can_bind_reference = requires(Value& value) {
    Reference{value};
};

template <class Reference>
concept has_bump = requires(Reference& value) {
    value.bump(1);
};

int main() {
    using namespace boost::ut;

    "structural satisfaction"_test = [] {
        expect(si::satisfies<Circle, Drawable>);
        expect(si::satisfies<Circle, MutableCounter>);
        expect(si::satisfies<Circle, Shape>);
        expect(!si::satisfies<MissingDraw, Drawable>);
        expect(!si::satisfies<MissingDraw, Shape>);
        expect(!si::satisfies<WrongNoexcept, MutableCounter>);
        expect(!si::satisfies<PrivateDrawable, Drawable>);
        expect(si::satisfies<Arithmetic, Calculator>);
        expect(!si::satisfies<WrongArity, Calculator>);
        expect(si::satisfies<OverloadedImpl, Overloaded>);
        expect(!si::satisfies<MissingOneOverload, Overloaded>);
    };

    "abbreviated satisfies constraints"_test = [] {
        auto render = [](si::satisfies<Drawable> auto&& obj) {
            obj.draw();
        };

        render(Circle{});
    };

    "owning existential stores and moves objects"_test = [] {
        si::existential<Drawable> drawable = Circle{};
        drawable.draw();

        auto copy = drawable;
        copy.draw();

        auto moved = std::move(drawable);
        moved.draw();
    };

    "runtime type checks and gets owning existentials"_test = [] {
        si::existential<Drawable> drawable = Circle{};
        static_assert(std::same_as<decltype(si::get<Circle>(drawable)), Circle&>);
        static_assert(std::same_as<
                      decltype(si::get<Circle>(std::as_const(drawable))),
                      const Circle&>);

        expect(si::is<Circle>(drawable));
        expect(!si::is<Arithmetic>(drawable));
        expect(si::get<Circle>(drawable).value == 0_i);

        bool threw = false;
        try {
            (void)si::get<Arithmetic>(drawable);
        } catch (const std::bad_cast&) {
            threw = true;
        }
        expect(threw);
    };

    "default allocator keeps the existing layout"_test = [] {
        using default_existential = si::existential<Drawable>;
        using explicit_default_existential =
            si::existential<Drawable, std::allocator<std::byte>>;
        static_assert(std::same_as<default_existential, explicit_default_existential>);
        static_assert(sizeof(default_existential) == sizeof(explicit_default_existential));

        default_existential value = Circle{};
        expect(si::is<Circle>(value));
    };

    "custom allocator handles heap storage and copy selection"_test = [] {
        int allocations = 0;
        int deallocations = 0;
        using allocator = counting_allocator<std::byte>;
        allocator source_allocator{&allocations, &deallocations, 7};

        using value_type = si::existential<Drawable, allocator>;
        value_type value{source_allocator, LargeDrawable{}};
        expect(allocations == 1_i);
        expect(value.get_allocator().id == 7_i);

        auto copy = value;
        expect(allocations == 2_i);
        expect(copy.get_allocator().id == 8_i);

        copy = value;
        expect(deallocations == 1_i);
        expect(copy.get_allocator().id == 8_i);

        value_type equal_source{
            allocator{&allocations, &deallocations, 9}, LargeDrawable{}};
        value_type equal_target{
            allocator{&allocations, &deallocations, 9}, LargeDrawable{}};
        int allocations_before_equal_move = allocations;
        equal_target = std::move(equal_source);
        expect(allocations == allocations_before_equal_move);
    };

    "pmr allocator receives heap allocation requests"_test = [] {
        counting_resource resource;
        using allocator = std::pmr::polymorphic_allocator<std::byte>;
        using value_type = si::existential<Drawable, allocator>;

        value_type value{allocator{&resource}, LargeDrawable{}};
        expect(resource.allocations == 1_i);
        expect(value.get_allocator().resource() == &resource);
        expect(si::is<LargeDrawable>(value));
    };

    "allocator propagation follows allocator traits"_test = [] {
        int left_allocations = 0;
        int left_deallocations = 0;
        int right_allocations = 0;
        int right_deallocations = 0;

        using nonpropagating = counting_allocator<std::byte>;
        using copy_propagating = counting_allocator<std::byte, true, false>;
        using propagating = counting_allocator<std::byte, false, true>;
        using value_type = si::existential<Drawable, nonpropagating>;
        using propagating_value_type = si::existential<Drawable, propagating>;

        value_type source{
            nonpropagating{&left_allocations, &left_deallocations, 1}, LargeDrawable{}};
        value_type target{
            nonpropagating{&right_allocations, &right_deallocations, 2}, LargeDrawable{}};
        target = std::move(source);
        expect(target.get_allocator().id == 2_i);
        expect(right_allocations == 2_i);

        using copy_propagating_value_type =
            si::existential<Drawable, copy_propagating>;
        copy_propagating_value_type copy_source{
            copy_propagating{&left_allocations, &left_deallocations, 5}, LargeDrawable{}};
        copy_propagating_value_type copy_target{
            copy_propagating{&right_allocations, &right_deallocations, 6}, LargeDrawable{}};
        copy_target = copy_source;
        expect(copy_target.get_allocator().id == 5_i);

        propagating_value_type propagated_source{
            propagating{&left_allocations, &left_deallocations, 3}, LargeDrawable{}};
        propagating_value_type propagated_target{
            propagating{&right_allocations, &right_deallocations, 4}, LargeDrawable{}};
        propagated_target = std::move(propagated_source);
        expect(propagated_target.get_allocator().id == 3_i);
    };

    "owning existential exposes reflected call members"_test = [] {
        si::existential<MutableCounter> counter = Circle{};
        static_assert(std::same_as<decltype(counter.value), int*>);
        expect(*counter.value == 0_i);
        *counter.value = 10;
        expect(*counter.value == 10_i);
        expect(counter.bump(3) == 13_i);
        expect(*counter.value == 13_i);
        expect(counter.bump(4) == 17_i);

        auto copied = counter;
        *copied.value = 100;
        expect(*copied.value == 100_i);
        expect(*counter.value == 17_i);

        auto moved = std::move(counter);
        expect(*moved.value == 17_i);
        *moved.value = 23;
        expect(moved.bump(1) == 24_i);
    };

    "multi-argument calls keep argument and return types"_test = [] {
        si::existential<Calculator> calculator = Arithmetic{};
        expect(calculator.add(2, 5) == 7_i);
        expect(calculator.mix(3, 4, 5) == 17_i);

        auto copied = calculator;
        expect(copied.add(8, 13) == 21_i);
    };

    "overloaded interfaces are checked independently"_test = [] {
        static_assert(si::satisfies<OverloadedImpl, Overloaded>);
        static_assert(!si::satisfies<MissingOneOverload, Overloaded>);
    };

    "move only existential accepts move only concrete types"_test = [] {
        static_assert(!std::copy_constructible<MoveOnlyDrawable>);
        si::existential_move_only<Drawable> drawable = MoveOnlyDrawable{};
        expect(si::is<MoveOnlyDrawable>(drawable));
        drawable.draw();

        auto moved = std::move(drawable);
        expect(!si::is<MoveOnlyDrawable>(drawable));
        expect(*si::get<MoveOnlyDrawable>(moved).value == 42_i);
        moved.draw();
    };

    "move only existential supports custom allocators"_test = [] {
        int allocations = 0;
        int deallocations = 0;
        using allocator = counting_allocator<std::byte>;
        using value_type = si::existential_move_only<Drawable, allocator>;

        value_type value{allocator{&allocations, &deallocations, 1}, LargeDrawable{}};
        auto moved = std::move(value);
        expect(si::is<LargeDrawable>(moved));
        expect(allocations == 1_i);
    };

    "rvalue qualified reflected calls dispatch to moved concrete object"_test = [] {
        si::existential_move_only<Consumable> token = Token{};
        expect(std::move(token).consume() == 11_i);
    };

    "reference existential observes an object"_test = [] {
        Circle circle{};
        si::existential_ref<MutableCounter> ref = circle;
        static_assert(std::same_as<decltype(si::get<Circle>(ref)), Circle&>);
        static_assert(std::same_as<
                      decltype(si::get<Circle>(std::as_const(ref))),
                      const Circle&>);
        expect(si::is<Circle>(ref));
        *ref.value = 41;
        expect(circle.value == 41_i);
        expect(ref.bump(1) == 42_i);
        expect(*ref.value == 42_i);
        si::existential_ref<Drawable> drawable_ref = circle;
        drawable_ref.draw();
    };

    "const reference existential preserves read only access"_test = [] {
        const Circle const_circle{};
        Circle mutable_circle{};

        si::existential_ref<const MutableCounter> const_ref = const_circle;
        si::existential_ref<const Drawable> const_drawable_ref = mutable_circle;

        static_assert(std::same_as<decltype(const_ref.value), const int*>);
        static_assert(!has_bump<si::existential_ref<const MutableCounter>>);
        static_assert(std::same_as<decltype(si::get<Circle>(const_ref)), const Circle&>);
        static_assert(std::same_as<
                      decltype(si::get<Circle>(const_drawable_ref)),
                      const Circle&>);

        expect(si::is<Circle>(const_ref));
        expect(si::is<Circle>(const_drawable_ref));
        expect(si::get<Circle>(const_ref).value == 0_i);
    };

    "reference existential rejects const objects for mutable views"_test = [] {
        static_assert(can_bind_reference<si::existential_ref<Drawable>, Circle>);
        static_assert(!can_bind_reference<si::existential_ref<Drawable>, const Circle>);
        static_assert(can_bind_reference<si::existential_ref<const Drawable>, Circle>);
        static_assert(can_bind_reference<si::existential_ref<const Drawable>, const Circle>);
    };
}
