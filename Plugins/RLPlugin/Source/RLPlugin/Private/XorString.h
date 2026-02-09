#pragma once

// https://stackoverflow.com/questions/4102320/c-how-to-encrypt-strings-at-compile-time
// need C++ 11 constexpr
namespace RLEncryptTool{

//-------------------------------------------------------------//
// "Malware related compile-time hacks with C++11" by LeFF   //
// You can use this code however you like, I just don't really //
// give a shit, but if you feel some respect for me, please //
// don't cut off this comment when copy-pasting... ;-)       //
//-------------------------------------------------------------//

////////////////////////////////////////////////////////////////////
template <int X> struct EnsureCompileTime {
    enum : int {
        Value = X
    };
};
////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
//Use Compile-Time as seed
#define Seed ((__TIME__[7] - '0') * 1  + (__TIME__[6] - '0') * 10  + \
              (__TIME__[4] - '0') * 60   + (__TIME__[3] - '0') * 600 + \
              (__TIME__[1] - '0') * 3600 + (__TIME__[0] - '0') * 36000)
////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
constexpr int LinearCongruentGenerator(int Rounds) {
    return 1013904223 + 1664525 * ((Rounds> 0) ? LinearCongruentGenerator(Rounds - 1) : Seed & 0xFFFFFFFF);
}
#define Random() EnsureCompileTime<LinearCongruentGenerator(10)>::Value //10 Rounds
#define RandomNumber(Min, Max) (Min + (Random() % (Max - Min + 1)))
////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
template <int... Pack> struct IndexList {};
////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
template <typename IndexList, int Right> struct Append;
template <int... Left, int Right> struct Append<IndexList<Left...>, Right> {
    typedef IndexList<Left..., Right> Result;
};
////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
template <int N> struct ConstructIndexList {
    typedef typename Append<typename ConstructIndexList<N - 1>::Result, N - 1>::Result Result;
};
template <> struct ConstructIndexList<0> {
    typedef IndexList<> Result;
};
////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
const char XORKEY = static_cast<char>(RandomNumber(0, 0xFF));
constexpr char EncryptCharacter(const char Character, int Index) {
    return Character ^ (XORKEY + Index);
}

template <typename IndexList> class CXorString;
template <int... Index> class CXorString<IndexList<Index...> > {
private:
    char Value[sizeof...(Index) + 1];
    wchar_t ValueW[sizeof...( Index ) + 1];
public:
    constexpr CXorString(const char* const String)
    : Value{ EncryptCharacter(String[Index], Index)... } {}

    char* decrypt() {
        for(int t = 0; t < sizeof...(Index); t++) {
            Value[t] = Value[t] ^ (XORKEY + t);
        }
        Value[sizeof...(Index)] = '\0';
        return Value;
    }
    wchar_t* decryptW() {
        for ( int t = 0; t < sizeof...( Index ); t++ ) {
            Value[ t ] = Value[ t ] ^ ( XORKEY + t );
            ValueW[ t ] = Value[ t ];
        }
        Value[ sizeof...( Index ) ] = '\0';
        ValueW[ sizeof...( Index ) ] = '\0';
        return ValueW;
    }
    char* get() {
        return Value;
    }
};



#define XorS(X, String) RLEncryptTool::CXorString<RLEncryptTool::ConstructIndexList<sizeof(String)-1>::Result> X(String)
#define XorString( String ) ( RLEncryptTool::CXorString<RLEncryptTool::ConstructIndexList<sizeof( String ) - 1>::Result>( String ).decrypt() )
#define XorStringW( String ) ( RLEncryptTool::CXorString<RLEncryptTool::ConstructIndexList<sizeof( String ) - 1>::Result>( String ).decryptW() )

};
