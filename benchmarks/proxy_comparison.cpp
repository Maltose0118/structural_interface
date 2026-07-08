#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <proxy/proxy.h>
#include <structural_interface.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t concrete_type_count = 100;
constexpr std::size_t default_object_count = 100'000;

std::size_t parse_object_count(int argc, char** argv) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::string_view{argv[index]} == "--objects") {
            return static_cast<std::size_t>(std::strtoull(argv[index + 1], nullptr, 10));
        }
    }
    return default_object_count;
}

template <std::size_t Bytes>
struct payload_for {
    std::array<std::byte, Bytes> bytes{};
};

template <>
struct payload_for<4> {
    std::uint32_t value = 0;
};

struct StepInterface {
    std::uint64_t step(std::uint64_t input) noexcept;
};

struct VirtualStep {
    virtual ~VirtualStep() = default;
    virtual std::uint64_t step(std::uint64_t input) noexcept = 0;
};

template <int Id, std::size_t PayloadBytes>
struct Model {
    payload_for<PayloadBytes> payload{};
    std::uint64_t state = static_cast<std::uint64_t>(Id + 1);

    std::uint64_t step(std::uint64_t input) noexcept {
        state = state * 1'103'515'245u + input + static_cast<std::uint64_t>(Id + 12'345);
        return state ^ (state >> ((Id % 13) + 1));
    }
};

template <int Id, std::size_t PayloadBytes>
struct VirtualModel final : VirtualStep {
    Model<Id, PayloadBytes> model{};

    std::uint64_t step(std::uint64_t input) noexcept override {
        return model.step(input);
    }
};

PRO_DEF_MEM_DISPATCH(MemStep, step);

struct ProxyStep : pro::facade_builder
    ::add_convention<MemStep, std::uint64_t(std::uint64_t) noexcept>
    ::support_copy<pro::constraint_level::nontrivial>
    ::build {};

template <class Fn, std::size_t... Indices>
decltype(auto) with_type_index(std::size_t index, Fn&& fn, std::index_sequence<Indices...>) {
    using result_t = decltype(fn.template operator()<0>());
    using callback_t = result_t (*)(Fn&);
    static constexpr callback_t callbacks[] = {
        +[](Fn& callback) -> result_t {
            return callback.template operator()<static_cast<int>(Indices)>();
        }...
    };
    return callbacks[index % sizeof...(Indices)](fn);
}

template <class Fn>
decltype(auto) with_type_index(std::size_t index, Fn&& fn) {
    return with_type_index(index, std::forward<Fn>(fn),
                           std::make_index_sequence<concrete_type_count>{});
}

template <std::size_t PayloadBytes>
std::vector<std::unique_ptr<VirtualStep>> make_virtual_objects(std::size_t object_count) {
    std::vector<std::unique_ptr<VirtualStep>> objects;
    objects.reserve(object_count);
    for (std::size_t index = 0; index != object_count; ++index) {
        with_type_index(index, [&]<int Id> {
            objects.emplace_back(std::make_unique<VirtualModel<Id, PayloadBytes>>());
        });
    }
    return objects;
}

template <std::size_t PayloadBytes>
std::vector<si::existential_move_only<StepInterface>> make_si_objects(std::size_t object_count) {
    std::vector<si::existential_move_only<StepInterface>> objects;
    objects.reserve(object_count);
    for (std::size_t index = 0; index != object_count; ++index) {
        with_type_index(index, [&]<int Id> {
            objects.emplace_back(Model<Id, PayloadBytes>{});
        });
    }
    return objects;
}

template <std::size_t PayloadBytes>
std::vector<pro::proxy<ProxyStep>> make_proxy_objects(std::size_t object_count) {
    std::vector<pro::proxy<ProxyStep>> objects;
    objects.reserve(object_count);
    for (std::size_t index = 0; index != object_count; ++index) {
        with_type_index(index, [&]<int Id> {
            objects.emplace_back(pro::make_proxy<ProxyStep, Model<Id, PayloadBytes>>());
        });
    }
    return objects;
}

template <class Objects, class Call>
void run_invocation_bench(ankerl::nanobench::Bench& bench,
                          std::string_view name,
                          Objects& objects,
                          Call&& call) {
    bench.batch(objects.size()).run(std::string{name}, [&] {
        std::uint64_t checksum = 0;
        std::uint64_t input = 0;
        for (auto& object : objects) {
            checksum ^= call(object, ++input);
        }
        ankerl::nanobench::doNotOptimizeAway(checksum);
    });
}

template <std::size_t PayloadBytes>
void run_invocation_group(ankerl::nanobench::Bench& bench,
                          std::string_view label,
                          std::size_t object_count) {
    auto virtual_objects = make_virtual_objects<PayloadBytes>(object_count);
    run_invocation_bench(bench, std::string{"virtual/invoke/"} + std::string{label},
                         virtual_objects,
                         [](auto& object, std::uint64_t input) noexcept {
                             return object->step(input);
                         });

    auto si_objects = make_si_objects<PayloadBytes>(object_count);
    run_invocation_bench(bench, std::string{"si/invoke/"} + std::string{label},
                         si_objects,
                         [](auto& object, std::uint64_t input) noexcept {
                             return object.step(input);
                         });

    auto proxy_objects = make_proxy_objects<PayloadBytes>(object_count);
    run_invocation_bench(bench, std::string{"proxy/invoke/"} + std::string{label},
                         proxy_objects,
                         [](auto& object, std::uint64_t input) noexcept {
                             return object->step(input);
                         });
}

template <class MakeObjects>
void run_lifetime_bench(ankerl::nanobench::Bench& bench,
                        std::string_view name,
                        std::size_t object_count,
                        MakeObjects&& make_objects) {
    bench.batch(object_count).run(std::string{name}, [&] {
        auto objects = make_objects(object_count);
        ankerl::nanobench::doNotOptimizeAway(objects.data());
    });
}

template <std::size_t PayloadBytes>
void run_lifetime_group(ankerl::nanobench::Bench& bench,
                        std::string_view label,
                        std::size_t object_count) {
    run_lifetime_bench(bench, std::string{"virtual/lifetime/"} + std::string{label},
                       object_count, make_virtual_objects<PayloadBytes>);
    run_lifetime_bench(bench, std::string{"si/lifetime/"} + std::string{label},
                       object_count, make_si_objects<PayloadBytes>);
    run_lifetime_bench(bench, std::string{"proxy/lifetime/"} + std::string{label},
                       object_count, make_proxy_objects<PayloadBytes>);
}

} // namespace

int main(int argc, char** argv) {
    auto object_count = parse_object_count(argc, argv);

    ankerl::nanobench::Bench bench;
    bench.title("Structural Interface vs virtual functions vs Microsoft Proxy")
        .unit("dispatch or object")
        .warmup(3)
        .minEpochIterations(20)
        .relative(true);

    run_invocation_group<4>(bench, "small-4B", object_count);
    run_invocation_group<96>(bench, "large-96B", object_count);
    run_lifetime_group<4>(bench, "small-4B", object_count);
    run_lifetime_group<96>(bench, "large-96B", object_count);
}
