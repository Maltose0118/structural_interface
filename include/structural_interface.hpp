#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <meta>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace si {

template <class I>
class existential;

template <class I>
class existential_move_only;

template <class I>
class existential_ref;

inline constexpr std::size_t sbo_storage_size = 64;
inline constexpr std::size_t sbo_storage_alignment = alignof(std::max_align_t);

namespace detail {

using meta_info = std::meta::info;

template <class T>
using bare_t = std::remove_cvref_t<T>;

consteval auto access() noexcept {
    return std::meta::access_context::current();
}

consteval auto reflected_members(meta_info type) {
    return std::meta::reflect_constant_array(std::meta::members_of(type, access()));
}

consteval auto reflected_bases(meta_info type) {
    return std::meta::reflect_constant_array(std::meta::bases_of(type, access()));
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

template <class Ptr, class Owner>
struct rebind_member_pointer;

template <class M, class C, class Owner>
struct rebind_member_pointer<M C::*, Owner> {
    using type = M Owner::*;
};

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
concrete_member_pointer() {
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

#define SI_FUNCTION_PROXY(CV, REF, NOEXCEPT_SPEC, OBJECT_EXPR)                  \
    template <class Owner, auto InterfaceMember, class I, class R, class... Args> \
    struct function_proxy<Owner, InterfaceMember, R (I::*)(Args...) CV REF NOEXCEPT_SPEC> { \
        using erased_call = R (*)(void*, Args...) NOEXCEPT_SPEC;                \
                                                                                \
        void** object = nullptr;                                                \
        erased_call call = nullptr;                                             \
                                                                                \
        template <class T>                                                      \
        consteval static erased_call thunk_for() {                              \
            return +[](void* object, Args... args) NOEXCEPT_SPEC -> R {          \
                constexpr auto member = concrete_member_pointer<InterfaceMember, T>(); \
                if constexpr (std::is_void_v<R>) {                              \
                    std::invoke(member, OBJECT_EXPR(static_cast<T*>(object)), std::forward<Args>(args)...); \
                } else {                                                        \
                    return std::invoke(member, OBJECT_EXPR(static_cast<T*>(object)), std::forward<Args>(args)...); \
                }                                                               \
            };                                                                  \
        }                                                                       \
                                                                                \
        template <class T>                                                      \
        void bind(Owner& new_owner) noexcept {                                  \
            object = new_owner._si_object_slot_();                              \
            call = thunk_for<T>();                                              \
        }                                                                       \
                                                                                \
        void rebind_from(const function_proxy& other, Owner& new_owner) noexcept { \
            object = new_owner._si_object_slot_();                              \
            call = other.call;                                                  \
        }                                                                       \
                                                                                \
        R operator()(Args... args) CV REF NOEXCEPT_SPEC {                       \
            if constexpr (std::is_void_v<R>) {                                  \
                call(*object, std::forward<Args>(args)...);                     \
            } else {                                                            \
                return call(*object, std::forward<Args>(args)...);              \
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

template <class Owner, class I>
struct generated_function_members {
    struct type;

    consteval {
        std::vector<meta_info> specs;
        template for (constexpr auto member : [: reflected_members(^^I) :]) {
            if constexpr (std::meta::is_function(member) &&
                          !std::meta::is_special_member_function(member) &&
                          !std::meta::is_constructor(member) &&
                          !std::meta::is_destructor(member)) {
                using proxy_type = function_proxy<Owner, member>;
                specs.push_back(std::meta::data_member_spec(
                    ^^proxy_type,
                    {.name = std::string(std::meta::identifier_of(member))}));
            }
        }
        std::meta::define_aggregate(^^type, specs);
    }
};

template <class Owner, class I>
using generated_function_members_t = typename generated_function_members<Owner, I>::type;

template <class Generated, auto InterfaceMember, class Proxy>
consteval auto generated_proxy_member() {
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

template <class Owner, class I, class T>
void bind_function_members(Owner& owner) noexcept {
    template for (constexpr auto member : [: reflected_members(^^I) :]) {
        if constexpr (std::meta::is_function(member) &&
                      !std::meta::is_special_member_function(member) &&
                      !std::meta::is_constructor(member) &&
                      !std::meta::is_destructor(member)) {
            using proxy_type = function_proxy<Owner, member>;
            constexpr auto base_member =
                generated_proxy_member<generated_function_members_t<Owner, I>, member, proxy_type>();
            (owner.*base_member).template bind<T>(owner);
        }
    }
}

template <class Owner, class I>
void copy_function_members(Owner& owner, const Owner& other) noexcept {
    template for (constexpr auto member : [: reflected_members(^^I) :]) {
        if constexpr (std::meta::is_function(member) &&
                      !std::meta::is_special_member_function(member) &&
                      !std::meta::is_constructor(member) &&
                      !std::meta::is_destructor(member)) {
            using proxy_type = function_proxy<Owner, member>;
            constexpr auto base_member =
                generated_proxy_member<generated_function_members_t<Owner, I>, member, proxy_type>();
            (owner.*base_member).rebind_from(other.*base_member, owner);
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

template <auto InterfaceMember, class Concrete>
consteval auto matching_concrete_member() {
    using interface_pointer = member_pointer_type_t<InterfaceMember>;
    using concrete_pointer = rebind_member_pointer_t<interface_pointer, Concrete>;

    template for (constexpr auto concrete_member : [: reflected_members(^^Concrete) :]) {
        if constexpr (is_relevant_member(concrete_member) &&
                      identifier_equal(InterfaceMember, concrete_member) &&
                      member_shapes_equal(InterfaceMember, concrete_member)) {
            if constexpr (requires { std::meta::extract<concrete_pointer>(concrete_member); }) {
                return concrete_member;
            }
        }
    }

    return meta_info{};
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

template <class T>
inline constexpr bool fits_sbo =
    sizeof(T) <= sbo_storage_size &&
    alignof(T) <= sbo_storage_alignment &&
    alignof(T) <= alignof(std::max_align_t);

template <class I>
struct interface_metadata {
    void (*destroy)(void*) noexcept;
    void (*copy_construct)(void*, void**, const void*);
    void (*move_construct)(void*, void**, void*);
    const void* type_token;
};

template <class T>
void destroy_small(void* object) noexcept {
    std::destroy_at(static_cast<T*>(object));
}

template <class T>
void destroy_large(void* object) noexcept {
    delete static_cast<T*>(object);
}

template <class T>
void copy_construct_small(void* storage, void** object, const void* source) {
    ::new (storage) T(*static_cast<const T*>(source));
    *object = storage;
}

template <class T>
void copy_construct_large(void*, void** object, const void* source) {
    *object = new T(*static_cast<const T*>(source));
}

template <class T>
consteval auto copy_construct_fn() {
    if constexpr (std::copy_constructible<T>) {
        if constexpr (fits_sbo<T>) {
            return &copy_construct_small<T>;
        } else {
            return &copy_construct_large<T>;
        }
    } else {
        return static_cast<void (*)(void*, void**, const void*)>(nullptr);
    }
}

template <class T>
void move_construct_small(void* storage, void** object, void* source) {
    ::new (storage) T(std::move(*static_cast<T*>(source)));
    *object = storage;
}

template <class T>
void move_construct_large(void*, void** object, void* source) {
    *object = source;
}

template <class I, class T>
inline const interface_metadata<I> metadata_for = {
    .destroy = fits_sbo<T> ? &destroy_small<T> : &destroy_large<T>,
    .copy_construct = copy_construct_fn<T>(),
    .move_construct = fits_sbo<T> ? &move_construct_small<T> : &move_construct_large<T>,
    .type_token = &metadata_for<I, T>,
};

template <class I>
struct _si_existential_details {
    alignas(sbo_storage_alignment) std::byte storage[sbo_storage_size]{};
    void* object = nullptr;
    const interface_metadata<I>* metadata = nullptr;

    bool has_object() const noexcept {
        return object != nullptr;
    }

    void destroy() noexcept {
        if (object != nullptr) {
            metadata->destroy(object);
            object = nullptr;
            metadata = nullptr;
        }
    }

    template <class T, class... Args>
    void emplace(Args&&... args) {
        using concrete = std::remove_cvref_t<T>;
        if constexpr (fits_sbo<concrete>) {
            ::new (storage) concrete(std::forward<Args>(args)...);
            object = storage;
        } else {
            object = new concrete(std::forward<Args>(args)...);
        }
        metadata = &metadata_for<I, concrete>;
    }

    void copy_from(const _si_existential_details& other) {
        other.metadata->copy_construct(storage, &object, other.object);
        metadata = other.metadata;
    }

    void move_from(_si_existential_details& other) {
        metadata = other.metadata;
        if (other.object == other.storage) {
            other.metadata->move_construct(storage, &object, other.object);
            other.destroy();
        } else {
            object = other.object;
            other.object = nullptr;
            other.metadata = nullptr;
        }
    }
};

template <class I>
struct _si_ref_details {
    void* object = nullptr;
    const interface_metadata<I>* metadata = nullptr;

    template <class T>
    void bind(T& value) noexcept {
        object = std::addressof(value);
        metadata = &metadata_for<I, std::remove_cvref_t<T>>;
    }
};

} // namespace detail

template <class T, class I>
concept satisfies =
    detail::has_interface_requirements<I>() &&
    (detail::satisfies_interface_members<I, T>() ||
     std::same_as<std::remove_cvref_t<T>, existential<I>> ||
     std::same_as<std::remove_cvref_t<T>, existential_move_only<I>> ||
     std::same_as<std::remove_cvref_t<T>, existential_ref<I>>);

template <class I>
class existential : public detail::generated_function_members_t<existential<I>, I> {
public:
    template <class T>
        requires (!std::same_as<std::remove_cvref_t<T>, existential>) &&
                 satisfies<std::remove_cvref_t<T>, I> &&
                 std::copy_constructible<std::remove_cvref_t<T>> &&
                 std::move_constructible<std::remove_cvref_t<T>>
    existential(T&& value) {
        emplace<std::remove_cvref_t<T>>(std::forward<T>(value));
    }

    existential(const existential& other) {
        _si_details_.copy_from(other._si_details_);
        detail::copy_function_members<existential, I>(*this, other);
    }

    existential(existential&& other) noexcept {
        move_from(std::move(other));
    }

    existential& operator=(const existential& other) {
        if (this == &other) {
            return *this;
        }
        _si_details_.destroy();
        _si_details_.copy_from(other._si_details_);
        detail::copy_function_members<existential, I>(*this, other);
        return *this;
    }

    existential& operator=(existential&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        _si_details_.destroy();
        move_from(std::move(other));
        return *this;
    }

    ~existential() {
        _si_details_.destroy();
    }

private:
    template <class, auto, class>
    friend struct detail::function_proxy;

    void** _si_object_slot_() noexcept {
        return &_si_details_.object;
    }

    template <class T, class... Args>
    void emplace(Args&&... args) {
        using concrete = std::remove_cvref_t<T>;
        _si_details_.template emplace<concrete>(std::forward<Args>(args)...);
        detail::bind_function_members<existential, I, concrete>(*this);
    }

    void move_from(existential&& other) noexcept {
        if (!other._si_details_.has_object()) {
            return;
        }
        _si_details_.move_from(other._si_details_);
        detail::copy_function_members<existential, I>(*this, other);
    }

    detail::_si_existential_details<I> _si_details_;
};

template <class I>
class existential_move_only : public detail::generated_function_members_t<existential_move_only<I>, I> {
public:
    template <class T>
        requires (!std::same_as<std::remove_cvref_t<T>, existential_move_only>) &&
                 satisfies<std::remove_cvref_t<T>, I> &&
                 std::move_constructible<std::remove_cvref_t<T>>
    existential_move_only(T&& value) {
        emplace<std::remove_cvref_t<T>>(std::forward<T>(value));
    }

    existential_move_only(const existential_move_only&) = delete;
    existential_move_only& operator=(const existential_move_only&) = delete;

    existential_move_only(existential_move_only&& other) noexcept {
        move_from(std::move(other));
    }

    existential_move_only& operator=(existential_move_only&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        _si_details_.destroy();
        move_from(std::move(other));
        return *this;
    }

    ~existential_move_only() {
        _si_details_.destroy();
    }

private:
    template <class, auto, class>
    friend struct detail::function_proxy;

    void** _si_object_slot_() noexcept {
        return &_si_details_.object;
    }

    template <class T, class... Args>
    void emplace(Args&&... args) {
        using concrete = std::remove_cvref_t<T>;
        _si_details_.template emplace<concrete>(std::forward<Args>(args)...);
        detail::bind_function_members<existential_move_only, I, concrete>(*this);
    }

    void move_from(existential_move_only&& other) noexcept {
        if (!other._si_details_.has_object()) {
            return;
        }
        _si_details_.move_from(other._si_details_);
        detail::copy_function_members<existential_move_only, I>(*this, other);
    }

    detail::_si_existential_details<I> _si_details_;
};

template <class I>
class existential_ref : public detail::generated_function_members_t<existential_ref<I>, I> {
public:
    template <class T>
        requires satisfies<std::remove_cvref_t<T>, I>
    existential_ref(T& value) noexcept {
        _si_details_.bind(value);
        detail::bind_function_members<existential_ref, I, std::remove_cvref_t<T>>(*this);
    }

private:
    template <class, auto, class>
    friend struct detail::function_proxy;

    void** _si_object_slot_() noexcept {
        return &_si_details_.object;
    }

    detail::_si_ref_details<I> _si_details_;
};

} // namespace si
