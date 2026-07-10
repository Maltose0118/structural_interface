#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <proxy/proxy.h>
#include <structural_interface.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
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

template <std::size_t ObjectBytes, std::size_t UsedBytes>
struct padding_for {
    static_assert(ObjectBytes >= UsedBytes);
    std::array<std::byte, ObjectBytes - UsedBytes> bytes{};
};

template <std::size_t ObjectBytes>
struct padding_for<ObjectBytes, ObjectBytes> {};

struct StepInterface {
    std::uint64_t step(std::uint64_t input) noexcept;
};

struct MultiStepInterface {
    std::uint64_t step(std::uint64_t input) noexcept;
    std::uint64_t mix(std::uint64_t left, std::uint64_t right) noexcept;
    std::uint64_t read() const noexcept;
};

struct FieldInterface {
    std::uint64_t value;
};

struct ManyFieldsInterface {
    std::uint16_t value0;
    std::uint16_t value1;
    std::uint16_t value2;
    std::uint16_t value3;
    std::uint16_t value4;
    std::uint16_t value5;
    std::uint16_t value6;
    std::uint16_t value7;
};

struct VirtualStep {
    virtual ~VirtualStep() = default;
    virtual std::uint64_t step(std::uint64_t input) noexcept = 0;
};

struct VirtualMultiStep {
    virtual ~VirtualMultiStep() = default;
    virtual std::uint64_t step(std::uint64_t input) noexcept = 0;
    virtual std::uint64_t mix(std::uint64_t left, std::uint64_t right) noexcept = 0;
    virtual std::uint64_t read() const noexcept = 0;
};

struct VirtualField {
    virtual ~VirtualField() = default;
    virtual std::uint64_t& value_ref() noexcept = 0;
};

struct VirtualManyFields {
    virtual ~VirtualManyFields() = default;
    virtual std::uint16_t& value0_ref() noexcept = 0;
    virtual std::uint16_t& value1_ref() noexcept = 0;
    virtual std::uint16_t& value2_ref() noexcept = 0;
    virtual std::uint16_t& value3_ref() noexcept = 0;
    virtual std::uint16_t& value4_ref() noexcept = 0;
    virtual std::uint16_t& value5_ref() noexcept = 0;
    virtual std::uint16_t& value6_ref() noexcept = 0;
    virtual std::uint16_t& value7_ref() noexcept = 0;
};

template <int Id, std::size_t ObjectBytes>
struct Model {
    [[no_unique_address]] padding_for<ObjectBytes, sizeof(std::uint64_t)> payload{};
    std::uint64_t value = static_cast<std::uint64_t>(Id + 1);

    std::uint64_t step(std::uint64_t input) noexcept {
        value = value * 1'103'515'245u + input + static_cast<std::uint64_t>(Id + 12'345);
        return value ^ (value >> ((Id % 13) + 1));
    }

    std::uint64_t mix(std::uint64_t left, std::uint64_t right) noexcept {
        value += (left * static_cast<std::uint64_t>(Id + 3)) ^ (right + 0x9e3779b97f4a7c15ull);
        return value;
    }

    std::uint64_t read() const noexcept {
        return value;
    }

    std::uint64_t& value_ref() noexcept {
        return value;
    }
};

template <int Id, std::size_t ObjectBytes>
struct ManyFieldsModel {
    static_assert(ObjectBytes >= sizeof(std::uint16_t) * 8);
    [[no_unique_address]] padding_for<ObjectBytes, sizeof(std::uint16_t) * 8> payload{};
    std::uint16_t value0 = static_cast<std::uint16_t>(Id + 1);
    std::uint16_t value1 = static_cast<std::uint16_t>(Id + 2);
    std::uint16_t value2 = static_cast<std::uint16_t>(Id + 3);
    std::uint16_t value3 = static_cast<std::uint16_t>(Id + 4);
    std::uint16_t value4 = static_cast<std::uint16_t>(Id + 5);
    std::uint16_t value5 = static_cast<std::uint16_t>(Id + 6);
    std::uint16_t value6 = static_cast<std::uint16_t>(Id + 7);
    std::uint16_t value7 = static_cast<std::uint16_t>(Id + 8);

    std::uint16_t& value0_ref() noexcept { return value0; }
    std::uint16_t& value1_ref() noexcept { return value1; }
    std::uint16_t& value2_ref() noexcept { return value2; }
    std::uint16_t& value3_ref() noexcept { return value3; }
    std::uint16_t& value4_ref() noexcept { return value4; }
    std::uint16_t& value5_ref() noexcept { return value5; }
    std::uint16_t& value6_ref() noexcept { return value6; }
    std::uint16_t& value7_ref() noexcept { return value7; }
};

template <int Id, std::size_t PayloadBytes>
struct VirtualStepModel final : VirtualStep {
    Model<Id, PayloadBytes> model{};

    std::uint64_t step(std::uint64_t input) noexcept override {
        return model.step(input);
    }
};

template <int Id, std::size_t PayloadBytes>
struct VirtualMultiStepModel final : VirtualMultiStep {
    Model<Id, PayloadBytes> model{};

    std::uint64_t step(std::uint64_t input) noexcept override {
        return model.step(input);
    }

    std::uint64_t mix(std::uint64_t left, std::uint64_t right) noexcept override {
        return model.mix(left, right);
    }

    std::uint64_t read() const noexcept override {
        return model.read();
    }
};

template <int Id, std::size_t PayloadBytes>
struct VirtualFieldModel final : VirtualField {
    Model<Id, PayloadBytes> model{};

    std::uint64_t& value_ref() noexcept override {
        return model.value_ref();
    }
};

template <int Id, std::size_t PayloadBytes>
struct VirtualManyFieldsModel final : VirtualManyFields {
    ManyFieldsModel<Id, PayloadBytes> model{};

    std::uint16_t& value0_ref() noexcept override { return model.value0_ref(); }
    std::uint16_t& value1_ref() noexcept override { return model.value1_ref(); }
    std::uint16_t& value2_ref() noexcept override { return model.value2_ref(); }
    std::uint16_t& value3_ref() noexcept override { return model.value3_ref(); }
    std::uint16_t& value4_ref() noexcept override { return model.value4_ref(); }
    std::uint16_t& value5_ref() noexcept override { return model.value5_ref(); }
    std::uint16_t& value6_ref() noexcept override { return model.value6_ref(); }
    std::uint16_t& value7_ref() noexcept override { return model.value7_ref(); }
};

PRO_DEF_MEM_DISPATCH(MemStep, step);
PRO_DEF_MEM_DISPATCH(MemMix, mix);
PRO_DEF_MEM_DISPATCH(MemRead, read);
PRO_DEF_MEM_DISPATCH(MemValueRef, value_ref);
PRO_DEF_MEM_DISPATCH(MemValue0Ref, value0_ref);
PRO_DEF_MEM_DISPATCH(MemValue1Ref, value1_ref);
PRO_DEF_MEM_DISPATCH(MemValue2Ref, value2_ref);
PRO_DEF_MEM_DISPATCH(MemValue3Ref, value3_ref);
PRO_DEF_MEM_DISPATCH(MemValue4Ref, value4_ref);
PRO_DEF_MEM_DISPATCH(MemValue5Ref, value5_ref);
PRO_DEF_MEM_DISPATCH(MemValue6Ref, value6_ref);
PRO_DEF_MEM_DISPATCH(MemValue7Ref, value7_ref);

struct ProxyStep : pro::facade_builder
    ::add_convention<MemStep, std::uint64_t(std::uint64_t) noexcept>
    ::support_copy<pro::constraint_level::nontrivial>
    ::build {};

struct ProxyMultiStep : pro::facade_builder
    ::add_convention<MemStep, std::uint64_t(std::uint64_t) noexcept>
    ::add_convention<MemMix, std::uint64_t(std::uint64_t, std::uint64_t) noexcept>
    ::add_convention<MemRead, std::uint64_t() const noexcept>
    ::support_copy<pro::constraint_level::nontrivial>
    ::build {};

struct ProxyField : pro::facade_builder
    ::add_convention<MemValueRef, std::uint64_t&() noexcept>
    ::support_copy<pro::constraint_level::nontrivial>
    ::build {};

struct ProxyManyFields : pro::facade_builder
    ::add_convention<MemValue0Ref, std::uint16_t&() noexcept>
    ::add_convention<MemValue1Ref, std::uint16_t&() noexcept>
    ::add_convention<MemValue2Ref, std::uint16_t&() noexcept>
    ::add_convention<MemValue3Ref, std::uint16_t&() noexcept>
    ::add_convention<MemValue4Ref, std::uint16_t&() noexcept>
    ::add_convention<MemValue5Ref, std::uint16_t&() noexcept>
    ::add_convention<MemValue6Ref, std::uint16_t&() noexcept>
    ::add_convention<MemValue7Ref, std::uint16_t&() noexcept>
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

template <class VirtualBase, template <int, std::size_t> class VirtualModel, std::size_t PayloadBytes>
std::vector<std::unique_ptr<VirtualBase>> make_virtual_objects(std::size_t object_count) {
    std::vector<std::unique_ptr<VirtualBase>> objects;
    objects.reserve(object_count);
    for (std::size_t index = 0; index != object_count; ++index) {
        with_type_index(index, [&]<int Id> {
            objects.emplace_back(std::make_unique<VirtualModel<Id, PayloadBytes>>());
        });
    }
    return objects;
}

template <class Interface, std::size_t PayloadBytes>
std::vector<si::existential_move_only<Interface>> make_si_objects(std::size_t object_count) {
    std::vector<si::existential_move_only<Interface>> objects;
    objects.reserve(object_count);
    for (std::size_t index = 0; index != object_count; ++index) {
        with_type_index(index, [&]<int Id> {
            objects.emplace_back(Model<Id, PayloadBytes>{});
        });
    }
    return objects;
}

template <class Interface, std::size_t PayloadBytes>
std::vector<si::existential_move_only<Interface>> make_si_many_field_objects(
    std::size_t object_count) {
    std::vector<si::existential_move_only<Interface>> objects;
    objects.reserve(object_count);
    for (std::size_t index = 0; index != object_count; ++index) {
        with_type_index(index, [&]<int Id> {
            objects.emplace_back(ManyFieldsModel<Id, PayloadBytes>{});
        });
    }
    return objects;
}

template <class Facade, std::size_t PayloadBytes>
std::vector<pro::proxy<Facade>> make_proxy_objects(std::size_t object_count) {
    std::vector<pro::proxy<Facade>> objects;
    objects.reserve(object_count);
    for (std::size_t index = 0; index != object_count; ++index) {
        with_type_index(index, [&]<int Id> {
            objects.emplace_back(pro::make_proxy<Facade, Model<Id, PayloadBytes>>());
        });
    }
    return objects;
}

template <class Facade, std::size_t PayloadBytes>
std::vector<pro::proxy<Facade>> make_proxy_many_field_objects(std::size_t object_count) {
    std::vector<pro::proxy<Facade>> objects;
    objects.reserve(object_count);
    for (std::size_t index = 0; index != object_count; ++index) {
        with_type_index(index, [&]<int Id> {
            objects.emplace_back(pro::make_proxy<Facade, ManyFieldsModel<Id, PayloadBytes>>());
        });
    }
    return objects;
}

template <class Objects, class Call>
void run_bench(ankerl::nanobench::Bench& bench,
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
void run_single_function_group(ankerl::nanobench::Bench& bench,
                               std::string_view label,
                               std::size_t object_count) {
    auto virtual_objects =
        make_virtual_objects<VirtualStep, VirtualStepModel, PayloadBytes>(object_count);
    run_bench(bench, std::string{label} + "/single/virtual", virtual_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  return object->step(input);
              });

    auto si_objects = make_si_objects<StepInterface, PayloadBytes>(object_count);
    run_bench(bench, std::string{label} + "/single/si", si_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  return object.step(input);
              });

    auto proxy_objects = make_proxy_objects<ProxyStep, PayloadBytes>(object_count);
    run_bench(bench, std::string{label} + "/single/proxy", proxy_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  return object->step(input);
              });
}

template <std::size_t PayloadBytes>
void run_multi_function_group(ankerl::nanobench::Bench& bench,
                              std::string_view label,
                              std::size_t object_count) {
    auto virtual_objects =
        make_virtual_objects<VirtualMultiStep, VirtualMultiStepModel, PayloadBytes>(object_count);
    run_bench(bench, std::string{label} + "/multi/virtual", virtual_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  return object->step(input) ^ object->mix(input, input + 7) ^ object->read();
              });

    auto si_objects = make_si_objects<MultiStepInterface, PayloadBytes>(object_count);
    run_bench(bench, std::string{label} + "/multi/si", si_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  return object.step(input) ^ object.mix(input, input + 7) ^ object.read();
              });

    auto proxy_objects = make_proxy_objects<ProxyMultiStep, PayloadBytes>(object_count);
    run_bench(bench, std::string{label} + "/multi/proxy", proxy_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  return object->step(input) ^ object->mix(input, input + 7) ^ object->read();
              });
}

template <std::size_t PayloadBytes>
void run_field_group(ankerl::nanobench::Bench& bench,
                     std::string_view label,
                     std::size_t object_count) {
    auto virtual_objects =
        make_virtual_objects<VirtualField, VirtualFieldModel, PayloadBytes>(object_count);
    run_bench(bench, std::string{label} + "/field/virtual-ref", virtual_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  auto& value = object->value_ref();
                  value += input;
                  return value;
              });

    auto si_objects = make_si_objects<FieldInterface, PayloadBytes>(object_count);
    run_bench(bench, std::string{label} + "/field/si-data-member", si_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  *object.value += input;
                  return *object.value;
              });

    auto proxy_objects = make_proxy_objects<ProxyField, PayloadBytes>(object_count);
    run_bench(bench, std::string{label} + "/field/proxy-ref", proxy_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  auto& value = object->value_ref();
                  value += input;
                  return value;
              });
}

template <std::size_t PayloadBytes>
void run_many_fields_group(ankerl::nanobench::Bench& bench,
                           std::string_view label,
                           std::size_t object_count) {
    auto virtual_objects =
        make_virtual_objects<VirtualManyFields, VirtualManyFieldsModel, PayloadBytes>(
            object_count);
    run_bench(bench, std::string{label} + "/many-fields/virtual-ref", virtual_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  auto& value0 = object->value0_ref();
                  auto& value1 = object->value1_ref();
                  auto& value2 = object->value2_ref();
                  auto& value3 = object->value3_ref();
                  auto& value4 = object->value4_ref();
                  auto& value5 = object->value5_ref();
                  auto& value6 = object->value6_ref();
                  auto& value7 = object->value7_ref();
                  value0 += input;
                  value1 += value0;
                  value2 += value1;
                  value3 += value2;
                  value4 += value3;
                  value5 += value4;
                  value6 += value5;
                  value7 += value6;
                  return value0 ^ value1 ^ value2 ^ value3 ^ value4 ^ value5 ^ value6 ^ value7;
              });

    auto si_objects = make_si_many_field_objects<ManyFieldsInterface, PayloadBytes>(object_count);
    run_bench(bench, std::string{label} + "/many-fields/si-data-members", si_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  *object.value0 += input;
                  *object.value1 += *object.value0;
                  *object.value2 += *object.value1;
                  *object.value3 += *object.value2;
                  *object.value4 += *object.value3;
                  *object.value5 += *object.value4;
                  *object.value6 += *object.value5;
                  *object.value7 += *object.value6;
                  return *object.value0 ^ *object.value1 ^ *object.value2 ^ *object.value3 ^
                         *object.value4 ^ *object.value5 ^ *object.value6 ^ *object.value7;
              });

    auto proxy_objects = make_proxy_many_field_objects<ProxyManyFields, PayloadBytes>(object_count);
    run_bench(bench, std::string{label} + "/many-fields/proxy-ref", proxy_objects,
              [](auto& object, std::uint64_t input) noexcept {
                  auto& value0 = object->value0_ref();
                  auto& value1 = object->value1_ref();
                  auto& value2 = object->value2_ref();
                  auto& value3 = object->value3_ref();
                  auto& value4 = object->value4_ref();
                  auto& value5 = object->value5_ref();
                  auto& value6 = object->value6_ref();
                  auto& value7 = object->value7_ref();
                  value0 += input;
                  value1 += value0;
                  value2 += value1;
                  value3 += value2;
                  value4 += value3;
                  value5 += value4;
                  value6 += value5;
                  value7 += value6;
                  return value0 ^ value1 ^ value2 ^ value3 ^ value4 ^ value5 ^ value6 ^ value7;
              });
}

template <std::size_t PayloadBytes>
void run_lifetime_group(ankerl::nanobench::Bench& bench,
                        std::string_view label,
                        std::size_t object_count) {
    run_lifetime_bench(bench, std::string{label} + "/lifetime/virtual",
                       object_count,
                       make_virtual_objects<VirtualStep, VirtualStepModel, PayloadBytes>);
    run_lifetime_bench(bench, std::string{label} + "/lifetime/si",
                       object_count, make_si_objects<StepInterface, PayloadBytes>);
    run_lifetime_bench(bench, std::string{label} + "/lifetime/proxy",
                       object_count, make_proxy_objects<ProxyStep, PayloadBytes>);
}

template <std::size_t PayloadBytes>
void run_payload_groups(ankerl::nanobench::Bench& bench,
                        std::string_view label,
                        std::size_t object_count) {
    run_single_function_group<PayloadBytes>(bench, label, object_count);
    run_multi_function_group<PayloadBytes>(bench, label, object_count);
    run_field_group<PayloadBytes>(bench, label, object_count);
    run_many_fields_group<PayloadBytes>(bench, label, object_count);
    run_lifetime_group<PayloadBytes>(bench, label, object_count);
}

} // namespace

int main(int argc, char** argv) {
    auto object_count = parse_object_count(argc, argv);

    ankerl::nanobench::Bench bench;
    bench.title("Structural Interface vs virtual functions vs Microsoft Proxy")
        .unit("dispatch or object")
        .warmup(3)
        .minEpochIterations(200)
        .relative(true);

    run_payload_groups<16>(bench, "small-16B", object_count);
    run_payload_groups<96>(bench, "large-96B", object_count);
}
