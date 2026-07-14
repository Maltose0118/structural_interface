#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <meta>
#include <new>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace si {

template <class I, class Allocator = std::allocator<std::byte>>
class existential;

template <class I, class Allocator = std::allocator<std::byte>>
class existential_move_only;

template <class I>
class existential_ref;

inline constexpr std::size_t sbo_storage_size = sizeof(void*) * 2;
inline constexpr std::size_t sbo_storage_alignment = alignof(void*);

namespace detail {

template <class Owner>
inline constexpr bool read_only_reference_owner = false;

template <class I>
inline constexpr bool read_only_reference_owner<si::existential_ref<I>> =
    std::is_const_v<I>;

using meta_info = std::meta::info;

template <class T>
using bare_t = std::remove_cvref_t<T>;

consteval auto reflection_access_context() noexcept {
    return std::meta::access_context::current();
}

consteval auto reflected_members(meta_info type) {
    return std::meta::reflect_constant_array(
        std::meta::members_of(type, reflection_access_context()));
}

consteval auto reflected_bases(meta_info type) {
    return std::meta::reflect_constant_array(
        std::meta::bases_of(type, reflection_access_context()));
}

consteval bool is_relevant_member(meta_info member) {
    if (!std::meta::is_public(member)) {
        return false;
    }

    return !std::meta::is_special_member_function(member) &&
           !std::meta::is_constructor(member) &&
           !std::meta::is_destructor(member) &&
           !std::meta::is_static_member(member) &&
           (std::meta::is_nonstatic_data_member(member) ||
            std::meta::is_function(member));
}

consteval bool is_interface_function(meta_info member) {
    return is_relevant_member(member) && std::meta::is_function(member);
}

consteval bool is_interface_field(meta_info member) {
    return is_relevant_member(member) && std::meta::is_nonstatic_data_member(member);
}

consteval bool identifier_equal(meta_info left, meta_info right) {
    return std::meta::has_identifier(left) &&
           std::meta::has_identifier(right) &&
           std::meta::identifier_of(left) == std::meta::identifier_of(right);
}

consteval bool type_equal(meta_info left, meta_info right) {
    return std::meta::dealias(left) == std::meta::dealias(right);
}

consteval bool parameter_lists_equal(meta_info left, meta_info right) {
    auto left_parameters = std::meta::parameters_of(left);
    auto right_parameters = std::meta::parameters_of(right);
    if (left_parameters.size() != right_parameters.size()) {
        return false;
    }

    for (std::size_t index = 0; index != left_parameters.size(); ++index) {
        if (!type_equal(std::meta::type_of(left_parameters[index]),
                        std::meta::type_of(right_parameters[index]))) {
            return false;
        }
    }

    return true;
}

consteval bool function_shapes_equal(meta_info left, meta_info right) {
    return type_equal(std::meta::return_type_of(left), std::meta::return_type_of(right)) &&
           parameter_lists_equal(left, right) &&
           std::meta::is_const(left) == std::meta::is_const(right) &&
           std::meta::is_volatile(left) == std::meta::is_volatile(right) &&
           std::meta::is_lvalue_reference_qualified(left) ==
               std::meta::is_lvalue_reference_qualified(right) &&
           std::meta::is_rvalue_reference_qualified(left) ==
               std::meta::is_rvalue_reference_qualified(right) &&
           std::meta::is_noexcept(left) == std::meta::is_noexcept(right);
}

consteval bool data_member_shapes_equal(meta_info left, meta_info right) {
    return type_equal(std::meta::type_of(left), std::meta::type_of(right));
}

consteval bool member_shapes_equal(meta_info left, meta_info right) {
    if (std::meta::is_nonstatic_data_member(left) &&
        std::meta::is_nonstatic_data_member(right)) {
        return data_member_shapes_equal(left, right);
    }

    if (std::meta::is_function(left) && std::meta::is_function(right)) {
        return function_shapes_equal(left, right);
    }

    return false;
}

template <auto Member>
struct member_pointer_type {
    using type = decltype(&[: Member :]);
};

template <auto Member>
using member_pointer_type_t = typename member_pointer_type<Member>::type;

template <class Owner, auto InterfaceMember, class Signature = member_pointer_type_t<InterfaceMember>>
struct function_proxy;

template <class Owner, class I>
struct generated_interface_members;

template <class Owner, class I>
using generated_interface_members_t = typename generated_interface_members<Owner, I>::type;

template <class Ptr, class Owner>
struct rebind_member_pointer;

template <class M, class C, class Owner>
struct rebind_member_pointer<M C::*, Owner> {
    using type = M Owner::*;
};

template <class Ptr>
struct member_object_type;

template <class M, class C>
struct member_object_type<M C::*> {
    using type = M;
};

template <auto Member>
using member_object_type_t = typename member_object_type<member_pointer_type_t<Member>>::type;

#define SI_REBIND_MEMBER_FUNCTION(CV, REF, NOEXCEPT_SPEC)                       \
    template <class R, class C, class... Args, class Owner>                     \
    struct rebind_member_pointer<R (C::*)(Args...) CV REF NOEXCEPT_SPEC, Owner> { \
        using type = R (Owner::*)(Args...) CV REF NOEXCEPT_SPEC;                \
    };

SI_REBIND_MEMBER_FUNCTION(, , )
SI_REBIND_MEMBER_FUNCTION(const, , )
SI_REBIND_MEMBER_FUNCTION(volatile, , )
SI_REBIND_MEMBER_FUNCTION(const volatile, , )
SI_REBIND_MEMBER_FUNCTION(, &, )
SI_REBIND_MEMBER_FUNCTION(const, &, )
SI_REBIND_MEMBER_FUNCTION(volatile, &, )
SI_REBIND_MEMBER_FUNCTION(const volatile, &, )
SI_REBIND_MEMBER_FUNCTION(, &&, )
SI_REBIND_MEMBER_FUNCTION(const, &&, )
SI_REBIND_MEMBER_FUNCTION(volatile, &&, )
SI_REBIND_MEMBER_FUNCTION(const volatile, &&, )
SI_REBIND_MEMBER_FUNCTION(, , noexcept)
SI_REBIND_MEMBER_FUNCTION(const, , noexcept)
SI_REBIND_MEMBER_FUNCTION(volatile, , noexcept)
SI_REBIND_MEMBER_FUNCTION(const volatile, , noexcept)
SI_REBIND_MEMBER_FUNCTION(, &, noexcept)
SI_REBIND_MEMBER_FUNCTION(const, &, noexcept)
SI_REBIND_MEMBER_FUNCTION(volatile, &, noexcept)
SI_REBIND_MEMBER_FUNCTION(const volatile, &, noexcept)
SI_REBIND_MEMBER_FUNCTION(, &&, noexcept)
SI_REBIND_MEMBER_FUNCTION(const, &&, noexcept)
SI_REBIND_MEMBER_FUNCTION(volatile, &&, noexcept)
SI_REBIND_MEMBER_FUNCTION(const volatile, &&, noexcept)

#undef SI_REBIND_MEMBER_FUNCTION

template <class Ptr, class Owner>
using rebind_member_pointer_t = typename rebind_member_pointer<Ptr, Owner>::type;

template <auto InterfaceMember, class T>
consteval rebind_member_pointer_t<member_pointer_type_t<InterfaceMember>, bare_t<T>>
get_concrete_member_pointer() {
    constexpr auto member = [] consteval {
        using interface_pointer = member_pointer_type_t<InterfaceMember>;
        using concrete_pointer = rebind_member_pointer_t<interface_pointer, bare_t<T>>;

        template for (constexpr auto concrete_member : [: reflected_members(^^bare_t<T>) :]) {
            if constexpr (is_relevant_member(concrete_member) &&
                          identifier_equal(InterfaceMember, concrete_member) &&
                          member_shapes_equal(InterfaceMember, concrete_member)) {
                if constexpr (requires { std::meta::extract<concrete_pointer>(concrete_member); }) {
                    return concrete_member;
                }
            }
        }

        return meta_info{};
    }();

    using interface_pointer = member_pointer_type_t<InterfaceMember>;
    using concrete_pointer = rebind_member_pointer_t<interface_pointer, bare_t<T>>;
    return std::meta::extract<concrete_pointer>(member);
}

template <class Owner>
void* get_object_pointer(Owner& owner) noexcept;

template <auto InterfaceMember, class ErasedCall, class Slots>
ErasedCall get_function_slot(const Slots& slots) noexcept;

template <class Generated, auto InterfaceMember>
consteval std::ptrdiff_t get_generated_member_offset();

#define SI_FUNCTION_PROXY(CV, REF, NOEXCEPT_SPEC, OBJECT_EXPR)                  \
    template <class Owner, auto InterfaceMember, class I, class R, class... Args> \
    struct function_proxy<Owner, InterfaceMember, R (I::*)(Args...) CV REF NOEXCEPT_SPEC> { \
        using erased_call = R (*)(void*, Args...) NOEXCEPT_SPEC;                \
                                                                                \
        R operator()(Args... args) CV REF NOEXCEPT_SPEC {                       \
            constexpr auto offset = get_generated_member_offset<               \
                generated_interface_members_t<Owner, I>, InterfaceMember>();  \
            auto* self = const_cast<function_proxy*>(this);                     \
            auto* owner = reinterpret_cast<Owner*>(                             \
                reinterpret_cast<std::byte*>(self) - offset);                   \
            void* object = get_object_pointer(*owner);                          \
            auto call = get_function_slot<InterfaceMember, erased_call>(         \
                owner->_si_details_.metadata_ptr()->functions);                 \
            if constexpr (std::is_void_v<R>) {                                  \
                call(object, std::forward<Args>(args)...);                      \
            } else {                                                            \
                return call(object, std::forward<Args>(args)...);               \
            }                                                                   \
        }                                                                       \
    };

#define SI_PROXY_OBJECT_LVALUE(PTR) (*(PTR))
#define SI_PROXY_OBJECT_RVALUE(PTR) (std::move(*(PTR)))

SI_FUNCTION_PROXY(, , , SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(const, , , SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(volatile, , , SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(const volatile, , , SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(, &, , SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(const, &, , SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(volatile, &, , SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(const volatile, &, , SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(, &&, , SI_PROXY_OBJECT_RVALUE)
SI_FUNCTION_PROXY(const, &&, , SI_PROXY_OBJECT_RVALUE)
SI_FUNCTION_PROXY(volatile, &&, , SI_PROXY_OBJECT_RVALUE)
SI_FUNCTION_PROXY(const volatile, &&, , SI_PROXY_OBJECT_RVALUE)
SI_FUNCTION_PROXY(, , noexcept, SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(const, , noexcept, SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(volatile, , noexcept, SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(const volatile, , noexcept, SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(, &, noexcept, SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(const, &, noexcept, SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(volatile, &, noexcept, SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(const volatile, &, noexcept, SI_PROXY_OBJECT_LVALUE)
SI_FUNCTION_PROXY(, &&, noexcept, SI_PROXY_OBJECT_RVALUE)
SI_FUNCTION_PROXY(const, &&, noexcept, SI_PROXY_OBJECT_RVALUE)
SI_FUNCTION_PROXY(volatile, &&, noexcept, SI_PROXY_OBJECT_RVALUE)
SI_FUNCTION_PROXY(const volatile, &&, noexcept, SI_PROXY_OBJECT_RVALUE)

#undef SI_PROXY_OBJECT_RVALUE
#undef SI_PROXY_OBJECT_LVALUE
#undef SI_FUNCTION_PROXY

template <auto InterfaceMember, class T>
void* get_concrete_field_address(void* object) noexcept {
    constexpr auto member = get_concrete_member_pointer<InterfaceMember, T>();
    return std::addressof(static_cast<T*>(object)->*member);
}

template <auto InterfaceMember, class T>
const void* get_concrete_field_address(const void* object) noexcept {
    constexpr auto member = get_concrete_member_pointer<InterfaceMember, T>();
    return std::addressof(static_cast<const T*>(object)->*member);
}

template <auto InterfaceMember, class Signature = member_pointer_type_t<InterfaceMember>>
struct erased_function_pointer;

#define SI_ERASED_FUNCTION_POINTER(CV, REF, NOEXCEPT_SPEC, OBJECT_EXPR)         \
    template <auto InterfaceMember, class I, class R, class... Args>            \
    struct erased_function_pointer<InterfaceMember, R (I::*)(Args...) CV REF NOEXCEPT_SPEC> { \
        using type = R (*)(void*, Args...) NOEXCEPT_SPEC;                       \
                                                                                \
        template <class T>                                                      \
        consteval static type thunk_for() {                                     \
            return +[](void* object, Args... args) NOEXCEPT_SPEC -> R {          \
                constexpr auto member = get_concrete_member_pointer<InterfaceMember, T>(); \
                if constexpr (std::is_void_v<R>) {                              \
                    std::invoke(member, OBJECT_EXPR(static_cast<T*>(object)), std::forward<Args>(args)...); \
                } else {                                                        \
                    return std::invoke(member, OBJECT_EXPR(static_cast<T*>(object)), std::forward<Args>(args)...); \
                }                                                               \
            };                                                                  \
        }                                                                       \
    };

#define SI_PROXY_OBJECT_LVALUE(PTR) (*(PTR))
#define SI_PROXY_OBJECT_RVALUE(PTR) (std::move(*(PTR)))

SI_ERASED_FUNCTION_POINTER(, , , SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(const, , , SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(volatile, , , SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(const volatile, , , SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(, &, , SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(const, &, , SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(volatile, &, , SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(const volatile, &, , SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(, &&, , SI_PROXY_OBJECT_RVALUE)
SI_ERASED_FUNCTION_POINTER(const, &&, , SI_PROXY_OBJECT_RVALUE)
SI_ERASED_FUNCTION_POINTER(volatile, &&, , SI_PROXY_OBJECT_RVALUE)
SI_ERASED_FUNCTION_POINTER(const volatile, &&, , SI_PROXY_OBJECT_RVALUE)
SI_ERASED_FUNCTION_POINTER(, , noexcept, SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(const, , noexcept, SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(volatile, , noexcept, SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(const volatile, , noexcept, SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(, &, noexcept, SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(const, &, noexcept, SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(volatile, &, noexcept, SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(const volatile, &, noexcept, SI_PROXY_OBJECT_LVALUE)
SI_ERASED_FUNCTION_POINTER(, &&, noexcept, SI_PROXY_OBJECT_RVALUE)
SI_ERASED_FUNCTION_POINTER(const, &&, noexcept, SI_PROXY_OBJECT_RVALUE)
SI_ERASED_FUNCTION_POINTER(volatile, &&, noexcept, SI_PROXY_OBJECT_RVALUE)
SI_ERASED_FUNCTION_POINTER(const volatile, &&, noexcept, SI_PROXY_OBJECT_RVALUE)

#undef SI_PROXY_OBJECT_RVALUE
#undef SI_PROXY_OBJECT_LVALUE
#undef SI_ERASED_FUNCTION_POINTER

template <class I>
struct generated_function_slots {
    struct type;

    consteval {
        std::vector<meta_info> specs;
        template for (constexpr auto member : [: reflected_members(^^I) :]) {
            if constexpr (is_interface_function(member)) {
                using proxy_type = typename erased_function_pointer<member>::type;
                specs.push_back(std::meta::data_member_spec(
                    ^^proxy_type,
                    {.name = std::string(std::meta::identifier_of(member))}));
            }
        }
        std::meta::define_aggregate(^^type, specs);
    }
};

template <class I>
using generated_function_slots_t = typename generated_function_slots<I>::type;

template <class Generated, auto InterfaceMember, class Proxy>
consteval auto get_member_pointer() {
    template for (constexpr auto member : [: reflected_members(^^Generated) :]) {
        if constexpr (std::meta::is_nonstatic_data_member(member) &&
                      std::meta::has_identifier(member) &&
                      std::meta::identifier_of(member) ==
                          std::meta::identifier_of(InterfaceMember)) {
            if constexpr (requires { std::meta::extract<Proxy Generated::*>(member); }) {
                return std::meta::extract<Proxy Generated::*>(member);
            }
        }
    }

    return static_cast<Proxy Generated::*>(nullptr);
}

template <class Generated, auto InterfaceMember>
consteval meta_info get_generated_member_info() {
    template for (constexpr auto member : [: reflected_members(^^Generated) :]) {
        if constexpr (std::meta::is_nonstatic_data_member(member) &&
                      std::meta::has_identifier(member) &&
                      std::meta::identifier_of(member) ==
                          std::meta::identifier_of(InterfaceMember)) {
            return member;
        }
    }

    return meta_info{};
}

template <class Generated, auto InterfaceMember>
consteval std::ptrdiff_t get_generated_member_offset() {
    constexpr auto member = get_generated_member_info<Generated, InterfaceMember>();
    constexpr auto offset = std::meta::offset_of(member);
    return offset.bytes;
}

template <class I, class T>
consteval generated_function_slots_t<I> make_function_slots() {
    generated_function_slots_t<I> slots{};
    template for (constexpr auto member : [: reflected_members(^^I) :]) {
        if constexpr (is_interface_function(member)) {
            using erased_call = typename erased_function_pointer<member>::type;
            constexpr auto slot_member =
                get_member_pointer<generated_function_slots_t<I>, member, erased_call>();
            slots.*slot_member = erased_function_pointer<member>::template thunk_for<T>();
        }
    }
    return slots;
}

template <auto InterfaceMember, class ErasedCall, class Slots>
ErasedCall get_function_slot(const Slots& slots) noexcept {
    constexpr auto slot_member =
        get_member_pointer<Slots, InterfaceMember, ErasedCall>();
    return slots.*slot_member;
}

template <class Owner, class I>
struct generated_interface_members {
    struct type;

    consteval {
        std::vector<meta_info> specs;
        template for (constexpr auto member : [: reflected_members(^^I) :]) {
            if constexpr (is_interface_function(member) &&
                          (!read_only_reference_owner<Owner> ||
                           std::meta::is_const(member))) {
                using proxy_type = function_proxy<Owner, member>;
                specs.push_back(std::meta::data_member_spec(
                    ^^proxy_type,
                    {.name = std::string(std::meta::identifier_of(member)),
                     .no_unique_address = true}));
            } else if constexpr (is_interface_field(member)) {
                using field_type = member_object_type_t<member>;
                using field_pointer = std::conditional_t<
                    read_only_reference_owner<Owner>, const field_type*, field_type*>;
                specs.push_back(std::meta::data_member_spec(
                    ^^field_pointer,
                    {.name = std::string(std::meta::identifier_of(member))}));
            }
        }
        std::meta::define_aggregate(^^type, specs);
    }
};

template <class Owner>
void* get_object_pointer(Owner& owner) noexcept {
    return owner._si_details_.object_ptr();
}

template <class Owner>
const void* get_const_object_pointer(const Owner& owner) noexcept {
    return owner._si_details_.object_ptr();
}

template <class Owner, class I, class T>
void bind_member_proxies(Owner& owner) noexcept {
    template for (constexpr auto member : [: reflected_members(^^I) :]) {
        if constexpr (is_interface_field(member)) {
            using field_type = member_object_type_t<member>;
            using field_pointer = std::conditional_t<
                read_only_reference_owner<Owner>, const field_type*, field_type*>;
            constexpr auto base_member =
                get_member_pointer<generated_interface_members_t<Owner, I>, member, field_pointer>();
            if constexpr (read_only_reference_owner<Owner>) {
                owner.*base_member = static_cast<field_pointer>(
                    get_concrete_field_address<member, T>(
                        static_cast<const void*>(owner._si_details_.object_ptr())));
            } else {
                owner.*base_member = static_cast<field_pointer>(
                    get_concrete_field_address<member, T>(get_object_pointer(owner)));
            }
        }
    }
}

template <class Owner, class I>
void copy_member_proxies(Owner& owner, const Owner& other) noexcept {
    if (!other._si_details_.has_object()) {
        template for (constexpr auto member : [: reflected_members(^^I) :]) {
            if constexpr (is_interface_field(member)) {
                using field_type = member_object_type_t<member>;
                constexpr auto proxy_member = get_member_pointer<
                    generated_interface_members_t<Owner, I>, member, field_type*>();
                owner.*proxy_member = nullptr;
            }
        }
        return;
    }

    template for (constexpr auto member : [: reflected_members(^^I) :]) {
        if constexpr (is_interface_field(member)) {
            using field_type = member_object_type_t<member>;
            constexpr auto base_member =
                get_member_pointer<generated_interface_members_t<Owner, I>, member, field_type*>();
            auto* other_object = static_cast<const std::byte*>(other._si_details_.object_ptr());
            auto* other_field = reinterpret_cast<const std::byte*>(other.*base_member);
            auto offset = other_field - other_object;
            owner.*base_member = reinterpret_cast<field_type*>(
                static_cast<std::byte*>(get_object_pointer(owner)) + offset);
        }
    }
}

template <auto InterfaceMember, class Concrete>
consteval bool concrete_has_matching_member() {
    using interface_pointer = member_pointer_type_t<InterfaceMember>;
    using concrete_pointer = rebind_member_pointer_t<interface_pointer, Concrete>;

    template for (constexpr auto concrete_member : [: reflected_members(^^Concrete) :]) {
        if constexpr (is_relevant_member(concrete_member) &&
                      identifier_equal(InterfaceMember, concrete_member) &&
                      member_shapes_equal(InterfaceMember, concrete_member) &&
                      std::meta::is_nonstatic_data_member(InterfaceMember) ==
                          std::meta::is_nonstatic_data_member(concrete_member) &&
                      std::meta::is_function(InterfaceMember) ==
                          std::meta::is_function(concrete_member)) {
            if constexpr (requires { std::meta::extract<concrete_pointer>(concrete_member); }) {
                return true;
            }
        }
    }

    return false;
}

template <class I, class T>
consteval bool satisfies_interface_members();

template <class I>
consteval bool has_interface_requirements();

template <auto Base>
consteval bool base_has_interface_requirements() {
    using base_type = typename [: std::meta::type_of(Base) :];
    return has_interface_requirements<base_type>();
}

template <class I>
consteval bool has_interface_requirements() {
    template for (constexpr auto base : [: reflected_bases(^^I) :]) {
        if (base_has_interface_requirements<base>()) {
            return true;
        }
    }

    template for (constexpr auto member : [: reflected_members(^^I) :]) {
        if constexpr (is_relevant_member(member)) {
            return true;
        }
    }

    return false;
}

template <auto Base, class T>
consteval bool satisfies_base() {
    using base_type = typename [: std::meta::type_of(Base) :];
    return satisfies_interface_members<base_type, T>();
}

template <class I, class T>
consteval bool satisfies_interface_members() {
    using concrete = bare_t<T>;

    template for (constexpr auto base : [: reflected_bases(^^I) :]) {
        if (!satisfies_base<base, concrete>()) {
            return false;
        }
    }

    template for (constexpr auto member : [: reflected_members(^^I) :]) {
        if constexpr (is_relevant_member(member)) {
            if (!concrete_has_matching_member<member, concrete>()) {
                return false;
            }
        }
    }

    return true;
}

template <class I, class Allocator>
struct interface_metadata {
    generated_function_slots_t<I> functions;
    const void* type_token;
    void (*destroy)(void*, bool, Allocator&) noexcept;
    void* (*copy_construct)(void*, const void*, bool, Allocator&);
    void* (*move_construct)(void*, void*, bool, Allocator&);
};

template <class I>
struct reference_metadata {
    generated_function_slots_t<I> functions;
    const void* type_token;
};

template <class T>
inline constexpr char type_token = 0;

template <class T>
inline constexpr bool fits_sbo =
    sizeof(T) <= sbo_storage_size &&
    alignof(T) <= sbo_storage_alignment &&
    alignof(T) <= alignof(std::max_align_t);

template <class T, class Allocator>
using object_allocator_t = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;

template <class T, class Allocator>
using object_allocator_traits = std::allocator_traits<object_allocator_t<T, Allocator>>;

template <class T, class Allocator>
void destroy_object(void* object, bool inline_storage, Allocator& allocator) noexcept {
    if (inline_storage) {
        std::destroy_at(static_cast<T*>(object));
    } else {
        using object_allocator = object_allocator_t<T, Allocator>;
        using traits = object_allocator_traits<T, Allocator>;
        object_allocator object_alloc(allocator);
        T* typed_object = static_cast<T*>(object);
        traits::destroy(object_alloc, typed_object);
        traits::deallocate(object_alloc, typed_object, 1);
    }
}

template <class T, class Allocator, class... Args>
T* allocate_construct(Allocator& allocator, Args&&... args) {
    using object_allocator = object_allocator_t<T, Allocator>;
    using traits = object_allocator_traits<T, Allocator>;
    object_allocator object_alloc(allocator);
    T* object = traits::allocate(object_alloc, 1);
    try {
        traits::construct(object_alloc, object, std::forward<Args>(args)...);
    } catch (...) {
        traits::deallocate(object_alloc, object, 1);
        throw;
    }
    return object;
}

template <class T, class Allocator>
void* copy_construct_object(void* destination_storage,
                            const void* source,
                            bool source_inline,
                            Allocator& allocator) {
    if (source_inline) {
        return ::new (destination_storage) T(*static_cast<const T*>(source));
    }
    return allocate_construct<T>(allocator, *static_cast<const T*>(source));
}

template <class T, class Allocator>
consteval auto get_copy_constructor() {
    if constexpr (std::copy_constructible<T>) {
        return &copy_construct_object<T, Allocator>;
    } else {
        return static_cast<void* (*)(void*, const void*, bool, Allocator&)>(nullptr);
    }
}

template <class T, class Allocator>
void* move_construct_object(void* destination_storage,
                            void* source,
                            bool source_inline,
                            Allocator& allocator) {
    if (source_inline) {
        return ::new (destination_storage) T(std::move(*static_cast<T*>(source)));
    }
    return allocate_construct<T>(allocator, std::move(*static_cast<T*>(source)));
}

template <class I, class T, class Allocator>
inline const interface_metadata<I, Allocator> metadata_for = {
    .functions = make_function_slots<I, T>(),
    .type_token = &type_token<T>,
    .destroy = &destroy_object<T, Allocator>,
    .copy_construct = get_copy_constructor<T, Allocator>(),
    .move_construct = &move_construct_object<T, Allocator>,
};

template <class I, class T>
inline const reference_metadata<I> reference_metadata_for = {
    .functions = make_function_slots<I, T>(),
    .type_token = &type_token<T>,
};

template <class I, class Owner, class Allocator>
struct existential_storage {
    union object_storage {
        alignas(sbo_storage_alignment) std::byte inline_storage[sbo_storage_size]{};
        void* heap_object;

        object_storage() noexcept : inline_storage{} {}
        ~object_storage() {}
    } storage;

    std::uintptr_t metadata_bits = 0;
    [[no_unique_address]] Allocator allocator{};

    using metadata_pointer = const interface_metadata<I, Allocator>*;
    using allocator_traits = std::allocator_traits<Allocator>;
    static constexpr std::uintptr_t inline_tag = 1;

    existential_storage() = default;

    bool is_inline() const noexcept {
        return (metadata_bits & inline_tag) != 0;
    }

    metadata_pointer metadata_ptr() const noexcept {
        return reinterpret_cast<metadata_pointer>(metadata_bits & ~inline_tag);
    }

    void set_metadata(metadata_pointer metadata, bool inline_storage) noexcept {
        auto bits = reinterpret_cast<std::uintptr_t>(metadata);
        metadata_bits = bits | (inline_storage ? inline_tag : 0);
    }

    void* object_ptr() noexcept {
        return is_inline() ? static_cast<void*>(storage.inline_storage)
                           : storage.heap_object;
    }

    const void* object_ptr() const noexcept {
        return is_inline() ? static_cast<const void*>(storage.inline_storage)
                           : storage.heap_object;
    }

    bool has_object() const noexcept {
        return metadata_bits != 0;
    }

    explicit existential_storage(const Allocator& allocator_value)
        : allocator(allocator_value) {}

    Allocator& allocator_ref() noexcept {
        return allocator;
    }

    const Allocator& allocator_ref() const noexcept {
        return allocator;
    }

    void destroy() noexcept {
        if (has_object()) {
            metadata_ptr()->destroy(object_ptr(), is_inline(), allocator);
            metadata_bits = 0;
        }
    }

    template <class T, class... Args>
    void emplace(Args&&... args) {
        using concrete = std::remove_cvref_t<T>;
        if constexpr (fits_sbo<concrete>) {
            ::new (storage.inline_storage) concrete(std::forward<Args>(args)...);
            set_metadata(&metadata_for<I, concrete, Allocator>, true);
        } else {
            storage.heap_object = allocate_construct<concrete>(allocator,
                                                                std::forward<Args>(args)...);
            set_metadata(&metadata_for<I, concrete, Allocator>, false);
        }
    }

    void copy_from(const existential_storage& other) {
        if (!other.has_object()) {
            return;
        }
        auto* other_metadata = other.metadata_ptr();
        bool inline_storage = other.is_inline();
        auto* copied = other_metadata->copy_construct(
            storage.inline_storage, other.object_ptr(), inline_storage, allocator);
        if (!inline_storage) {
            storage.heap_object = copied;
        }
        set_metadata(other_metadata, inline_storage);
    }

    template <class T, class... Args>
    void construct_value(Owner& owner, Args&&... args) {
        using concrete = std::remove_cvref_t<T>;
        emplace<concrete>(std::forward<Args>(args)...);
        bind_member_proxies<Owner, I, concrete>(owner);
    }

    void move_value_from(Owner& owner, Owner& other, bool can_steal) {
        if (!other._si_details_.has_object()) {
            return;
        }
        auto* other_metadata = other._si_details_.metadata_ptr();
        bool inline_storage = other._si_details_.is_inline();
        if (inline_storage) {
            other_metadata->move_construct(
                storage.inline_storage, other._si_details_.object_ptr(), true, allocator);
        } else if (can_steal) {
            storage.heap_object = other._si_details_.storage.heap_object;
        } else {
            auto* moved = other_metadata->move_construct(
                storage.inline_storage, other._si_details_.object_ptr(), false, allocator);
            storage.heap_object = moved;
        }
        set_metadata(other_metadata, inline_storage);
        copy_member_proxies<Owner, I>(owner, other);
        if (inline_storage) {
            other._si_details_.destroy();
        } else if (can_steal) {
            other._si_details_.storage.heap_object = nullptr;
            other._si_details_.metadata_bits = 0;
        } else {
            other._si_details_.destroy();
        }
    }
};

template <class I>
struct reference_storage {
    const void* object = nullptr;
    const reference_metadata<I>* metadata = nullptr;

    void* object_ptr() const noexcept {
        return const_cast<void*>(object);
    }

    const reference_metadata<I>* metadata_ptr() const noexcept {
        return metadata;
    }

    template <class T>
    void bind(T& value) noexcept {
        object = std::addressof(value);
        metadata = &reference_metadata_for<I, std::remove_cvref_t<T>>;
    }
};

template <class T, class Metadata>
bool metadata_matches(const Metadata* metadata) noexcept {
    return metadata != nullptr &&
           metadata->type_token == &type_token<std::remove_cvref_t<T>>;
}

template <class T>
inline constexpr bool is_owning_existential = false;

template <class I, class Allocator>
inline constexpr bool is_owning_existential<existential<I, Allocator>> = true;

template <class I, class Allocator>
inline constexpr bool is_owning_existential<existential_move_only<I, Allocator>> = true;

} // namespace detail

template <class T, class I>
concept satisfies =
    detail::has_interface_requirements<I>() &&
    (detail::satisfies_interface_members<I, T>() ||
     detail::is_owning_existential<std::remove_cvref_t<T>> ||
     std::same_as<std::remove_cvref_t<T>, existential_ref<I>>);

template <class I, class Allocator>
class existential : public detail::generated_interface_members_t<
                        existential<I, Allocator>, I> {
public:
    using allocator_type = Allocator;

    template <class T>
        requires (!std::same_as<std::remove_cvref_t<T>, existential>) &&
                 satisfies<std::remove_cvref_t<T>, I> &&
                 std::copy_constructible<std::remove_cvref_t<T>> &&
                 std::move_constructible<std::remove_cvref_t<T>>
    existential(T&& value) : _si_details_{} {
        _si_details_.template construct_value<std::remove_cvref_t<T>>(*this, std::forward<T>(value));
    }

    template <class T>
        requires satisfies<std::remove_cvref_t<T>, I> &&
                 std::copy_constructible<std::remove_cvref_t<T>> &&
                 std::move_constructible<std::remove_cvref_t<T>>
    existential(const Allocator& allocator, T&& value)
        : _si_details_(allocator) {
        _si_details_.template construct_value<std::remove_cvref_t<T>>(*this, std::forward<T>(value));
    }

    existential(const existential& other)
        : _si_details_(std::allocator_traits<Allocator>::
                           select_on_container_copy_construction(other._si_details_.allocator_ref())) {
        _si_details_.copy_from(other._si_details_);
        detail::copy_member_proxies<existential<I, Allocator>, I>(*this, other);
    }

    existential(existential&& other)
        : _si_details_(std::move(other._si_details_.allocator_ref())) {
        _si_details_.move_value_from(*this, other, true);
    }

    allocator_type get_allocator() const {
        return _si_details_.allocator_ref();
    }

    existential& operator=(const existential& other) {
        if (this == &other) {
            return *this;
        }
        using traits = std::allocator_traits<Allocator>;
        if constexpr (traits::propagate_on_container_copy_assignment::value) {
            _si_details_.destroy();
            _si_details_.allocator_ref() = other._si_details_.allocator_ref();
        } else {
            _si_details_.destroy();
        }
        _si_details_.copy_from(other._si_details_);
        detail::copy_member_proxies<existential<I, Allocator>, I>(*this, other);
        return *this;
    }

    existential& operator=(existential&& other) {
        if (this == &other) {
            return *this;
        }
        using traits = std::allocator_traits<Allocator>;
        if constexpr (traits::propagate_on_container_move_assignment::value) {
            _si_details_.destroy();
            _si_details_.allocator_ref() = std::move(other._si_details_.allocator_ref());
            _si_details_.move_value_from(*this, other, true);
        } else {
            bool can_steal = traits::is_always_equal::value ||
                             (_si_details_.allocator_ref() == other._si_details_.allocator_ref());
            _si_details_.destroy();
            _si_details_.move_value_from(*this, other, can_steal);
        }
        return *this;
    }

    ~existential() {
        _si_details_.destroy();
    }

private:
    template <class, class, class>
    friend struct detail::existential_storage;
    template <class Owner>
    friend void* detail::get_object_pointer(Owner&) noexcept;
    template <class Owner>
    friend const void* detail::get_const_object_pointer(const Owner&) noexcept;
    template <class, auto, class>
    friend struct detail::function_proxy;
    template <class Owner, class Interface, class T>
    friend void detail::bind_member_proxies(Owner&) noexcept;
    template <class Owner, class Interface>
    friend void detail::copy_member_proxies(Owner&, const Owner&) noexcept;

    template <class T, class J, class A>
    friend bool is(const existential<J, A>&) noexcept;
    template <class T, class J, class A>
    friend bool is(const existential_move_only<J, A>&) noexcept;
    template <class T, class J>
    friend bool is(const existential_ref<J>&) noexcept;
    template <class T, class J, class A>
    friend std::remove_cvref_t<T>& get(existential<J, A>&);
    template <class T, class J, class A>
    friend const std::remove_cvref_t<T>& get(const existential<J, A>&);
    template <class T, class J, class A>
    friend std::remove_cvref_t<T>& get(existential_move_only<J, A>&);
    template <class T, class J, class A>
    friend const std::remove_cvref_t<T>& get(const existential_move_only<J, A>&);
    template <class T, class J>
    friend std::remove_cvref_t<T>& get(existential_ref<J>&);
    template <class T, class J>
    friend const std::remove_cvref_t<T>& get(const existential_ref<J>&);

    detail::existential_storage<I, existential<I, Allocator>, Allocator> _si_details_;
};

template <class I, class Allocator>
class existential_move_only : public detail::generated_interface_members_t<
                                  existential_move_only<I, Allocator>, I> {
public:
    using allocator_type = Allocator;

    template <class T>
        requires (!std::same_as<std::remove_cvref_t<T>, existential_move_only>) &&
                 satisfies<std::remove_cvref_t<T>, I> &&
                 std::move_constructible<std::remove_cvref_t<T>>
    existential_move_only(T&& value) : _si_details_{} {
        _si_details_.template construct_value<std::remove_cvref_t<T>>(*this, std::forward<T>(value));
    }

    template <class T>
        requires satisfies<std::remove_cvref_t<T>, I> &&
                 std::move_constructible<std::remove_cvref_t<T>>
    existential_move_only(const Allocator& allocator, T&& value)
        : _si_details_(allocator) {
        _si_details_.template construct_value<std::remove_cvref_t<T>>(*this, std::forward<T>(value));
    }

    existential_move_only(const existential_move_only&) = delete;
    existential_move_only& operator=(const existential_move_only&) = delete;

    existential_move_only(existential_move_only&& other)
        : _si_details_(std::move(other._si_details_.allocator_ref())) {
        _si_details_.move_value_from(*this, other, true);
    }

    allocator_type get_allocator() const {
        return _si_details_.allocator_ref();
    }

    existential_move_only& operator=(existential_move_only&& other) {
        if (this == &other) {
            return *this;
        }
        using traits = std::allocator_traits<Allocator>;
        if constexpr (traits::propagate_on_container_move_assignment::value) {
            _si_details_.destroy();
            _si_details_.allocator_ref() = std::move(other._si_details_.allocator_ref());
            _si_details_.move_value_from(*this, other, true);
        } else {
            bool can_steal = traits::is_always_equal::value ||
                             (_si_details_.allocator_ref() == other._si_details_.allocator_ref());
            _si_details_.destroy();
            _si_details_.move_value_from(*this, other, can_steal);
        }
        return *this;
    }

    ~existential_move_only() {
        _si_details_.destroy();
    }

private:
    template <class, class, class>
    friend struct detail::existential_storage;
    template <class Owner>
    friend void* detail::get_object_pointer(Owner&) noexcept;
    template <class Owner>
    friend const void* detail::get_const_object_pointer(const Owner&) noexcept;
    template <class, auto, class>
    friend struct detail::function_proxy;
    template <class Owner, class Interface, class T>
    friend void detail::bind_member_proxies(Owner&) noexcept;
    template <class Owner, class Interface>
    friend void detail::copy_member_proxies(Owner&, const Owner&) noexcept;

    template <class T, class J, class A>
    friend bool is(const existential<J, A>&) noexcept;
    template <class T, class J, class A>
    friend bool is(const existential_move_only<J, A>&) noexcept;
    template <class T, class J>
    friend bool is(const existential_ref<J>&) noexcept;
    template <class T, class J, class A>
    friend std::remove_cvref_t<T>& get(existential<J, A>&);
    template <class T, class J, class A>
    friend const std::remove_cvref_t<T>& get(const existential<J, A>&);
    template <class T, class J, class A>
    friend std::remove_cvref_t<T>& get(existential_move_only<J, A>&);
    template <class T, class J, class A>
    friend const std::remove_cvref_t<T>& get(const existential_move_only<J, A>&);
    template <class T, class J>
    friend std::remove_cvref_t<T>& get(existential_ref<J>&);
    template <class T, class J>
    friend const std::remove_cvref_t<T>& get(const existential_ref<J>&);

    detail::existential_storage<I, existential_move_only<I, Allocator>, Allocator> _si_details_;
};

template <class I>
class existential_ref : public detail::generated_interface_members_t<
                            existential_ref<I>, std::remove_cv_t<I>> {
public:
    template <class T>
        requires ((!std::is_const_v<T> || std::is_const_v<I>) &&
                  satisfies<std::remove_cvref_t<T>, std::remove_cv_t<I>>)
    existential_ref(T& value) noexcept {
        _si_details_.bind(value);
        detail::bind_member_proxies<
            existential_ref, std::remove_cv_t<I>, std::remove_cvref_t<T>>(*this);
    }

private:
    template <class, auto, class>
    friend struct detail::function_proxy;
    template <class Owner>
    friend void* detail::get_object_pointer(Owner&) noexcept;
    template <class Owner>
    friend const void* detail::get_const_object_pointer(const Owner&) noexcept;
    template <class Owner, class Interface, class T>
    friend void detail::bind_member_proxies(Owner&) noexcept;

    template <class T, class J, class A>
    friend bool is(const existential<J, A>&) noexcept;
    template <class T, class J, class A>
    friend bool is(const existential_move_only<J, A>&) noexcept;
    template <class T, class J>
    friend bool is(const existential_ref<J>&) noexcept;
    template <class T, class J, class A>
    friend std::remove_cvref_t<T>& get(existential<J, A>&);
    template <class T, class J, class A>
    friend const std::remove_cvref_t<T>& get(const existential<J, A>&);
    template <class T, class J, class A>
    friend std::remove_cvref_t<T>& get(existential_move_only<J, A>&);
    template <class T, class J, class A>
    friend const std::remove_cvref_t<T>& get(const existential_move_only<J, A>&);
    template <class T, class J>
    friend std::remove_cvref_t<T>& get(existential_ref<J>&);
    template <class T, class J>
    friend const std::remove_cvref_t<T>& get(const existential_ref<J>&);

    detail::reference_storage<std::remove_cv_t<I>> _si_details_;
};

template <class T, class I, class Allocator>
bool is(const existential<I, Allocator>& object) noexcept {
    return detail::metadata_matches<T>(object._si_details_.metadata_ptr());
}

template <class T, class I, class Allocator>
bool is(const existential_move_only<I, Allocator>& object) noexcept {
    return detail::metadata_matches<T>(object._si_details_.metadata_ptr());
}

template <class T, class I>
bool is(const existential_ref<I>& object) noexcept {
    return detail::metadata_matches<T>(object._si_details_.metadata_ptr());
}

template <class T, class I, class Allocator>
std::remove_cvref_t<T>& get(existential<I, Allocator>& object) {
    using concrete = std::remove_cvref_t<T>;
    if (!is<T>(object)) {
        throw std::bad_cast{};
    }
    return *static_cast<concrete*>(detail::get_object_pointer(object));
}

template <class T, class I, class Allocator>
const std::remove_cvref_t<T>& get(const existential<I, Allocator>& object) {
    using concrete = std::remove_cvref_t<T>;
    if (!is<T>(object)) {
        throw std::bad_cast{};
    }
    return *static_cast<const concrete*>(detail::get_const_object_pointer(object));
}

template <class T, class I, class Allocator>
std::remove_cvref_t<T>& get(existential_move_only<I, Allocator>& object) {
    using concrete = std::remove_cvref_t<T>;
    if (!is<T>(object)) {
        throw std::bad_cast{};
    }
    return *static_cast<concrete*>(detail::get_object_pointer(object));
}

template <class T, class I, class Allocator>
const std::remove_cvref_t<T>& get(const existential_move_only<I, Allocator>& object) {
    using concrete = std::remove_cvref_t<T>;
    if (!is<T>(object)) {
        throw std::bad_cast{};
    }
    return *static_cast<const concrete*>(detail::get_const_object_pointer(object));
}

template <class T, class I>
    requires (!std::is_const_v<I>)
std::remove_cvref_t<T>& get(existential_ref<I>& object) {
    using concrete = std::remove_cvref_t<T>;
    if (!is<T>(object)) {
        throw std::bad_cast{};
    }
    return *static_cast<concrete*>(detail::get_object_pointer(object));
}

template <class T, class I>
const std::remove_cvref_t<T>& get(const existential_ref<I>& object) {
    using concrete = std::remove_cvref_t<T>;
    if (!is<T>(object)) {
        throw std::bad_cast{};
    }
    return *static_cast<const concrete*>(detail::get_const_object_pointer(object));
}

} // namespace si
