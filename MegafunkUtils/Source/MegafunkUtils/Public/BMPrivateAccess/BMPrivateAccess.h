/*
* MIT License

Copyright (c) 2025 BlueMan

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include <type_traits>
#include <tuple>

#if defined(__cpp_concepts)
#define PRIVATE_CONCEPTS_DEFINED 1
#else
#define PRIVATE_CONCEPTS_DEFINED 0
#endif

#define PRIVATE_CPP20_FEATURE_SET PRIVATE_CONCEPTS_DEFINED && __cpp_concepts >= 201907L

#define PRIVATE_FORCE_TOKEN(...) __VA_ARGS__
#define PRIVATE_CAT_PASTE(a, b) a##b
#define PRIVATE_CAT(a, b) PRIVATE_CAT_PASTE(a, b)

#define PRIVATE_IMPL_VALUE_IF1(True, False, ...) True(__VA_ARGS__)
#define PRIVATE_IMPL_VALUE_IF0(True, False, ...) False(__VA_ARGS__)
#define PRIVATE_IMPL_VALUE_IF(True, False, ...) False(__VA_ARGS__)
#define PRIVATE_VALUE_IF(condition, True, False, ...) PRIVATE_IMPL_VALUE_IF##condition(True, False, __VA_ARGS__)

#define PRIVATE_IMPL_VALUE_IF1_NOCALL(True, False) True
#define PRIVATE_IMPL_VALUE_IF0_NOCALL(True, False) False
#define PRIVATE_IMPL_VALUE_IF_NOCALL(True, False) False
#define PRIVATE_VALUE_IF_NOCALL(condition, True, False) PRIVATE_IMPL_VALUE_IF##condition##_NOCALL(True, False)

#if PRIVATE_CPP20_FEATURE_SET
    #define PRIVATE_CONSTEVAL consteval
    #define PRIVATE_VA_ASSERT(...)
#else
    #define PRIVATE_CONSTEVAL constexpr
    #define PRIVATE_VA_ASSERT(Msg, ...) static_assert(BMPrivateAccess::TArgCounter<__VA_ARGS__>::Value == 0, Msg);
#endif



#define PRIVATE_ACCESS_PROLOGUE(TagName, ...)\
    struct TagName

#define PRIVATE_ACCESS_TEMPLATE_PROLOGUE(TagName, ...)\
    template<typename... TArgs>\
    struct TagName;\
\
    template<>\
    struct TagName<__VA_ARGS__>


#define PRIVATE_MEMBER_EPILOGUE(ClassName, MemberName, Tag, Class, ...)\
    template struct TAccessPrivateMember<Tag, &Class::MemberName>;\

#define PRIVATE_MEMBER_CALLAPI(NamespaceName, MemberName, TagName, Condition)\
    namespace NamespaceName##_Private\
    {\
        template< Condition typename... TArgs>\
        static auto& Get_##MemberName(TArgs&&... Args)\
        {\
            return AccessPrivate(TagName{}, std::forward<TArgs>(Args)...);\
        }\
    }\

#define PRIVATE_MEMBER_CALLAPI_CONDITION(ClassName, ...) typename T, std::enable_if_t<std::is_same_v<T, ClassName<__VA_ARGS__>>, int> = 0, 
#define PRIVATE_MEMBER_CALLAPI_NO_CONDITION(ClassName, ...) int = 0, 

#define PRIVATE_DEFINE_TYPE(A, ...) A;
#define PRIVATE_DEFINE_TYPE_TMPLT(A, ...) A<__VA_ARGS__>;

#define DEFINE_PRIVATE_MEMBER_ACCESSOR_Impl(ClassName, MemberName, Type, Prologue, DefineType, CallAPICondition, ...) \
namespace BMPrivateAccess\
{\
    Prologue(ClassName##MemberName##Tag, __VA_ARGS__)\
    {\
        template<typename... TArgs>\
        friend auto& AccessPrivate(ClassName##MemberName##Tag, TArgs&&... Args);\
    };\
    \
    using PRIVATE_CAT(TTag##ClassName##MemberName, __LINE__) = DefineType(ClassName##MemberName##Tag, __VA_ARGS__);\
    using PRIVATE_CAT(TClassType##ClassName, __LINE__) = DefineType(ClassName, __VA_ARGS__);\
    PRIVATE_MEMBER_EPILOGUE(ClassName, MemberName, PRIVATE_CAT(TTag##ClassName##MemberName, __LINE__), PRIVATE_CAT(TClassType##ClassName, __LINE__), __VA_ARGS__)\
}\
    PRIVATE_MEMBER_CALLAPI(ClassName, MemberName, BMPrivateAccess::PRIVATE_CAT(TTag##ClassName##MemberName, __LINE__),\
    CallAPICondition(ClassName, __VA_ARGS__))



#define CREATE_OVERLOAD_HELPER(ClassName, MemberName, ReturnType, ...)\
    static_assert(BMPrivateAccess::TArgCounter<__VA_ARGS__>::Value > 0, "Overload helper requires at least one argument to differentiate overloads, use void if overloaded by cv-qualifiers only");\
\
    static PRIVATE_CONSTEVAL auto* PRIVATE_CAT(_GetType##ClassName##MemberName,__LINE__)(ReturnType(*)(__VA_ARGS__))\
    {\
        using TFunctionPtr = ReturnType(*)(__VA_ARGS__);\
        TFunctionPtr* FunctionPtr = nullptr;\
        return FunctionPtr;\
    }\
\
    static PRIVATE_CONSTEVAL auto* PRIVATE_CAT(_GetType##ClassName##MemberName,__LINE__)(ReturnType(ClassName::*)(__VA_ARGS__))\
    {\
        using TFunctionPtr = ReturnType(ClassName::*)(__VA_ARGS__);\
        TFunctionPtr* FunctionPtr = nullptr;\
        return FunctionPtr;\
    }\
\
    static PRIVATE_CONSTEVAL auto* PRIVATE_CAT(_GetType##ClassName##MemberName,__LINE__)(ReturnType(ClassName::*)(__VA_ARGS__) const)\
    {\
        using TFunctionPtr = ReturnType(ClassName::*)(__VA_ARGS__) const;\
        TFunctionPtr* FunctionPtr = nullptr;\
        return FunctionPtr;\
    }\

#define CREATE_NO_OVERLOAD_HELPER(...)

#define CREATE_CALL_FUNCTION(ClassName, MemberName, ReturnType, TagName, ...)\
    template<typename... TArgs>\
    static ReturnType Call_##MemberName(TArgs&&... Args)\
    {\
        return CallPrivate(TagName{}, std::forward<TArgs>(Args)...);\
    }\

#define CREATE_CALL_FUNCTION_TMPLT(ClassName, MemberName, ReturnType, TagName, ...)\
    template<typename... TArgs>\
    struct T##MemberName;\
\
    template<>\
    struct T##MemberName<__VA_ARGS__>\
    {\
        template<typename... TArgs>\
        static ReturnType Call(TArgs&&... Args)\
        {\
            return CallPrivate(TagName{}, std::forward<TArgs>(Args)...);\
        }\
    };

#define CREATE_CALL_FUNCTION_OVERLOAD(ClassName, MemberName, ReturnType, TagName, ...)\
    static constexpr bool PRIVATE_CAT(_bConstFunction_##MemberName##_, __LINE__) = IsConstFunction(TagName{});\
    \
    template<typename... TArgs>\
    static auto Call_##MemberName(TArgs&&... Args) -> std::enable_if_t<BMPrivateAccess::TOverloadHelper<TArgs...>::template bSame<__VA_ARGS__>, ReturnType>\
    {\
        return CallPrivate(TagName{}, std::forward<TArgs>(Args)...);\
    }\
\
    template<typename... TArgs>\
    static auto Call_##MemberName(ClassName& Obj, TArgs&&... Args) -> std::enable_if_t<!PRIVATE_CAT(_bConstFunction_##MemberName##_, __LINE__) && BMPrivateAccess::TOverloadHelper<TArgs...>::template bSame<__VA_ARGS__>, ReturnType>\
    {\
        return CallPrivate(TagName{}, Obj, std::forward<TArgs>(Args)...);\
    }\
\
    template<typename... TArgs>\
    static auto Call_##MemberName(const ClassName& Obj, TArgs&&... Args) -> std::enable_if_t<PRIVATE_CAT(_bConstFunction_##MemberName##_, __LINE__) && BMPrivateAccess::TOverloadHelper<TArgs...>::template bSame<__VA_ARGS__>, ReturnType>\
    {\
        return CallPrivate(TagName{}, Obj, std::forward<TArgs>(Args)...);\
    }\
    

#define PRIVATE_RESOLVE_OVERLOAD_FUNCTION_ADDRESS(ClassName, MemberName, HelperFunction, ...)\
    static_cast<std::remove_pointer_t<decltype(HelperFunction(&ClassName::MemberName))>>(&ClassName::MemberName)

#define PRIVATE_RESOLVE_FUNCTION_ADDRESS(ClassName, MemberName, ...)\
    &ClassName::MemberName

#define PRIVATE_RESOLVE_TMPLT_FUNCTION_ADDRESS(ClassName, MemberName, Misc, ...)\
    &ClassName::MemberName<__VA_ARGS__>

#define DEFINE_PRIVATE_FUNCTION_ACCESSOR_Impl(ClassName, MemberName, ReturnType, Prologue, DefineTypes, CreateOverloadHelper, ResolveFunctionAddress, CreateCallFunction, ...) \
namespace BMPrivateAccess\
{\
    Prologue(ClassName##MemberName##Tag, ReturnType, __VA_ARGS__)\
    {\
        template<typename... TArgs>\
        friend ReturnType CallPrivate(ClassName##MemberName##Tag, TArgs&&... Args);\
    };\
    using PRIVATE_CAT(TTag##ClassName##MemberName, __LINE__) = DefineTypes(ClassName##MemberName##Tag, ReturnType, __VA_ARGS__);\
\
    CreateOverloadHelper(ClassName, MemberName, ReturnType, __VA_ARGS__)\
    template struct TAccessPrivateFunction<PRIVATE_CAT(TTag##ClassName##MemberName, __LINE__), ClassName, ReturnType,\
    ResolveFunctionAddress(ClassName, MemberName, PRIVATE_CAT(_GetType##ClassName##MemberName,__LINE__), __VA_ARGS__)>;\
    \
    constexpr bool IsConstFunction(PRIVATE_CAT(TTag##ClassName##MemberName, __LINE__));\
}\
namespace ClassName##_Private\
{\
    CreateCallFunction(ClassName, MemberName, ReturnType, BMPrivateAccess::PRIVATE_CAT(TTag##ClassName##MemberName, __LINE__), __VA_ARGS__)\
}\


#define DEFINE_PRIVATE_TYPE_ACCESSOR(Type1, Type2) \
namespace BMPrivateAccess\
{\
    struct Type1##_##Type2##_PrivateAccessTag\
    {\
    };\
\
template struct TAccessPrivateType<Type1##_##Type2##_PrivateAccessTag, Type1::Type2>;\
PRIVATE_CONSTEVAL auto* ResolvePrivateType(BMPrivateAccess::Type1##_##Type2##_PrivateAccessTag);\
}\
namespace Type1##_Private\
{\
    using Type2 = std::remove_pointer_t<decltype(ResolvePrivateType(BMPrivateAccess::Type1##_##Type2##_PrivateAccessTag{}))>;\
}\

#define DEFINE_PRIVATE_MEMBER_ACCESSOR_TMPLT(ClassName, MemberName, Type, ...) DEFINE_PRIVATE_MEMBER_ACCESSOR_Impl(ClassName, MemberName, Type,\
    PRIVATE_ACCESS_TEMPLATE_PROLOGUE,\
    PRIVATE_DEFINE_TYPE_TMPLT,\
    PRIVATE_MEMBER_CALLAPI_CONDITION,\
    __VA_ARGS__)

#define DEFINE_PRIVATE_FUNCTION_ACCESSOR_OVERLOAD(ClassName, MemberName, ReturnType, ...) DEFINE_PRIVATE_FUNCTION_ACCESSOR_Impl(ClassName, MemberName, ReturnType,\
    PRIVATE_ACCESS_TEMPLATE_PROLOGUE,\
    PRIVATE_DEFINE_TYPE_TMPLT,\
    CREATE_OVERLOAD_HELPER,\
    PRIVATE_RESOLVE_OVERLOAD_FUNCTION_ADDRESS,\
    CREATE_CALL_FUNCTION_OVERLOAD,\
    __VA_ARGS__)

#define DEFINE_PRIVATE_FUNCTION_ACCESSOR_TMPLT(ClassName, MemberName, ReturnType, ...)\
    DEFINE_PRIVATE_FUNCTION_ACCESSOR_Impl(ClassName, MemberName, ReturnType,\
    PRIVATE_ACCESS_PROLOGUE,\
    PRIVATE_DEFINE_TYPE,\
    CREATE_NO_OVERLOAD_HELPER,\
    PRIVATE_RESOLVE_TMPLT_FUNCTION_ADDRESS,\
    CREATE_CALL_FUNCTION_TMPLT,\
    __VA_ARGS__)

#if PRIVATE_CPP20_FEATURE_SET
#define DEFINE_PRIVATE_MEMBER_ACCESSOR(ClassName, MemberName, Type, ...) DEFINE_PRIVATE_MEMBER_ACCESSOR_Impl(ClassName, MemberName, Type,\
    PRIVATE_VALUE_IF_NOCALL(__VA_OPT__(1), PRIVATE_ACCESS_TEMPLATE_PROLOGUE, PRIVATE_ACCESS_PROLOGUE),\
    PRIVATE_VALUE_IF_NOCALL(__VA_OPT__(1), PRIVATE_DEFINE_TYPE_TMPLT, PRIVATE_DEFINE_TYPE),\
    PRIVATE_VALUE_IF_NOCALL(__VA_OPT__(1), PRIVATE_MEMBER_CALLAPI_CONDITION, PRIVATE_MEMBER_CALLAPI_NO_CONDITION), __VA_ARGS__)

#define DEFINE_PRIVATE_FUNCTION_ACCESSOR(ClassName, MemberName, ReturnType, ...) DEFINE_PRIVATE_FUNCTION_ACCESSOR_Impl(ClassName, MemberName, ReturnType,\
    PRIVATE_VALUE_IF_NOCALL(__VA_OPT__(1), PRIVATE_ACCESS_TEMPLATE_PROLOGUE, PRIVATE_ACCESS_PROLOGUE),\
    PRIVATE_VALUE_IF_NOCALL(__VA_OPT__(1), PRIVATE_DEFINE_TYPE_TMPLT, PRIVATE_DEFINE_TYPE),\
    PRIVATE_VALUE_IF_NOCALL(__VA_OPT__(1), CREATE_OVERLOAD_HELPER, CREATE_NO_OVERLOAD_HELPER),\
    PRIVATE_VALUE_IF_NOCALL(__VA_OPT__(1), PRIVATE_RESOLVE_OVERLOAD_FUNCTION_ADDRESS, PRIVATE_RESOLVE_FUNCTION_ADDRESS),\
    PRIVATE_VALUE_IF_NOCALL(__VA_OPT__(1), CREATE_CALL_FUNCTION_OVERLOAD, CREATE_CALL_FUNCTION), __VA_ARGS__)

namespace BMPrivateAccesscpp20_
{
    template<auto TFunction, typename... TArgs>
    concept CIsStaticFunctionPtr = requires(TArgs&&... Args)
    {
        {TFunction(std::forward<TArgs>(Args)...)};
    };

    template<auto TMemberPtr>
    concept CIsRegularPointer = requires()
    {
        {*TMemberPtr};
    };
}
#else
#define DEFINE_PRIVATE_MEMBER_ACCESSOR(ClassName, MemberName, Type, ...)\
    PRIVATE_VA_ASSERT("Private template accessor not supported in C++17, use DEFINE_PRIVATE_MEMBER_ACCESSOR_TMPLT instead", __VA_ARGS__)\
    DEFINE_PRIVATE_MEMBER_ACCESSOR_Impl(ClassName, MemberName, Type,\
    PRIVATE_ACCESS_PROLOGUE,\
    PRIVATE_DEFINE_TYPE,\
    PRIVATE_MEMBER_CALLAPI_NO_CONDITION)

#define DEFINE_PRIVATE_FUNCTION_ACCESSOR(ClassName, MemberName, ReturnType, ...)\
    PRIVATE_VA_ASSERT("Private function overload accessor not supported in C++17, use DEFINE_PRIVATE_FUNCTION_ACCESSOR_OVERLOAD instead", __VA_ARGS__)\
    DEFINE_PRIVATE_FUNCTION_ACCESSOR_Impl(ClassName, MemberName, ReturnType,\
    PRIVATE_ACCESS_PROLOGUE,\
    PRIVATE_DEFINE_TYPE,\
    CREATE_NO_OVERLOAD_HELPER,\
    PRIVATE_RESOLVE_FUNCTION_ADDRESS,\
    CREATE_CALL_FUNCTION)

namespace BMPrivateAccesscpp17_
{
    //cpp17 version using SFINAE
    template<auto TFunctionPtr, typename... TArgs>
    struct TIsStaticFunctionPtr
    {
        static constexpr bool bValue = true; 
    };

    template<auto TFunctionPtr, typename TObj, typename... TArgs>
    struct TIsStaticFunctionPtr<TFunctionPtr, TObj, TArgs...>
    {
        template<decltype(TFunctionPtr) TPtr>
        static constexpr auto Test() -> decltype((std::declval<TObj>().*TPtr)(std::declval<TArgs>()...), std::false_type{})
        {
            return std::false_type{};
        }

        template<decltype(TFunctionPtr) TPtr>
        static constexpr auto Test(...) -> std::true_type
        {
            return std::true_type{};
        }

        static constexpr bool bValue = decltype(Test<TFunctionPtr>())::value;
    };

    template<typename TPointer>
    struct TIsRegularPointer : public std::false_type
    {
    };

    template<typename TPointer>
    struct TIsRegularPointer<TPointer*> : public std::true_type
    {
    };
}
#endif

namespace BMPrivateAccess
{
    template<auto TFunction, typename... TArgs>
    inline static constexpr bool bIsStaticFunctionPtr =
#if PRIVATE_CPP20_FEATURE_SET
        BMPrivateAccesscpp20_::CIsStaticFunctionPtr<TFunction, TArgs...>;
#else
        BMPrivateAccesscpp17_::TIsStaticFunctionPtr<TFunction, TArgs...>::bValue;
#endif

    template<auto TPointer>
    inline static constexpr bool bIsRegularPointer =
#if PRIVATE_CPP20_FEATURE_SET
        BMPrivateAccesscpp20_::CIsRegularPointer<TPointer>;
#else
        BMPrivateAccesscpp17_::TIsRegularPointer<decltype(TPointer)>::value;
#endif
    
    template<typename T>
    struct TIsConstFunctionPtr : public std::false_type {};
    
    template<typename C, typename R, typename... TArgs>
    struct TIsConstFunctionPtr<R(C::*)(TArgs...) const> : public std::true_type {};
    
    template<typename... TArgs>
    struct TArgCounter
    {
        static constexpr size_t Value = sizeof...(TArgs);
    };
    
    template<auto TFunctionPtr, typename... TArgs>
    struct TGetFunctionReturnType
    {
        using Type = std::invoke_result_t<decltype(TFunctionPtr), TArgs...>;
    };

    template<typename TTuple>
    struct TTupleVoidDecay
    {
        using Type = TTuple;
    };
    
    template<>
    struct TTupleVoidDecay<std::tuple<void>>
    {
        using Type = std::tuple<>;
    };
    
    template<typename... TArgs>
    struct TOverloadHelper
    {
        template<typename... TTupleArgs>
        using TTuple = std::tuple<std::decay_t<TTupleArgs>...>;
        
        template<typename... TOverloadArgs>
        static constexpr bool bSame = std::is_same_v<TTuple<TArgs...>, typename TTupleVoidDecay<TTuple<TOverloadArgs...>>::Type>;
    };
    
    template<typename Tag, auto TMemberPtr>
    struct TAccessPrivateMember
    {
        template<typename... TArgs>
        friend auto& AccessPrivate(Tag, TArgs&&... Args)
        {
            if constexpr(bIsRegularPointer<TMemberPtr>)
            {
                return *TMemberPtr;
            }
            else
            {
                auto Helper = [](auto&& Object, auto&&...) -> auto&
                {
                    return Object.*TMemberPtr;
                };

                return Helper(std::forward<TArgs>(Args)...);
            }
        }
    };

    template<typename Tag, typename T, typename TReturn, auto TFunctionPtr>
    struct TAccessPrivateFunction
    {
        //Workaround for internal compiler error with msvc 14.43.34808
        using TFunctionType = decltype(TFunctionPtr);
        inline static const TFunctionType TFunctionValue = TFunctionPtr;
        
        template<typename TObject, typename... THelperArgs>
        static auto CallHelper(TObject&& Object, THelperArgs&&... HelperArgs) -> TReturn
        {
            return (Object.*TFunctionPtr)(std::forward<THelperArgs>(HelperArgs)...);
        };
        
        template<typename... TArgs>
        friend TReturn CallPrivate(Tag, TArgs&&... Args)
        {
            if constexpr(bIsStaticFunctionPtr<TFunctionPtr, TArgs...>)
            {
                return TFunctionValue(std::forward<TArgs>(Args)...);
            }
            else
            {
                return CallHelper(std::forward<TArgs>(Args)...);
            }
        }
        
        friend constexpr bool IsConstFunction(Tag)
        {
            return TIsConstFunctionPtr<TFunctionType>::value;
        }
    };

    template<typename Tag, typename T>
    struct TAccessPrivateType
    {
        friend PRIVATE_CONSTEVAL auto* ResolvePrivateType(Tag)
        {
            T* Ptr = nullptr;
            return Ptr;
        }
    };
    
    namespace Danger
    {
        /*
         * DANGER: This function swaps the vtable of the provided object with the vtable of the parent object for the duration of the callable
         * - This is UB and only works on compilers that place the vtable pointer at the start of the object
         * - During the call to any parent functions, the object will be treated as the parent object
         * - Executing any virtual functions on the object will call the parent function
         * - Calling any virtual function not present in the parent object will result in a crash or worse
         */
        template<typename TThis, typename TParent, typename TCallable>
        void SwapVTable(TThis&& ThisObj, TParent&& ParentObj, const TCallable& Callable)
        {
            using TThisType = std::decay_t<TThis>;
            using TParentType = std::decay_t<TParent>;
            static_assert(std::is_base_of_v<TParentType, TThisType>, "TThis must be derived from TParent");

            void*** ThisVTable = (void***)&ThisObj;
            void** ThisVTableValue = *ThisVTable;
            void*** ParentVTable = (void***)&ParentObj;

            *ThisVTable = *ParentVTable;
            Callable();
            *ThisVTable = ThisVTableValue;
        }
    }
}
