#include <boost/ut.hpp>
#include <structural_interface.hpp>

#include <concepts>
#include <memory>
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
