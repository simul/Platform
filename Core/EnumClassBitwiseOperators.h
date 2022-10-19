#pragma once

template<typename _Ty>
constexpr _Ty operator|(_Ty lhs, _Ty rhs)
{
	return static_cast<_Ty>(static_cast<typename std::underlying_type<_Ty>::type>(lhs) | static_cast<typename std::underlying_type<_Ty>::type>(rhs));
}
template<typename _Ty>
constexpr _Ty operator&(_Ty lhs, _Ty rhs)
{
	return static_cast<_Ty>(static_cast<typename std::underlying_type<_Ty>::type>(lhs) & static_cast<typename std::underlying_type<_Ty>::type>(rhs));
}
template<typename _Ty>
constexpr _Ty operator^(_Ty lhs, _Ty rhs)
{
	return static_cast<_Ty>(static_cast<typename std::underlying_type<_Ty>::type>(lhs) ^ static_cast<typename std::underlying_type<_Ty>::type>(rhs));
}
template<typename _Ty>
constexpr _Ty operator~(_Ty rhs)
{
	return static_cast<_Ty>(~static_cast<typename std::underlying_type<_Ty>::type>(rhs));
}
template<typename _Ty>
constexpr _Ty& operator|=(_Ty& lhs, _Ty rhs)
{
	lhs = static_cast<_Ty>(static_cast<typename std::underlying_type<_Ty>::type>(lhs) | static_cast<typename std::underlying_type<_Ty>::type>(rhs));
	return lhs;
}
template<typename _Ty>
constexpr _Ty& operator&=(_Ty& lhs, _Ty rhs)
{
	lhs = static_cast<_Ty>(static_cast<typename std::underlying_type<_Ty>::type>(lhs) & static_cast<typename std::underlying_type<_Ty>::type>(rhs));
	return lhs;
}
template<typename _Ty>
constexpr _Ty& operator^=(_Ty& lhs, _Ty rhs)
{
	lhs = static_cast<_Ty>(static_cast<typename std::underlying_type<_Ty>::type>(lhs) ^ static_cast<typename std::underlying_type<_Ty>::type>(rhs));
	return lhs;
}