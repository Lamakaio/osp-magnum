#pragma once

#include <algorithm>
#include <godot_cpp/variant/variant.hpp>


namespace godot {

template<size_t N>
struct StringLiteral {
    [[nodiscard]] constexpr size_t size() const {return N;};
    constexpr StringLiteral() = default;
    constexpr StringLiteral(const char (&str)[N]) {         
    std::copy_n(str, N, value); }
    char value[N];
    auto operator<=>(const StringLiteral&) const = default;
    bool operator==(const StringLiteral&) const  = default;
};

//constexpr concat of string literals
template<size_t N, size_t P, StringLiteral<N> S1, StringLiteral<P> S2>
static constexpr StringLiteral<N+P-1> concat() {
    StringLiteral<N+P-1> ret;
    std::copy_n(S1.value, N-1, ret.value);
    std::copy_n(S2.value, P, &ret.value[N-1]);
    return ret;
}


template<class T>
inline Variant::Type get_gd_type();

template<>
inline Variant::Type get_gd_type<String>() {
    return Variant::STRING;
}
template<>
inline Variant::Type get_gd_type<NodePath>() {
    return Variant::NODE_PATH;
}
// add others as needed



}


