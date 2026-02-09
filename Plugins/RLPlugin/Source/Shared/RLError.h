#pragma once

namespace RL
{

struct Result
{
    Result() {}
    explicit Result( long nRet ) : m_nRet( nRet ) {}
    operator long() const { return m_nRet; }
    bool operator>( const Result& kRet ) const { return m_nRet > kRet.m_nRet; }
    bool operator<( const Result& kRet ) const { return m_nRet < kRet.m_nRet; }
    bool operator>=( const Result& kRet ) const { return !operator<( kRet ); }
    bool operator<=( const Result& kRet ) const { return !operator>( kRet ); }
    bool operator==( const Result& kRet ) const { return m_nRet == kRet.m_nRet; }
    bool operator!=( const Result& kRet ) const { return !operator==( kRet ); }

    unsigned long HexFull() const { return *( unsigned long* )&m_nRet; }
    long DecFull() const { return m_nRet; }

    long Code() const { return ( m_nRet )& 0xFFFF; }
    long Module() const { return ( m_nRet >> 16 ) & 0xFFF; }
    long Application() const { return ( m_nRet >> 28 ) & 0x1; }
    long Error() const { return ( m_nRet >> 31 ) & 0x1; }

    long Identifier() const { return ( m_nRet & 0x1FFF0000 ); }
    long Message() const { return ( m_nRet & 0x8000FFFF ); }

    bool IsApplication() const { return Application() ? true : false; }
    bool IsError() const { return Error() ? true : false; }

    long m_nRet;
};

}

#define RL_OK                       RL::Result( 0x00000000 )
#define RL_SAFE                     RL::Result( 0x00000001 )
#define RL_FAIL                     RL::Result( 0x80000000 )
#define RL_NOT_IMPLEMENTED	        RL::Result( 0x80000001 )
#define RL_OUT_OF_MEMORY            RL::Result( 0x80000002 )
#define RL_INVALID_CALL             RL::Result( 0x80000003 )
#define RL_OUT_OF_BOUND             RL::Result( 0x80000004 )
#define RL_GPU_HUNG                 RL::Result( 0x80000005 )
#define RL_GPU_RESET                RL::Result( 0x80000007 )
#define RL_GPU_REMOVED              RL::Result( 0x80000008 )

inline bool RLOK( const RL::Result& kRet ) { return kRet >= RL_OK; }
inline bool RLFail( const RL::Result& kRet ) { return kRet < RL_OK; }

#define WIDE_PREFIX( x ) L##x
#define WIDE_CHAR( x ) WIDE_PREFIX( x )

#define MTBegin \
    auto kMultiThreadErrorCode = RL_OK; \
    std::mutex g_kUndoRedoMutex;

#define MTEnd \
    if ( kMultiThreadErrorCode != RL_OK ) \
    { \
        return kMultiThreadErrorCode; \
    }

#define MTRebegin \
    kMultiThreadErrorCode = RL_OK;

#define MTFailReturnAssert( Func )         \
{                                          \
    RL::Result __nResult = ( Func );       \
    if ( RLFail( __nResult ) )             \
    {                                      \
        ensure( false );                   \
        g_kUndoRedoMutex.lock();           \
        kMultiThreadErrorCode = __nResult; \
        g_kUndoRedoMutex.unlock();         \
        return __nResult;                  \
    }                                      \
}
#define MTFRA( Func ) MTFailReturnAssert( Func )

#define MTPtrCheckAssert( _ptr )             \
if ( !( _ptr ) )                             \
{                                            \
    ensure( false );                         \
    kMultiThreadErrorCode = RL_ILLEGAL_DATA; \
    return RL_ILLEGAL_DATA;                  \
}
#define MTPCA( Func ) MTPtrCheckAssert( Func )


#define RLFailReturn(Func) \
{\
    RL::Result __nResult = (Func);\
    if ( RLFail( __nResult ) )\
    {\
        return __nResult;\
    }\
}
#define RLFR( Func ) RLFailReturn( Func )

#define RLFailReturnAssert( Func )  \
{                                   \
    RL::Result __nResult = ( Func );\
    if ( RLFail( __nResult ) )      \
    {                               \
        ensure( false );            \
        return __nResult;           \
    }                               \
}
#define RLFRA( Func ) RLFailReturnAssert( Func )

#define RLFailPass( Func )  \
{                           \
    Func;                   \
}
#define RLFP( Func ) RLFailPass( Func )

#define RLFailContinue( Func )    \
{                                 \
    RL::Result __nResult = (Func);\
    if ( RLFail( __nResult ) )    \
    {                             \
        continue;                 \
    }                             \
}
#define RLFC( Func ) RLFailContinue( Func )

#define RLFCA( Func )    \
{                                 \
    RL::Result __nResult = (Func);\
    if ( RLFail( __nResult ) )    \
    {                             \
        ensure( 0 );              \
        continue;                 \
    }                             \
}

#define RLFailAssert( Func )        \
{                                    \
    RL::Result __nResult = ( Func ); \
    if( RLFail( __nResult ) )        \
    {                                \
        ensure( false );             \
    }                                \
}
#define RLFA( Func ) RLFailAssert( Func )

#define RLFailReturnVoid( Func ) \
{                                       \
    RL::Result __nResult = ( Func );    \
    if( RLFail( __nResult ) )           \
    {                                   \
        return;                         \
    }                                   \
}
#define RLFRV( Func ) RLFailReturnVoid( Func )

#define RLFaileAssertReturnVoid( Func ) \
{                                       \
    RL::Result __nResult = ( Func );    \
    if( RLFail( __nResult ) )           \
    {                                   \
        ensure( false );                \
        return;                         \
    }                                   \
}
#define RLFARV( Func ) RLFaileAssertReturnVoid( Func )

#define RLFailReturnDefault( Func )  \
{                                    \
    RL::Result __nResult = ( Func ); \
    if( RLFail( __nResult ) )        \
    {                                \
        return {};                   \
    }                                \
}
#define RLFRD( Func ) RLFailReturnDefault( Func )

#define RLFailAssertReturnDefault( Func )  \
{                                    \
    RL::Result __nResult = ( Func ); \
    if( RLFail( __nResult ) )        \
    {                                \
        ensure( false );             \
        return {};                   \
    }                                \
}
#define RLFARD( Func ) RLFailAssertReturnDefault( Func )

#define RLFailGoto(Func, nRet, Tag) \
{\
    nRet = (Func);\
    if ( RLFail( nRet ) )\
    {\
        goto Tag;\
    }\
}
#define RLFG( Func ) RLFailGoto( Func )

#define RLSafeReturn( Func ) \
{\
    RL::Result __nResult = (Func);\
    if ( __nResult == RL_SAFE || RLFail( __nResult ) ) \
    { \
        return __nResult; \
    }\
}
#define RLSR( Func ) RLSafeReturn( Func )

#define RLCancelReturn( Func ) \
{\
    RL::Result __nResult = (Func);\
    if ( __nResult == RL_CANCEL || RLFail( __nResult ) ) \
    { \
        return __nResult; \
    }\
}
#define RLCR( Func ) RLCancelReturn( Func )

#define RLCancelReturnAssert( Func ) \
{\
    RL::Result __nResult = (Func);\
    if ( __nResult == RL_CANCEL || RLFail( __nResult ) ) \
    { \
        if ( RLFail( __nResult ) ) \
        { \
            ensure( false ); \
        } \
        return __nResult; \
    }\
}
#define RLCRA( Func ) RLCancelReturnAssert( Func )

#define RLMemoryCheck(x) \
if ( !(x) )\
{\
    return RL_OUT_OF_MEMORY;\
}
#define RLMc( Func ) RLMemoryCheck( Func )

#define RLMemoryCheckAssert(x) \
if ( !(x) )\
{\
    ensure( false );\
    return RL_OUT_OF_MEMORY;\
}
#define RLMcA( Func ) RLMemoryCheckAssert( Func )

#define RLPtrInvalidCall( _ptr ) \
if ( !( _ptr ) ) \
{ \
    return RL_INVALID_CALL; \
}
#define RLPic( Func ) RLPtrInvalidCall( Func )

#define RLPtrInvalidCallAssert( _ptr ) \
if ( !( _ptr ) ) \
{ \
    ensure( false ); \
    return RL_INVALID_CALL; \
}
#define RLPicA( Func ) RLPtrInvalidCallAssert( Func )

#define RLPtrIllegalData( _ptr ) \
if( !( _ptr ) ) \
{ \
    return RL_ILLEGAL_DATA; \
}
#define RLPid( Func ) RLPtrIllegalData( Func )

#define RLPtrIllegalDataAssert( _ptr ) \
if( !( _ptr ) ) \
{ \
    ensure( false ); \
    return RL_ILLEGAL_DATA; \
}
#define RLPidA( Func ) RLPtrIllegalDataAssert( Func )

#define RLBoundCheck( _exp )    \
if( !( _exp ) )             \
{                           \
    return RL_OUT_OF_BOUND; \
}
#define RLBc( Func ) RLBoundCheck( Func )

#define RLBoundCheckAssert( _exp ) \
if ( !( _exp ) )\
{\
    ensure( false );\
    return RL_OUT_OF_BOUND;\
}
#define RLBcA( Func ) RLBoundCheckAssert( Func )

#define RLPtrNotInitialized(x) \
if ( !(x) )\
{\
    return RL_NOT_INITIALIZED;\
}
#define RLPni( Func ) RLPtrNotInitialized( Func )

#define RLPtrNotInitializedAssert(x) \
if ( !(x) )\
{ \
    ensure( false ); \
    return RL_NOT_INITIALIZED;\
}
#define RLPniA( Func ) RLPtrNotInitializedAssert( Func )

#define RLFailReturnFalse(Func) \
{\
    RL::Result __nResult = (Func);\
    if ( RLFail( __nResult ) )\
    {\
        return false;\
    }\
}
#define RLFRF( Func ) RLFailReturnFalse( Func )

#define RLFailReturnAssertFalse(Func) \
{\
    RL::Result __nResult = (Func);\
    if ( RLFail( __nResult ) )\
    {\
        ensure( false ); \
        return false;\
    }\
}
#define RLFRAF( Func ) RLFailReturnAssertFalse( Func )

#define RLBVA( _Func ) \
{ \
    bool bReturn = ( _Func ); \
    if ( bReturn == false ) \
    { \
        ensure( false ); \
        return; \
    } \
}

#define RLBA( _Func ) \
{ \
    bool bReturn = ( _Func ); \
    if ( bReturn == false ) \
    { \
        ensure( false ); \
    } \
}


