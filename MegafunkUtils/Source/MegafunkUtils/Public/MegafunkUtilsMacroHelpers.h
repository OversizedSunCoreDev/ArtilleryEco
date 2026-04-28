// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
/**
 * A bunch of wrappers around other implementations of accessing private things.
 * Private member functions and members to known types work in all cases, but there are some outliers
 * I would use the built-in UE setup if it worked but it does't support overloads
 * The other weird case are private types which we cheese out using a small part of BMPrivateAccess (thanks to Blue Man)
 */
#define MEGAFUNKUTILS_USE_ACCESS_PRIVATE 1

#ifndef MEGAFUNKUTILS_CUSTOM_BUILD
#define MEGAFUNKUTILS_CUSTOM_BUILD 0
#endif

#if !MEGAFUNKUTILS_CUSTOM_BUILD

// Credit to https://github.com/Blueman2/BMPrivateAccess/tree/main for showing how to access a private type
namespace Megafunk::BMPrivateAccess {
	template<typename Tag, typename T>
	struct TAccessPrivateType
	{
		friend consteval auto* ResolvePrivateType(Tag)
		{
			T* Ptr = nullptr;
			return Ptr;
		}
	};
}

#define MFUTILS_DEFINE_PRIVATE_ACCESS_TYPE(ContainerType, Type2) \
namespace Megafunk::BMPrivateAccess\
{\
struct ContainerType##_##Type2##_PrivateAccessTag\
{\
};\
\
template struct TAccessPrivateType<ContainerType##_##Type2##_PrivateAccessTag, ContainerType::Type2>;\
consteval auto* ResolvePrivateType(BMPrivateAccess::ContainerType##_##Type2##_PrivateAccessTag);\
}\
namespace ContainerType##_Private\
{\
using Type2 = std::remove_pointer_t<decltype(ResolvePrivateType(Megafunk::BMPrivateAccess::ContainerType##_##Type2##_PrivateAccessTag{}))>;\
}\

#define MFUTILS_USE_PRIVATE_ACCESS_TYPE(ContainerType, PrivateType) ContainerType##_Private::##PrivateType;


#if MEGAFUNKUTILS_USE_ACCESS_PRIVATE == 1

#include "BMPrivateAccess/BMPrivateAccess.h"

#define MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(ContainerType, ReturnType, CallArgsInParenthesis, MemberName) \
DEFINE_PRIVATE_FUNCTION_ACCESSOR(ContainerType, MemberName, ReturnType)

#define MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC_OVERLOAD(ContainerType, ReturnType, CallArgsInParenthesis, MemberName, ...) \
DEFINE_PRIVATE_FUNCTION_ACCESSOR_OVERLOAD(ContainerType, MemberName, ReturnType, __VA_ARGS__)

#define MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(ContainerType, MemberType, MemberName) \
DEFINE_PRIVATE_MEMBER_ACCESSOR(ContainerType, MemberName, MemberType)

// Accesses a private member. This assumes you used MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER in the global namespace to make a private macro to access!
#define MFUTILS_GET_PRIVATE(ContainerType, Ref, MemberName) ContainerType##_Private::Get_##MemberName(Ref)

// Calls a private function. This assumes you used MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC in the global namespace to make a private macro to access!
#define MFUTILS_CALL_PRIVATE(ContainerType, Ref, MemberName) ContainerType##_Private::Call_##MemberName(Ref)
/**
 * Calls a private function. This assumes you used MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC in the global namespace to make a private macro to access!
 * Args are __VA_ARGS__ style, so you can call it like  MFUTILS_CALL_PRIVATE_ARGS(*OwherPtr, FunctionName, Arg1, Arg2);
 */
#define MFUTILS_CALL_PRIVATE_ARGS(ContainerType, Ref, MemberName, ...) ContainerType##_Private::Call_##MemberName(Ref, __VA_ARGS__)

#elif MEGAFUNKUTILS_USE_ACCESS_PRIVATE == 2 

#define MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(ContainerType, ReturnType, CallArgsInParenthesis, MemberName) \
ACCESS_PRIVATE_FUN(ContainerType, ReturnType CallArgsInParenthesis, MemberName)

#define MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC_OVERLOAD(ContainerType, ReturnType, CallArgsInParenthesis, MemberName, ...) \
ACCESS_PRIVATE_FUN(ContainerType, ReturnType CallArgsInParenthesis, MemberName)

#define MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(ContainerType, MemberType, MemberName) \
ACCESS_PRIVATE_FIELD(ContainerType, MemberType, MemberName)

// Accesses a private member. This assumes you used MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER in the global namespace to make a private macro to access!
#define MFUTILS_GET_PRIVATE(ContainerType, Ref, MemberName) access_private::MemberName(Ref)

// Calls a private function. This assumes you used MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC in the global namespace to make a private macro to access!
#define MFUTILS_CALL_PRIVATE(ContainerType, Ref, MemberName) call_private::MemberName(Ref)
/**
 * Calls a private function. This assumes you used MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC in the global namespace to make a private macro to access!
 * Args are __VA_ARGS__ style, so you can call it like  MFUTILS_CALL_PRIVATE_ARGS(*OwherPtr, FunctionName, Arg1, Arg2);
 */
#define MFUTILS_CALL_PRIVATE_ARGS(ContainerType, Ref, MemberName, ...) call_private::MemberName(Ref, __VA_ARGS__)


#else
#include "Misc/DefinePrivateMemberPtr.h"
// This is just UE_DEFINE_PRIVATE_MEMBER_PTR but less awkward to use. There are way better templates out there but I don't need fancy stuff
// Note that this doesn't check for duplicate member names (yeah this is bad, I don't need to care right now though)
#define MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(ContainerType, ReturnType, CallArgsInParenthesis, MemberName) \
	UE_DEFINE_PRIVATE_MEMBER_PTR(ReturnType CallArgsInParenthesis, GPrivateMacro##MemberName,ContainerType,MemberName)

// This does not work! yay! (This is why we have to use the third party access private header)
#define MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC_OVERLOAD(ContainerType, ReturnType, CallArgsInParenthesis, MemberName) \
	UE_DEFINE_PRIVATE_MEMBER_PTR(ReturnType##CallArgsInParenthesis, GPrivateMacro##MemberName, ContainerType, MemberName)

#define MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(ContainerType, MemberType, MemberName) \
	UE_DEFINE_PRIVATE_MEMBER_PTR(MemberType, GPrivateMacro##MemberName, ContainerType, MemberName);

// Accesses a private member. This assumes you used MFUTILS_DEFINE_PRIVATE_ACCESS_PTR in the global namespace to make a private macro to access!
#define MFUTILS_GET_PRIVATE(ContainerType, Ref, MemberName) (Ref.*GPrivateMacro##MemberName)

// Calls a private function. This assumes you used MFUTILS_DEFINE_PRIVATE_ACCESS_PTR in the global namespace to make a private macro to access!
#define MFUTILS_CALL_PRIVATE(ContainerType, Ref, MemberName) (Ref.*GPrivateMacro##MemberName)()
#define MFUTILS_CALL_PRIVATE_OVERLOAD(ContainerType, Ref, MemberName) (Ref.*GPrivateMacro##MemberName)()
/**
 * Args are __VA_ARGS__ style, so you can call it like  MFUTILS_CALL_PRIVATE_ARGS(ContainerType, *OwherPtr, FunctionName, Arg1, Arg2);
 * This assumes you used MFUTILS_DEFINE_PRIVATE_ACCESS_PTR in the global namespace to make a private macro to access
 */
#define MFUTILS_CALL_PRIVATE_ARGS(ContainerType, Ref, MemberName, ...) (Ref.*GPrivateMacro##MemberName)(__VA_ARGS__)
#endif

#else

#define MFUTILS_DEFINE_PRIVATE_ACCESS_PTR(ContainerType, MemberType, MemberName)
#define MFUTILS_GET_PRIVATE(Ref, MemberName) Ref.MemberName
#define MFUTILS_CALL_PRIVATE(Ref, MemberName) Ref.MemberName()

#define MFUTILS_CALL_PRIVATE_ARGS(Ref, MemberName, ...) Ref.MemberName(__VA_ARGS__)
#endif
