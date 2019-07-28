#ifndef META_FACTORY_HPP
#define META_FACTORY_HPP


#include <tuple>
#include <array>
#include <cassert>
#include <cstddef>
#include <utility>
#include <functional>
#include <type_traits>
#include "meta.hpp"


namespace meta {


/**
 * @cond TURN_OFF_DOXYGEN
 * Internal details not to be documented.
 */


namespace internal {


template<typename...>
struct function_helper;


template<typename Ret, typename... Args>
struct function_helper<Ret(Args...)> {
    using return_type = Ret;
    using args_type = std::tuple<Args...>;

    template<std::size_t Index>
    using arg_type = std::decay_t<std::tuple_element_t<Index, args_type>>;

    static constexpr auto size = sizeof...(Args);

    static auto arg(typename internal::func_node::size_type index) noexcept {
        return std::array<type_node *, sizeof...(Args)>{{type_info<Args>::resolve()...}}[index];
    }
};


template<typename Class, typename Ret, typename... Args, bool Const, bool Static>
struct function_helper<Class, Ret(Args...), std::bool_constant<Const>, std::bool_constant<Static>>: function_helper<Ret(Args...)> {
    using class_type = Class;
    static constexpr auto is_const = Const;
    static constexpr auto is_static = Static;
};


template<typename Ret, typename... Args, typename Class>
constexpr function_helper<Class, Ret(Args...), std::bool_constant<false>, std::bool_constant<false>>
to_function_helper(Ret(Class:: *)(Args...));


template<typename Ret, typename... Args, typename Class>
constexpr function_helper<Class, Ret(Args...), std::bool_constant<true>, std::bool_constant<false>>
to_function_helper(Ret(Class:: *)(Args...) const);


template<typename Ret, typename... Args>
constexpr function_helper<void, Ret(Args...), std::bool_constant<false>, std::bool_constant<true>>
to_function_helper(Ret(*)(Args...));


template<auto Func>
struct function_helper<std::integral_constant<decltype(Func), Func>>: decltype(to_function_helper(Func)) {};


template<typename Type, typename... Args, std::size_t... Indexes>
any construct(any * const args, std::index_sequence<Indexes...>) {
    [[maybe_unused]] auto direct = std::make_tuple((args+Indexes)->try_cast<std::remove_cv_t<std::remove_reference_t<Args>>>()...);
    any any{};

    if(((std::get<Indexes>(direct) || (args+Indexes)->convert<std::remove_cv_t<std::remove_reference_t<Args>>>()) && ...)) {
        any = Type{(std::get<Indexes>(direct) ? *std::get<Indexes>(direct) : (args+Indexes)->cast<std::remove_cv_t<std::remove_reference_t<Args>>>())...};
    }

    return any;
}


template<bool Const, typename Type, auto Data>
bool setter([[maybe_unused]] handle handle, [[maybe_unused]] any index, [[maybe_unused]] any value) {
    bool accepted = false;

    if constexpr(!Const) {
        if constexpr(std::is_function_v<std::remove_pointer_t<decltype(Data)>> || std::is_member_function_pointer_v<decltype(Data)>) {
            using helper_type = function_helper<std::integral_constant<decltype(Data), Data>>;
            using data_type = std::decay_t<std::tuple_element_t<!std::is_member_function_pointer_v<decltype(Data)>, typename helper_type::args_type>>;
            static_assert(std::is_invocable_v<decltype(Data), Type &, data_type>);
            auto *direct = value.try_cast<data_type>();
            auto *clazz = handle.data<Type>();

            if(clazz && (direct || value.convert<data_type>())) {
                std::invoke(Data, *clazz, direct ? *direct : value.cast<data_type>());
                accepted = true;
            }
        } else if constexpr(std::is_member_object_pointer_v<decltype(Data)>) {
            using data_type = std::remove_cv_t<std::remove_reference_t<decltype(std::declval<Type>().*Data)>>;
            static_assert(std::is_invocable_v<decltype(Data), Type>);
            auto *clazz = handle.data<Type>();

            if constexpr(std::is_array_v<data_type>) {
                using underlying_type = std::remove_extent_t<data_type>;
                auto *direct = value.try_cast<underlying_type>();
                auto *idx = index.try_cast<std::size_t>();

                if(clazz && idx && (direct || value.convert<underlying_type>())) {
                    std::invoke(Data, clazz)[*idx] = direct ? *direct : value.cast<underlying_type>();
                    accepted = true;
                }
            } else {
                auto *direct = value.try_cast<data_type>();

                if(clazz && (direct || value.convert<data_type>())) {
                    std::invoke(Data, clazz) = (direct ? *direct : value.cast<data_type>());
                    accepted = true;
                }
            }
        } else {
            static_assert(std::is_pointer_v<decltype(Data)>);
            using data_type = std::remove_cv_t<std::remove_reference_t<decltype(*Data)>>;

            if constexpr(std::is_array_v<data_type>) {
                using underlying_type = std::remove_extent_t<data_type>;
                auto *direct = value.try_cast<underlying_type>();
                auto *idx = index.try_cast<std::size_t>();

                if(idx && (direct || value.convert<underlying_type>())) {
                    (*Data)[*idx] = (direct ? *direct : value.cast<underlying_type>());
                    accepted = true;
                }
            } else {
                auto *direct = value.try_cast<data_type>();

                if(direct || value.convert<data_type>()) {
                    *Data = (direct ? *direct : value.cast<data_type>());
                    accepted = true;
                }
            }
        }
    }

    return accepted;
}


template<typename Type, auto Data>
any getter([[maybe_unused]] handle handle, [[maybe_unused]] any index) {
    if constexpr(std::is_function_v<std::remove_pointer_t<decltype(Data)>> || std::is_member_function_pointer_v<decltype(Data)>) {
       static_assert(std::is_invocable_v<decltype(Data), Type &>);
        auto *clazz = handle.data<Type>();
        return clazz ? std::invoke(Data, *clazz) : any{};
    } else if constexpr(std::is_member_object_pointer_v<decltype(Data)>) {
        using data_type = std::remove_cv_t<std::remove_reference_t<decltype(std::declval<Type>().*Data)>>;
        static_assert(std::is_invocable_v<decltype(Data), Type *>);
        auto *clazz = handle.data<Type>();

        if constexpr(std::is_array_v<data_type>) {
            auto *idx = index.try_cast<std::size_t>();
            return (clazz && idx) ? std::invoke(Data, clazz)[*idx] : any{};
        } else {
            return clazz ? std::invoke(Data, clazz) : any{};
        }
    } else {
        static_assert(std::is_pointer_v<decltype(Data)>);

        if constexpr(std::is_array_v<std::remove_pointer_t<decltype(Data)>>) {
            auto *idx = index.try_cast<std::size_t>();
            return idx ? (*Data)[*idx] : any{};
        } else {
            return *Data;
        }
    }
}


template<typename Type, auto Func, std::size_t... Indexes>
std::enable_if_t<std::is_function_v<std::remove_pointer_t<decltype(Func)>>, any>
invoke(handle, any *args, std::index_sequence<Indexes...>) {
    using helper_type = function_helper<std::integral_constant<decltype(Func), Func>>;
    [[maybe_unused]] auto direct = std::make_tuple((args+Indexes)->try_cast<typename helper_type::template arg_type<Indexes>>()...);
    any any{};

    if(((std::get<Indexes>(direct) || (args+Indexes)->convert<typename helper_type::template arg_type<Indexes>>()) && ...)) {
        if constexpr(std::is_void_v<typename helper_type::return_type>) {
            std::invoke(Func, (std::get<Indexes>(direct) ? *std::get<Indexes>(direct) : (args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>())...);
            any.emplace<void>();
        } else {
            any = std::invoke(Func, (std::get<Indexes>(direct) ? *std::get<Indexes>(direct) : (args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>())...);
        }
    }

    return any;
}


template<typename Type, auto Member, std::size_t... Indexes>
std::enable_if_t<std::is_member_function_pointer_v<decltype(Member)>, any>
invoke(handle handle, any *args, std::index_sequence<Indexes...>) {
    using helper_type = function_helper<std::integral_constant<decltype(Member), Member>>;
    static_assert(std::is_base_of_v<typename helper_type::class_type, Type>);
    [[maybe_unused]] auto direct = std::make_tuple((args+Indexes)->try_cast<typename helper_type::template arg_type<Indexes>>()...);
    auto *clazz = handle.data<Type>();
    any any{};

    if(clazz && ((std::get<Indexes>(direct) || (args+Indexes)->convert<typename helper_type::template arg_type<Indexes>>()) && ...)) {
        if constexpr(std::is_void_v<typename helper_type::return_type>) {
            std::invoke(Member, clazz, (std::get<Indexes>(direct) ? *std::get<Indexes>(direct) : (args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>())...);
            any.emplace<void>();
        } else {
            any = std::invoke(Member, clazz, (std::get<Indexes>(direct) ? *std::get<Indexes>(direct) : (args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>())...);
        }
    }

    return any;
}


}


/**
 * Internal details not to be documented.
 * @endcond TURN_OFF_DOXYGEN
 */


/**
 * @brief A meta factory to be used for reflection purposes.
 *
 * A meta factory is an utility class used to reflect types, data and functions
 * of all sorts. This class ensures that the underlying web of types is built
 * correctly and performs some checks in debug mode to ensure that there are no
 * subtle errors at runtime.
 *
 * @tparam Type Reflected type for which the factory was created.
 */
template<typename Type>
class factory {
    static_assert(std::is_object_v<Type> && !(std::is_const_v<Type> || std::is_volatile_v<Type>));

    template<typename Node>
    bool duplicate(const std::size_t id, const Node *node) noexcept {
        return node ? node->id == id || duplicate(id, node->next) : false;
    }

    bool duplicate(const any &key, const internal::prop_node *node) noexcept {
        return node ? node->key() == key || duplicate(key, node->next) : false;
    }

    template<typename>
    internal::prop_node * properties() {
        return nullptr;
    }

    template<typename Owner, typename Property, typename... Other>
    internal::prop_node * properties(Property &&property, Other &&... other) {
        static std::remove_cv_t<std::remove_reference_t<Property>> prop{};

        static internal::prop_node node{
            nullptr,
            []() -> any {
                return std::get<0>(prop);
            },
            []() -> any {
                return std::get<1>(prop);
            },
            []() noexcept -> meta::prop {
                return &node;
            }
        };

        prop = std::forward<Property>(property);
        node.next = properties<Owner>(std::forward<Other>(other)...);
        assert(!duplicate(any{std::get<0>(prop)}, node.next));
        return &node;
    }

    void unregister_prop(internal::prop_node **prop) {
        while(*prop) {
            auto *node = *prop;
            *prop = node->next;
            node->next = nullptr;
        }
    }

    void unregister_dtor() {
        if(auto node = internal::type_info<Type>::type->dtor; node) {
            internal::type_info<Type>::type->dtor = nullptr;
            *node->underlying = nullptr;
        }
    }

    template<auto Member>
    auto unregister_all(int)
    -> decltype((internal::type_info<Type>::type->*Member)->prop, void()) {
        while(internal::type_info<Type>::type->*Member) {
            auto node = internal::type_info<Type>::type->*Member;
            internal::type_info<Type>::type->*Member = node->next;
            unregister_prop(&node->prop);
            node->next = nullptr;
            *node->underlying = nullptr;
        }
    }

    template<auto Member>
    void unregister_all(char) {
        while(internal::type_info<Type>::type->*Member) {
            auto node = internal::type_info<Type>::type->*Member;
            internal::type_info<Type>::type->*Member = node->next;
            node->next = nullptr;
            *node->underlying = nullptr;
        }
    }

    factory() noexcept = default;

public:
    /**
     * @brief Extends a meta type by assigning it an identifier and properties.
     * @tparam Property Types of properties to assign to the meta type.
     * @param name Unique identifier.
     * @param property Properties to assign to the meta type.
     * @return A meta factory for the parent type.
     */
    template<typename... Property>
    factory type(const char *name, Property &&... property) noexcept {
        assert(!internal::type_info<Type>::type);
        auto *node = internal::type_info<Type>::resolve();
        node->name = name;
        node->id = std::hash<std::string>{}(name);;
        node->next = internal::type_info<>::type;
        node->prop = properties<Type>(std::forward<Property>(property)...);
        assert(!duplicate(node->id, node->next));
        internal::type_info<Type>::type = node;
        internal::type_info<>::type = node;

        return *this;
    }

    /**
     * @brief Assigns a meta base to a meta type.
     *
     * A reflected base class must be a real base class of the reflected type.
     *
     * @tparam Base Type of the base class to assign to the meta type.
     * @return A meta factory for the parent type.
     */
    template<typename Base>
    factory base() noexcept {
        static_assert(std::is_base_of_v<Base, Type>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::base_node node{
            &internal::type_info<Type>::template base<Base>,
            type,
            nullptr,
            &internal::type_info<Base>::resolve,
            [](void *instance) noexcept -> void * {
                return static_cast<Base *>(static_cast<Type *>(instance));
            },
            []() noexcept -> meta::base {
                return &node;
            }
        };

        node.next = type->base;
        assert((!internal::type_info<Type>::template base<Base>));
        internal::type_info<Type>::template base<Base> = &node;
        type->base = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta conversion function to a meta type.
     *
     * The given type must be such that an instance of the reflected type can be
     * converted to it.
     *
     * @tparam To Type of the conversion function to assign to the meta type.
     * @return A meta factory for the parent type.
     */
    template<typename To>
    factory conv() noexcept {
        static_assert(std::is_convertible_v<Type, To>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::conv_node node{
            &internal::type_info<Type>::template conv<To>,
            type,
            nullptr,
            &internal::type_info<To>::resolve,
            [](const void *instance) -> any {
                return static_cast<To>(*static_cast<const Type *>(instance));
            },
            []() noexcept -> meta::conv {
                return &node;
            }
        };

        node.next = type->conv;
        assert((!internal::type_info<Type>::template conv<To>));
        internal::type_info<Type>::template conv<To> = &node;
        type->conv = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta conversion function to a meta type.
     *
     * Conversion functions can be either free functions or member
     * functions.<br/>
     * In case of free functions, they must accept a const reference to an
     * instance of the parent type as an argument. In case of member functions,
     * they should have no arguments at all.
     *
     * @tparam Candidate The actual function to use for the conversion.
     * @return A meta factory for the parent type.
     */
    template<auto Candidate>
    factory conv() noexcept {
        using conv_type = std::invoke_result_t<decltype(Candidate), Type &>;
        auto * const type = internal::type_info<Type>::resolve();

        static internal::conv_node node{
            &internal::type_info<Type>::template conv<conv_type>,
            type,
            nullptr,
            &internal::type_info<conv_type>::resolve,
            [](const void *instance) -> any {
                return std::invoke(Candidate, *static_cast<const Type *>(instance));
            },
            []() noexcept -> meta::conv {
                return &node;
            }
        };

        node.next = type->conv;
        assert((!internal::type_info<Type>::template conv<conv_type>));
        internal::type_info<Type>::template conv<conv_type> = &node;
        type->conv = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta constructor to a meta type.
     *
     * Free functions can be assigned to meta types in the role of constructors.
     * All that is required is that they return an instance of the underlying
     * type.<br/>
     * From a client's point of view, nothing changes if a constructor of a meta
     * type is a built-in one or a free function.
     *
     * @tparam Func The actual function to use as a constructor.
     * @tparam Property Types of properties to assign to the meta data.
     * @param property Properties to assign to the meta data.
     * @return A meta factory for the parent type.
     */
    template<auto Func, typename... Property>
    factory ctor(Property &&... property) noexcept {
        using helper_type = internal::function_helper<std::integral_constant<decltype(Func), Func>>;
        static_assert(std::is_same_v<typename helper_type::return_type, Type>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::ctor_node node{
            &internal::type_info<Type>::template ctor<typename helper_type::args_type>,
            type,
            nullptr,
            nullptr,
            helper_type::size,
            &helper_type::arg,
            [](any * const any) {
                return internal::invoke<Type, Func>({}, any, std::make_index_sequence<helper_type::size>{});
            },
            []() noexcept -> meta::ctor {
                return &node;
            }
        };

        node.next = type->ctor;
        node.prop = properties<typename helper_type::args_type>(std::forward<Property>(property)...);
        assert((!internal::type_info<Type>::template ctor<typename helper_type::args_type>));
        internal::type_info<Type>::template ctor<typename helper_type::args_type> = &node;
        type->ctor = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta constructor to a meta type.
     *
     * A meta constructor is uniquely identified by the types of its arguments
     * and is such that there exists an actual constructor of the underlying
     * type that can be invoked with parameters whose types are those given.
     *
     * @tparam Args Types of arguments to use to construct an instance.
     * @tparam Property Types of properties to assign to the meta data.
     * @param property Properties to assign to the meta data.
     * @return A meta factory for the parent type.
     */
    template<typename... Args, typename... Property>
    factory ctor(Property &&... property) noexcept {
        using helper_type = internal::function_helper<Type(Args...)>;
        auto * const type = internal::type_info<Type>::resolve();

        static internal::ctor_node node{
            &internal::type_info<Type>::template ctor<typename helper_type::args_type>,
            type,
            nullptr,
            nullptr,
            helper_type::size,
            &helper_type::arg,
            [](any * const any) {
                return internal::construct<Type, Args...>(any, std::make_index_sequence<helper_type::size>{});
            },
            []() noexcept -> meta::ctor {
                return &node;
            }
        };

        node.next = type->ctor;
        node.prop = properties<typename helper_type::args_type>(std::forward<Property>(property)...);
        assert((!internal::type_info<Type>::template ctor<typename helper_type::args_type>));
        internal::type_info<Type>::template ctor<typename helper_type::args_type> = &node;
        type->ctor = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta destructor to a meta type.
     *
     * Free functions can be assigned to meta types in the role of destructors.
     * The signature of the function should identical to the following:
     *
     * @code{.cpp}
     * void(Type *);
     * @endcode
     *
     * From a client's point of view, nothing changes if the destructor of a
     * meta type is the default one or a custom one.
     *
     * @tparam Func The actual function to use as a destructor.
     * @return A meta factory for the parent type.
     */
    template<auto *Func>
    factory dtor() noexcept {
        static_assert(std::is_invocable_v<decltype(Func), Type *>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::dtor_node node{
            &internal::type_info<Type>::template dtor<Func>,
            type,
            [](handle handle) {
                return handle.type() == internal::type_info<Type>::resolve()->clazz()
                        ? ((*Func)(handle.data<Type>()), true)
                        : false;
            },
            []() noexcept -> meta::dtor {
                return &node;
            }
        };

        assert(!internal::type_info<Type>::type->dtor);
        assert((!internal::type_info<Type>::template dtor<Func>));
        internal::type_info<Type>::template dtor<Func> = &node;
        internal::type_info<Type>::type->dtor = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta data to a meta type.
     *
     * Both data members and static and global variables, as well as constants
     * of any kind, can be assigned to a meta type.<br/>
     * From a client's point of view, all the variables associated with the
     * reflected object will appear as if they were part of the type itself.
     *
     * @tparam Data The actual variable to attach to the meta type.
     * @tparam Property Types of properties to assign to the meta data.
     * @param name Unique identifier.
     * @param property Properties to assign to the meta data.
     * @return A meta factory for the parent type.
     */
    template<auto Data, typename... Property>
    factory data(const char *name, Property &&... property) noexcept {
        auto * const type = internal::type_info<Type>::resolve();
        internal::data_node *curr = nullptr;

        if constexpr(std::is_same_v<Type, decltype(Data)>) {
            static internal::data_node node{
                &internal::type_info<Type>::template data<Data>,
                nullptr,
                {},
                type,
                nullptr,
                nullptr,
                true,
                true,
                &internal::type_info<Type>::resolve,
                [](handle, any, any) { return false; },
                [](handle, any) -> any { return Data; },
                []() noexcept -> meta::data {
                    return &node;
                }
            };

            node.prop = properties<std::integral_constant<Type, Data>>(std::forward<Property>(property)...);
            curr = &node;
        } else if constexpr(std::is_member_object_pointer_v<decltype(Data)>) {
            using data_type = std::remove_reference_t<decltype(std::declval<Type>().*Data)>;

            static internal::data_node node{
                &internal::type_info<Type>::template data<Data>,
                nullptr,
                {},
                type,
                nullptr,
                nullptr,
                std::is_const_v<data_type>,
                !std::is_member_object_pointer_v<decltype(Data)>,
                &internal::type_info<data_type>::resolve,
                &internal::setter<std::is_const_v<data_type>, Type, Data>,
                &internal::getter<Type, Data>,
                []() noexcept -> meta::data {
                    return &node;
                }
            };

            node.prop = properties<std::integral_constant<decltype(Data), Data>>(std::forward<Property>(property)...);
            curr = &node;
        } else {
            static_assert(std::is_pointer_v<decltype(Data)>);
            using data_type = std::remove_pointer_t<decltype(Data)>;

            static internal::data_node node{
                &internal::type_info<Type>::template data<Data>,
                nullptr,
                {},
                type,
                nullptr,
                nullptr,
                std::is_const_v<data_type>,
                !std::is_member_object_pointer_v<decltype(Data)>,
                &internal::type_info<data_type>::resolve,
                &internal::setter<std::is_const_v<data_type>, Type, Data>,
                &internal::getter<Type, Data>,
                []() noexcept -> meta::data {
                    return &node;
                }
            };

            node.prop = properties<std::integral_constant<decltype(Data), Data>>(std::forward<Property>(property)...);
            curr = &node;
        }

        curr->name = name;
        curr->id = std::hash<std::string>{}(name);
        curr->next = type->data;
        assert(!duplicate(curr->id, curr->next));
        assert((!internal::type_info<Type>::template data<Data>));
        internal::type_info<Type>::template data<Data> = curr;
        type->data = curr;

        return *this;
    }

    /**
     * @brief Assigns a meta data to a meta type by means of its setter and
     * getter.
     *
     * Setters and getters can be either free functions, member functions or a
     * mix of them.<br/>
     * In case of free functions, setters and getters must accept a reference to
     * an instance of the parent type as their first argument. A setter has then
     * an extra argument of a type convertible to that of the parameter to
     * set.<br/>
     * In case of member functions, getters have no arguments at all, while
     * setters has an argument of a type convertible to that of the parameter to
     * set.
     *
     * @tparam Setter The actual function to use as a setter.
     * @tparam Getter The actual function to use as a getter.
     * @tparam Property Types of properties to assign to the meta data.
     * @param name Unique identifier.
     * @param property Properties to assign to the meta data.
     * @return A meta factory for the parent type.
     */
    template<auto Setter, auto Getter, typename... Property>
    factory data(const char *name, Property &&... property) noexcept {
        using owner_type = std::tuple<std::integral_constant<decltype(Setter), Setter>, std::integral_constant<decltype(Getter), Getter>>;
        using underlying_type = std::invoke_result_t<decltype(Getter), Type &>;
        static_assert(std::is_invocable_v<decltype(Setter), Type &, underlying_type>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::data_node node{
            &internal::type_info<Type>::template data<Setter, Getter>,
            nullptr,
            {},
            type,
            nullptr,
            nullptr,
            false,
            false,
            &internal::type_info<underlying_type>::resolve,
            &internal::setter<false, Type, Setter>,
            &internal::getter<Type, Getter>,
            []() noexcept -> meta::data {
                return &node;
            }
        };

        node.name = name;
        node.id = std::hash<std::string>{}(name);
        node.next = type->data;
        node.prop = properties<owner_type>(std::forward<Property>(property)...);
        assert(!duplicate(node.id, node.next));
        assert((!internal::type_info<Type>::template data<Setter, Getter>));
        internal::type_info<Type>::template data<Setter, Getter> = &node;
        type->data = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta funcion to a meta type.
     *
     * Both member functions and free functions can be assigned to a meta
     * type.<br/>
     * From a client's point of view, all the functions associated with the
     * reflected object will appear as if they were part of the type itself.
     *
     * @tparam Func The actual function to attach to the meta type.
     * @tparam Property Types of properties to assign to the meta function.
     * @param name Unique identifier.
     * @param property Properties to assign to the meta function.
     * @return A meta factory for the parent type.
     */
    template<auto Func, typename... Property>
    factory func(const char *name, Property &&... property) noexcept {
        using owner_type = std::integral_constant<decltype(Func), Func>;
        using func_type = internal::function_helper<std::integral_constant<decltype(Func), Func>>;
        auto * const type = internal::type_info<Type>::resolve();

        static internal::func_node node{
            &internal::type_info<Type>::template func<Func>,
            nullptr,
            {},
            type,
            nullptr,
            nullptr,
            func_type::size,
            func_type::is_const,
            func_type::is_static,
            &internal::type_info<typename func_type::return_type>::resolve,
            &func_type::arg,
            [](handle handle, any *any) {
                return internal::invoke<Type, Func>(handle, any, std::make_index_sequence<func_type::size>{});
            },
            []() noexcept -> meta::func {
                return &node;
            }
        };

        node.name = name;
        node.id = std::hash<std::string>{}(name);
        node.next = type->func;
        node.prop = properties<owner_type>(std::forward<Property>(property)...);
        assert(!duplicate(node.id, node.next));
        assert((!internal::type_info<Type>::template func<Func>));
        internal::type_info<Type>::template func<Func> = &node;
        type->func = &node;

        return *this;
    }

    /**
     * @brief Unregisters a meta type and all its parts.
     *
     * This function unregisters a meta type and all its data members, member
     * functions and properties, as well as its constructors, destructors and
     * conversion functions if any.<br/>
     * Base classes aren't unregistered but the link between the two types is
     * removed.
     *
     * @return True if the meta type exists, false otherwise.
     */
    bool unregister() noexcept {
        const auto registered = internal::type_info<Type>::type;

        if(registered) {
            if(auto *curr = internal::type_info<>::type; curr == internal::type_info<Type>::type) {
                internal::type_info<>::type = internal::type_info<Type>::type->next;
            } else {
                while(curr && curr->next != internal::type_info<Type>::type) {
                    curr = curr->next;
                }

                if(curr) {
                    curr->next = internal::type_info<Type>::type->next;
                }
            }

            unregister_prop(&internal::type_info<Type>::type->prop);
            unregister_all<&internal::type_node::base>(0);
            unregister_all<&internal::type_node::conv>(0);
            unregister_all<&internal::type_node::ctor>(0);
            unregister_all<&internal::type_node::data>(0);
            unregister_all<&internal::type_node::func>(0);
            unregister_dtor();

            internal::type_info<Type>::type->name = nullptr;
            internal::type_info<Type>::type->next = nullptr;
            internal::type_info<Type>::type = nullptr;
        }

        return registered;
    }
};


/**
 * @brief Utility function to use for reflection.
 *
 * This is the point from which everything starts.<br/>
 * By invoking this function with a type that is not yet reflected, a meta type
 * is created to which it will be possible to attach data and functions through
 * a dedicated factory.
 *
 * @tparam Type Type to reflect.
 * @tparam Property Types of properties to assign to the reflected type.
 * @param name Unique identifier.
 * @param property Properties to assign to the reflected type.
 * @return A meta factory for the given type.
 */
template<typename Type, typename... Property>
inline factory<Type> reflect(const char *name, Property &&... property) noexcept {
    return factory<Type>{}.type(name, std::forward<Property>(property)...);
}


/**
 * @brief Utility function to use for reflection.
 *
 * This is the point from which everything starts.<br/>
 * By invoking this function with a type that is not yet reflected, a meta type
 * is created to which it will be possible to attach data and functions through
 * a dedicated factory.
 *
 * @tparam Type Type to reflect.
 * @return A meta factory for the given type.
 */
template<typename Type>
inline factory<Type> reflect() noexcept {
    return factory<Type>{};
}


/**
 * @brief Utility function to unregister a type.
 *
 * This function unregisters a type and all its data members, member functions
 * and properties, as well as its constructors, destructors and conversion
 * functions if any.<br/>
 * Base classes aren't unregistered but the link between the two types is
 * removed.
 *
 * @tparam Type Type to unregister.
 * @return True if the type to unregister exists, false otherwise.
 */
template<typename Type>
inline bool unregister() noexcept {
    return factory<Type>{}.unregister();
}


/**
 * @brief Returns the meta type associated with a given type.
 * @tparam Type Type to use to search for a meta type.
 * @return The meta type associated with the given type, if any.
 */
template<typename Type>
inline type resolve() noexcept {
    return internal::type_info<Type>::resolve()->clazz();
}


/**
 * @brief Returns the meta type associated with a given identifier.
 * @param name Unique identifier.
 * @return The meta type associated with the given identifier, if any.
 */
inline type resolve(const char *name) noexcept {
    const auto *curr = internal::find_if([id = std::hash<std::string>{}(name)](auto *node) {
        return node->id == id;
    }, internal::type_info<>::type);

    return curr ? curr->clazz() : type{};
}


/**
 * @brief Iterates all the reflected types.
 * @tparam Op Type of the function object to invoke.
 * @param op A valid function object.
 */
template<typename Op>
inline std::enable_if_t<std::is_invocable_v<Op, type>, void>
resolve(Op op) noexcept {
    internal::iterate([op = std::move(op)](auto *node) {
        op(node->clazz());
    }, internal::type_info<>::type);
}


}


#endif // META_FACTORY_HPP
