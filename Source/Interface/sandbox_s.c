

/* this ALWAYS GENERATED file contains the RPC server stubs */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 11:14:07 2038
 */
/* Compiler settings for ..\..\Source\Interface\sandbox.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0628 
    protocol : all , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if defined(_M_AMD64)


#if _MSC_VER >= 1200
#pragma warning(push)
#endif

#pragma warning( disable: 4211 )  /* redefine extern to static */
#pragma warning( disable: 4232 )  /* dllimport identity*/
#pragma warning( disable: 4024 )  /* array to pointer mapping*/

#include <string.h>
#include "sandbox_h.h"

#define TYPE_FORMAT_STRING_SIZE   131                               
#define PROC_FORMAT_STRING_SIZE   157                               
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   0            

typedef struct _sandbox_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } sandbox_MIDL_TYPE_FORMAT_STRING;

typedef struct _sandbox_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } sandbox_MIDL_PROC_FORMAT_STRING;

typedef struct _sandbox_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } sandbox_MIDL_EXPR_FORMAT_STRING;


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax_2_0 = 
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};

static const RPC_SYNTAX_IDENTIFIER  _NDR64_RpcTransferSyntax_1_0 = 
{{0x71710533,0xbeba,0x4937,{0x83,0x19,0xb5,0xdb,0xef,0x9c,0xcc,0x36}},{1,0}};

#if defined(_CONTROL_FLOW_GUARD_XFG)
#define XFG_TRAMPOLINES(ObjectType)\
NDR_SHAREABLE unsigned long ObjectType ## _UserSize_XFG(unsigned long * pFlags, unsigned long Offset, void * pObject)\
{\
return  ObjectType ## _UserSize(pFlags, Offset, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserMarshal_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserMarshal(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserUnmarshal_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserUnmarshal(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE void ObjectType ## _UserFree_XFG(unsigned long * pFlags, void * pObject)\
{\
ObjectType ## _UserFree(pFlags, (ObjectType *)pObject);\
}
#define XFG_TRAMPOLINES64(ObjectType)\
NDR_SHAREABLE unsigned long ObjectType ## _UserSize64_XFG(unsigned long * pFlags, unsigned long Offset, void * pObject)\
{\
return  ObjectType ## _UserSize64(pFlags, Offset, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserMarshal64_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserMarshal64(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserUnmarshal64_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserUnmarshal64(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE void ObjectType ## _UserFree64_XFG(unsigned long * pFlags, void * pObject)\
{\
ObjectType ## _UserFree64(pFlags, (ObjectType *)pObject);\
}
#define XFG_BIND_TRAMPOLINES(HandleType, ObjectType)\
static void* ObjectType ## _bind_XFG(HandleType pObject)\
{\
return ObjectType ## _bind((ObjectType) pObject);\
}\
static void ObjectType ## _unbind_XFG(HandleType pObject, handle_t ServerHandle)\
{\
ObjectType ## _unbind((ObjectType) pObject, ServerHandle);\
}
#define XFG_TRAMPOLINE_FPTR(Function) Function ## _XFG
#define XFG_TRAMPOLINE_FPTR_DEPENDENT_SYMBOL(Symbol) Symbol ## _XFG
#else
#define XFG_TRAMPOLINES(ObjectType)
#define XFG_TRAMPOLINES64(ObjectType)
#define XFG_BIND_TRAMPOLINES(HandleType, ObjectType)
#define XFG_TRAMPOLINE_FPTR(Function) Function
#define XFG_TRAMPOLINE_FPTR_DEPENDENT_SYMBOL(Symbol) Symbol
#endif


extern const sandbox_MIDL_TYPE_FORMAT_STRING sandbox__MIDL_TypeFormatString;
extern const sandbox_MIDL_PROC_FORMAT_STRING sandbox__MIDL_ProcFormatString;
extern const sandbox_MIDL_EXPR_FORMAT_STRING sandbox__MIDL_ExprFormatString;

/* Standard interface: __MIDL_itf_sandbox_0000_0000, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Standard interface: Sandbox, ver. 1.0,
   GUID={0x1139fa8f,0x8df4,0x4538,{0xa8,0x2f,0xea,0x47,0x7b,0x3e,0x8e,0xa9}} */


extern const MIDL_SERVER_INFO Sandbox_ServerInfo;

extern const RPC_DISPATCH_TABLE Sandbox_v1_0_DispatchTable;

static const RPC_SERVER_INTERFACE Sandbox___RpcServerInterface =
    {
    sizeof(RPC_SERVER_INTERFACE),
    {{0x1139fa8f,0x8df4,0x4538,{0xa8,0x2f,0xea,0x47,0x7b,0x3e,0x8e,0xa9}},{1,0}},
    {{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}},
    (RPC_DISPATCH_TABLE*)&Sandbox_v1_0_DispatchTable,
    0,
    0,
    0,
    &Sandbox_ServerInfo,
    0x06000000
    };
RPC_IF_HANDLE Sandbox_v1_0_s_ifspec = (RPC_IF_HANDLE)& Sandbox___RpcServerInterface;
#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Sandbox_StubDesc;
#ifdef __cplusplus
}
#endif


#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif

static const sandbox_MIDL_PROC_FORMAT_STRING sandbox__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure RpcSendFrame */

			0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x0 ),	/* 0 */
/*  8 */	NdrFcShort( 0x58 ),	/* X64 Stack size/offset = 88 */
/* 10 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 12 */	NdrFcShort( 0x0 ),	/* X64 Stack size/offset = 0 */
/* 14 */	NdrFcShort( 0x4c ),	/* 76 */
/* 16 */	NdrFcShort( 0x24 ),	/* 36 */
/* 18 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0xa,		/* 10 */
/* 20 */	0xa,		/* 10 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 22 */	NdrFcShort( 0x1 ),	/* 1 */
/* 24 */	NdrFcShort( 0x1 ),	/* 1 */
/* 26 */	NdrFcShort( 0x0 ),	/* 0 */
/* 28 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter Width */

/* 30 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 32 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 34 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter Height */

/* 36 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 38 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 40 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter Size */

/* 42 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 44 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 46 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter BitmapData */

/* 48 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 50 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 52 */	NdrFcShort( 0x2 ),	/* Type Offset=2 */

	/* Parameter CursorX */

/* 54 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 56 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 58 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter CursorY */

/* 60 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 62 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 64 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter CursorVisible */

/* 66 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 68 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 70 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter EventCount */

/* 72 */	NdrFcShort( 0x158 ),	/* Flags:  in, out, base type, simple ref, */
/* 74 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 76 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter Events */

/* 78 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 80 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 82 */	NdrFcShort( 0x5a ),	/* Type Offset=90 */

	/* Return value */

/* 84 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 86 */	NdrFcShort( 0x50 ),	/* X64 Stack size/offset = 80 */
/* 88 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RpcSetCursorShape */

/* 90 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 92 */	NdrFcLong( 0x0 ),	/* 0 */
/* 96 */	NdrFcShort( 0x1 ),	/* 1 */
/* 98 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 100 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 102 */	NdrFcShort( 0x0 ),	/* X64 Stack size/offset = 0 */
/* 104 */	NdrFcShort( 0x28 ),	/* 40 */
/* 106 */	NdrFcShort( 0x0 ),	/* 0 */
/* 108 */	0x42,		/* Oi2 Flags:  clt must size, has ext, */
			0x6,		/* 6 */
/* 110 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 112 */	NdrFcShort( 0x0 ),	/* 0 */
/* 114 */	NdrFcShort( 0x1 ),	/* 1 */
/* 116 */	NdrFcShort( 0x0 ),	/* 0 */
/* 118 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter Width */

/* 120 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 122 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 124 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter Height */

/* 126 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 128 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 130 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter HotX */

/* 132 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 134 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 136 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter HotY */

/* 138 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 140 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 142 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter Size */

/* 144 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 146 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 148 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter Pixels */

/* 150 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 152 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 154 */	NdrFcShort( 0x70 ),	/* Type Offset=112 */

			0x0
        }
    };

static const sandbox_MIDL_TYPE_FORMAT_STRING sandbox__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x1c,		/* FC_CVARRAY */
			0x0,		/* 0 */
/*  4 */	NdrFcShort( 0x1 ),	/* 1 */
/*  6 */	0x29,		/* Corr desc:  parameter, FC_ULONG */
			0x0,		/*  */
/*  8 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 10 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 12 */	0x29,		/* Corr desc:  parameter, FC_ULONG */
			0x0,		/*  */
/* 14 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 16 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 18 */	0x2,		/* FC_CHAR */
			0x5b,		/* FC_END */
/* 20 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/* 22 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 24 */	
			0x2b,		/* FC_NON_ENCAPSULATED_UNION */
			0x8,		/* FC_LONG */
/* 26 */	0x8,		/* Corr desc: FC_LONG */
			0x0,		/*  */
/* 28 */	NdrFcShort( 0xfffc ),	/* -4 */
/* 30 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 32 */	NdrFcShort( 0x2 ),	/* Offset= 2 (34) */
/* 34 */	NdrFcShort( 0x10 ),	/* 16 */
/* 36 */	NdrFcShort( 0x3 ),	/* 3 */
/* 38 */	NdrFcLong( 0x0 ),	/* 0 */
/* 42 */	NdrFcShort( 0x10 ),	/* Offset= 16 (58) */
/* 44 */	NdrFcLong( 0x1 ),	/* 1 */
/* 48 */	NdrFcShort( 0x12 ),	/* Offset= 18 (66) */
/* 50 */	NdrFcLong( 0x2 ),	/* 2 */
/* 54 */	NdrFcShort( 0xc ),	/* Offset= 12 (66) */
/* 56 */	NdrFcShort( 0x0 ),	/* Offset= 0 (56) */
/* 58 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 60 */	NdrFcShort( 0x8 ),	/* 8 */
/* 62 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 64 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 66 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 68 */	NdrFcShort( 0x10 ),	/* 16 */
/* 70 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 72 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 74 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 76 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 78 */	NdrFcShort( 0x14 ),	/* 20 */
/* 80 */	NdrFcShort( 0x0 ),	/* 0 */
/* 82 */	NdrFcShort( 0x0 ),	/* Offset= 0 (82) */
/* 84 */	0x8,		/* FC_LONG */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 86 */	0x0,		/* 0 */
			NdrFcShort( 0xffc1 ),	/* Offset= -63 (24) */
			0x5b,		/* FC_END */
/* 90 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 92 */	NdrFcShort( 0x0 ),	/* 0 */
/* 94 */	0x29,		/* Corr desc:  parameter, FC_ULONG */
			0x54,		/* FC_DEREFERENCE */
/* 96 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 98 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 100 */	0x29,		/* Corr desc:  parameter, FC_ULONG */
			0x54,		/* FC_DEREFERENCE */
/* 102 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 104 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 106 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 108 */	NdrFcShort( 0xffe0 ),	/* Offset= -32 (76) */
/* 110 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 112 */	
			0x1c,		/* FC_CVARRAY */
			0x0,		/* 0 */
/* 114 */	NdrFcShort( 0x1 ),	/* 1 */
/* 116 */	0x29,		/* Corr desc:  parameter, FC_ULONG */
			0x0,		/*  */
/* 118 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 120 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 122 */	0x29,		/* Corr desc:  parameter, FC_ULONG */
			0x0,		/*  */
/* 124 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 126 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 128 */	0x2,		/* FC_CHAR */
			0x5b,		/* FC_END */

			0x0
        }
    };

static const unsigned short Sandbox_FormatStringOffsetTable[] =
    {
    0,
    90
    };


static const RPC_DISPATCH_FUNCTION Sandbox_table[] =
    {
    NdrServerCall2,
    NdrServerCall2,
    0
    };
static const RPC_DISPATCH_TABLE Sandbox_v1_0_DispatchTable = 
    {
    2,
    (RPC_DISPATCH_FUNCTION*)Sandbox_table
    };


#endif /* defined(_M_AMD64)*/



/* this ALWAYS GENERATED file contains the RPC server stubs */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 11:14:07 2038
 */
/* Compiler settings for ..\..\Source\Interface\sandbox.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0628 
    protocol : all , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if defined(_M_AMD64)




#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif


#include "ndr64types.h"
#include "pshpack8.h"
#ifdef __cplusplus
namespace {
#endif


typedef 
struct 
{
    NDR64_FORMAT_UINT32 frag1;
    struct _NDR64_EXPR_NOOP frag2;
    struct _NDR64_EXPR_CONST64 frag3;
}
__midl_frag35_t;
extern const __midl_frag35_t __midl_frag35;

typedef 
struct 
{
    NDR64_FORMAT_UINT32 frag1;
    struct _NDR64_EXPR_VAR frag2;
}
__midl_frag34_t;
extern const __midl_frag34_t __midl_frag34;

typedef 
struct 
{
    struct _NDR64_CONF_VAR_ARRAY_HEADER_FORMAT frag1;
}
__midl_frag32_t;
extern const __midl_frag32_t __midl_frag32;

typedef 
NDR64_FORMAT_CHAR
__midl_frag31_t;
extern const __midl_frag31_t __midl_frag31;

typedef 
struct 
{
    struct _NDR64_PROC_FORMAT frag1;
    struct _NDR64_BIND_AND_NOTIFY_EXTENSION frag2;
    struct _NDR64_PARAM_FORMAT frag3;
    struct _NDR64_PARAM_FORMAT frag4;
    struct _NDR64_PARAM_FORMAT frag5;
    struct _NDR64_PARAM_FORMAT frag6;
    struct _NDR64_PARAM_FORMAT frag7;
    struct _NDR64_PARAM_FORMAT frag8;
}
__midl_frag26_t;
extern const __midl_frag26_t __midl_frag26;

typedef 
struct 
{
    struct _NDR64_STRUCTURE_HEADER_FORMAT frag1;
}
__midl_frag24_t;
extern const __midl_frag24_t __midl_frag24;

typedef 
struct 
{
    struct _NDR64_STRUCTURE_HEADER_FORMAT frag1;
}
__midl_frag22_t;
extern const __midl_frag22_t __midl_frag22;

typedef 
struct 
{
    NDR64_FORMAT_UINT32 frag1;
    struct _NDR64_EXPR_VAR frag2;
}
__midl_frag21_t;
extern const __midl_frag21_t __midl_frag21;

typedef 
struct 
{
    struct _NDR64_NON_ENCAPSULATED_UNION frag1;
    struct _NDR64_UNION_ARM_SELECTOR frag2;
    struct _NDR64_UNION_ARM frag3;
    struct _NDR64_UNION_ARM frag4;
    struct _NDR64_UNION_ARM frag5;
    NDR64_UINT32 frag6;
}
__midl_frag20_t;
extern const __midl_frag20_t __midl_frag20;

typedef 
struct 
{
    struct _NDR64_BOGUS_STRUCTURE_HEADER_FORMAT frag1;
    struct 
    {
        struct _NDR64_SIMPLE_MEMBER_FORMAT frag1;
        struct _NDR64_EMBEDDED_COMPLEX_FORMAT frag2;
        struct _NDR64_SIMPLE_MEMBER_FORMAT frag3;
    } frag2;
}
__midl_frag19_t;
extern const __midl_frag19_t __midl_frag19;

typedef 
struct 
{
    NDR64_FORMAT_UINT32 frag1;
    struct _NDR64_EXPR_OPERATOR frag2;
    struct _NDR64_EXPR_VAR frag3;
}
__midl_frag17_t;
extern const __midl_frag17_t __midl_frag17;

typedef 
struct 
{
    struct _NDR64_CONF_VAR_BOGUS_ARRAY_HEADER_FORMAT frag1;
}
__midl_frag15_t;
extern const __midl_frag15_t __midl_frag15;

typedef 
struct _NDR64_POINTER_FORMAT
__midl_frag13_t;
extern const __midl_frag13_t __midl_frag13;

typedef 
struct 
{
    NDR64_FORMAT_UINT32 frag1;
    struct _NDR64_EXPR_VAR frag2;
}
__midl_frag8_t;
extern const __midl_frag8_t __midl_frag8;

typedef 
struct 
{
    struct _NDR64_CONF_VAR_ARRAY_HEADER_FORMAT frag1;
}
__midl_frag6_t;
extern const __midl_frag6_t __midl_frag6;

typedef 
struct 
{
    struct _NDR64_PROC_FORMAT frag1;
    struct _NDR64_BIND_AND_NOTIFY_EXTENSION frag2;
    struct _NDR64_PARAM_FORMAT frag3;
    struct _NDR64_PARAM_FORMAT frag4;
    struct _NDR64_PARAM_FORMAT frag5;
    struct _NDR64_PARAM_FORMAT frag6;
    struct _NDR64_PARAM_FORMAT frag7;
    struct _NDR64_PARAM_FORMAT frag8;
    struct _NDR64_PARAM_FORMAT frag9;
    struct _NDR64_PARAM_FORMAT frag10;
    struct _NDR64_PARAM_FORMAT frag11;
    struct _NDR64_PARAM_FORMAT frag12;
}
__midl_frag2_t;
extern const __midl_frag2_t __midl_frag2;

typedef 
NDR64_FORMAT_UINT32
__midl_frag1_t;
extern const __midl_frag1_t __midl_frag1;

static const __midl_frag35_t __midl_frag35 =
{ 
/*  */
    (NDR64_UINT32) 1 /* 0x1 */,
    { 
    /* struct _NDR64_EXPR_NOOP */
        0x5,    /* FC_EXPR_PAD */
        (NDR64_UINT8) 4 /* 0x4 */,
        (NDR64_UINT16) 0 /* 0x0 */
    },
    { 
    /* struct _NDR64_EXPR_CONST64 */
        0x2,    /* FC_EXPR_CONST64 */
        0x7,    /* FC64_INT64 */
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT64) 0 /* 0x0 */
    }
};

static const __midl_frag34_t __midl_frag34 =
{ 
/*  */
    (NDR64_UINT32) 1 /* 0x1 */,
    { 
    /* struct _NDR64_EXPR_VAR */
        0x3,    /* FC_EXPR_VAR */
        0x6,    /* FC64_UINT32 */
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT32) 40 /* 0x28 */  /* Offset */
    }
};

static const __midl_frag32_t __midl_frag32 =
{ 
/*  */
    { 
    /* struct _NDR64_CONF_VAR_ARRAY_HEADER_FORMAT */
        0x43,    /* FC64_CONFVAR_ARRAY */
        (NDR64_UINT8) 0 /* 0x0 */,
        { 
        /* struct _NDR64_CONF_VAR_ARRAY_HEADER_FORMAT */
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0
        },
        (NDR64_UINT8) 0 /* 0x0 */,
        (NDR64_UINT32) 1 /* 0x1 */,
        &__midl_frag34,
        &__midl_frag34
    }
};

static const __midl_frag31_t __midl_frag31 =
0x5    /* FC64_INT32 */;

static const __midl_frag26_t __midl_frag26 =
{ 
/* RpcSetCursorShape */
    { 
    /* RpcSetCursorShape */      /* procedure RpcSetCursorShape */
        (NDR64_UINT32) 19136576 /* 0x1240040 */,    /* explicit handle */ /* IsIntrepreted, ClientMustSize, ServerCorrelation, HasExtensions */
        (NDR64_UINT32) 56 /* 0x38 */ ,  /* Stack size */
        (NDR64_UINT32) 40 /* 0x28 */,
        (NDR64_UINT32) 0 /* 0x0 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT16) 6 /* 0x6 */,
        (NDR64_UINT16) 8 /* 0x8 */
    },
    { 
    /* struct _NDR64_BIND_AND_NOTIFY_EXTENSION */
        { 
        /* struct _NDR64_BIND_AND_NOTIFY_EXTENSION */
            0x72,    /* FC64_BIND_PRIMITIVE */
            (NDR64_UINT8) 0 /* 0x0 */,
            0 /* 0x0 */,   /* Stack offset */
            (NDR64_UINT8) 0 /* 0x0 */,
            (NDR64_UINT8) 0 /* 0x0 */
        },
        (NDR64_UINT16) 0 /* 0x0 */      /* Notify index */
    },
    { 
    /* Width */      /* parameter Width */
        &__midl_frag31,
        { 
        /* Width */
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        8 /* 0x8 */,   /* Stack offset */
    },
    { 
    /* Height */      /* parameter Height */
        &__midl_frag31,
        { 
        /* Height */
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        16 /* 0x10 */,   /* Stack offset */
    },
    { 
    /* HotX */      /* parameter HotX */
        &__midl_frag31,
        { 
        /* HotX */
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        24 /* 0x18 */,   /* Stack offset */
    },
    { 
    /* HotY */      /* parameter HotY */
        &__midl_frag31,
        { 
        /* HotY */
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        32 /* 0x20 */,   /* Stack offset */
    },
    { 
    /* Size */      /* parameter Size */
        &__midl_frag31,
        { 
        /* Size */
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        40 /* 0x28 */,   /* Stack offset */
    },
    { 
    /* Pixels */      /* parameter Pixels */
        &__midl_frag32,
        { 
        /* Pixels */
            1,
            1,
            0,
            1,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* MustSize, MustFree, [in] */
        (NDR64_UINT16) 0 /* 0x0 */,
        48 /* 0x30 */,   /* Stack offset */
    }
};

static const __midl_frag24_t __midl_frag24 =
{ 
/* SandboxWheelEvent */
    { 
    /* SandboxWheelEvent */
        0x30,    /* FC64_STRUCT */
        (NDR64_UINT8) 3 /* 0x3 */,
        { 
        /* SandboxWheelEvent */
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0
        },
        (NDR64_UINT8) 0 /* 0x0 */,
        (NDR64_UINT32) 16 /* 0x10 */
    }
};

static const __midl_frag22_t __midl_frag22 =
{ 
/* SandboxKeyboardEvent */
    { 
    /* SandboxKeyboardEvent */
        0x30,    /* FC64_STRUCT */
        (NDR64_UINT8) 3 /* 0x3 */,
        { 
        /* SandboxKeyboardEvent */
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0
        },
        (NDR64_UINT8) 0 /* 0x0 */,
        (NDR64_UINT32) 8 /* 0x8 */
    }
};

static const __midl_frag21_t __midl_frag21 =
{ 
/*  */
    (NDR64_UINT32) 1 /* 0x1 */,
    { 
    /* struct _NDR64_EXPR_VAR */
        0x3,    /* FC_EXPR_VAR */
        0x5,    /* FC64_INT32 */
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT32) 0 /* 0x0 */
    }
};

static const __midl_frag20_t __midl_frag20 =
{ 
/* __MIDL___MIDL_itf_sandbox_0000_0000_0001 */
    { 
    /* __MIDL___MIDL_itf_sandbox_0000_0000_0001 */
        0x51,    /* FC64_NON_ENCAPSULATED_UNION */
        (NDR64_UINT8) 3 /* 0x3 */,
        (NDR64_UINT8) 0 /* 0x0 */,
        0x5,    /* FC64_INT32 */
        (NDR64_UINT32) 16 /* 0x10 */,
        &__midl_frag21,
        (NDR64_UINT32) 0 /* 0x0 */
    },
    { 
    /* struct _NDR64_UNION_ARM_SELECTOR */
        (NDR64_UINT8) 0 /* 0x0 */,
        (NDR64_UINT8) 3 /* 0x3 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT32) 3 /* 0x3 */
    },
    { 
    /* struct _NDR64_UNION_ARM */
        (NDR64_INT64) 0 /* 0x0 */,
        &__midl_frag22,
        (NDR64_UINT32) 0 /* 0x0 */
    },
    { 
    /* struct _NDR64_UNION_ARM */
        (NDR64_INT64) 1 /* 0x1 */,
        &__midl_frag24,
        (NDR64_UINT32) 0 /* 0x0 */
    },
    { 
    /* struct _NDR64_UNION_ARM */
        (NDR64_INT64) 2 /* 0x2 */,
        &__midl_frag24,
        (NDR64_UINT32) 0 /* 0x0 */
    },
    (NDR64_UINT32) 0 /* 0x0 */
};

static const __midl_frag19_t __midl_frag19 =
{ 
/* SandboxEvent */
    { 
    /* SandboxEvent */
        0x34,    /* FC64_BOGUS_STRUCT */
        (NDR64_UINT8) 3 /* 0x3 */,
        { 
        /* SandboxEvent */
            0,
            1,
            0,
            0,
            0,
            0,
            0,
            0
        },
        (NDR64_UINT8) 0 /* 0x0 */,
        (NDR64_UINT32) 20 /* 0x14 */,
        0,
        0,
        0,
    },
    { 
    /*  */
        { 
        /* struct _NDR64_SIMPLE_MEMBER_FORMAT */
            0x5,    /* FC64_INT32 */
            (NDR64_UINT8) 0 /* 0x0 */,
            (NDR64_UINT16) 0 /* 0x0 */,
            (NDR64_UINT32) 0 /* 0x0 */
        },
        { 
        /* struct _NDR64_EMBEDDED_COMPLEX_FORMAT */
            0x91,    /* FC64_EMBEDDED_COMPLEX */
            (NDR64_UINT8) 0 /* 0x0 */,
            (NDR64_UINT16) 0 /* 0x0 */,
            &__midl_frag20
        },
        { 
        /* struct _NDR64_SIMPLE_MEMBER_FORMAT */
            0x93,    /* FC64_END */
            (NDR64_UINT8) 0 /* 0x0 */,
            (NDR64_UINT16) 0 /* 0x0 */,
            (NDR64_UINT32) 0 /* 0x0 */
        }
    }
};

static const __midl_frag17_t __midl_frag17 =
{ 
/*  */
    (NDR64_UINT32) 1 /* 0x1 */,
    { 
    /* struct _NDR64_EXPR_OPERATOR */
        0x4,    /* FC_EXPR_OPER */
        0x5,    /* OP_UNARY_INDIRECTION */
        0x6,    /* FC64_UINT32 */
        (NDR64_UINT8) 0 /* 0x0 */
    },
    { 
    /* struct _NDR64_EXPR_VAR */
        0x3,    /* FC_EXPR_VAR */
        0x8,    /* FC64_UINT64 */
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT32) 64 /* 0x40 */  /* Offset */
    }
};

static const __midl_frag15_t __midl_frag15 =
{ 
/*  */
    { 
    /* struct _NDR64_CONF_VAR_BOGUS_ARRAY_HEADER_FORMAT */
        { 
        /* struct _NDR64_CONF_VAR_BOGUS_ARRAY_HEADER_FORMAT */
            0x47,    /* FC64_BOGUS_ARRAY */
            (NDR64_UINT8) 3 /* 0x3 */,
            { 
            /* struct _NDR64_CONF_VAR_BOGUS_ARRAY_HEADER_FORMAT */
                0,
                1,
                0,
                0,
                0,
                0,
                0,
                0
            },
            (NDR64_UINT8) 1 /* 0x1 */,
            (NDR64_UINT32) 0 /* 0x0 */,
            &__midl_frag19
        },
        &__midl_frag17,
        &__midl_frag17,
        &__midl_frag35
    }
};

static const __midl_frag13_t __midl_frag13 =
{ 
/* *long */
    0x20,    /* FC64_RP */
    (NDR64_UINT8) 8 /* 0x8 */,
    (NDR64_UINT16) 0 /* 0x0 */,
    &__midl_frag31
};

static const __midl_frag8_t __midl_frag8 =
{ 
/*  */
    (NDR64_UINT32) 1 /* 0x1 */,
    { 
    /* struct _NDR64_EXPR_VAR */
        0x3,    /* FC_EXPR_VAR */
        0x6,    /* FC64_UINT32 */
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT32) 24 /* 0x18 */  /* Offset */
    }
};

static const __midl_frag6_t __midl_frag6 =
{ 
/*  */
    { 
    /* struct _NDR64_CONF_VAR_ARRAY_HEADER_FORMAT */
        0x43,    /* FC64_CONFVAR_ARRAY */
        (NDR64_UINT8) 0 /* 0x0 */,
        { 
        /* struct _NDR64_CONF_VAR_ARRAY_HEADER_FORMAT */
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0
        },
        (NDR64_UINT8) 0 /* 0x0 */,
        (NDR64_UINT32) 1 /* 0x1 */,
        &__midl_frag8,
        &__midl_frag8
    }
};

static const __midl_frag2_t __midl_frag2 =
{ 
/* RpcSendFrame */
    { 
    /* RpcSendFrame */      /* procedure RpcSendFrame */
        (NDR64_UINT32) 23986240 /* 0x16e0040 */,    /* explicit handle */ /* IsIntrepreted, ServerMustSize, ClientMustSize, HasReturn, ServerCorrelation, ClientCorrelation, HasExtensions */
        (NDR64_UINT32) 88 /* 0x58 */ ,  /* Stack size */
        (NDR64_UINT32) 80 /* 0x50 */,
        (NDR64_UINT32) 40 /* 0x28 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT16) 10 /* 0xa */,
        (NDR64_UINT16) 8 /* 0x8 */
    },
    { 
    /* struct _NDR64_BIND_AND_NOTIFY_EXTENSION */
        { 
        /* struct _NDR64_BIND_AND_NOTIFY_EXTENSION */
            0x72,    /* FC64_BIND_PRIMITIVE */
            (NDR64_UINT8) 0 /* 0x0 */,
            0 /* 0x0 */,   /* Stack offset */
            (NDR64_UINT8) 0 /* 0x0 */,
            (NDR64_UINT8) 0 /* 0x0 */
        },
        (NDR64_UINT16) 0 /* 0x0 */      /* Notify index */
    },
    { 
    /* Width */      /* parameter Width */
        &__midl_frag31,
        { 
        /* Width */
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        8 /* 0x8 */,   /* Stack offset */
    },
    { 
    /* Height */      /* parameter Height */
        &__midl_frag31,
        { 
        /* Height */
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        16 /* 0x10 */,   /* Stack offset */
    },
    { 
    /* Size */      /* parameter Size */
        &__midl_frag31,
        { 
        /* Size */
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        24 /* 0x18 */,   /* Stack offset */
    },
    { 
    /* BitmapData */      /* parameter BitmapData */
        &__midl_frag6,
        { 
        /* BitmapData */
            1,
            1,
            0,
            1,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* MustSize, MustFree, [in] */
        (NDR64_UINT16) 0 /* 0x0 */,
        32 /* 0x20 */,   /* Stack offset */
    },
    { 
    /* CursorX */      /* parameter CursorX */
        &__midl_frag31,
        { 
        /* CursorX */
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        40 /* 0x28 */,   /* Stack offset */
    },
    { 
    /* CursorY */      /* parameter CursorY */
        &__midl_frag31,
        { 
        /* CursorY */
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        48 /* 0x30 */,   /* Stack offset */
    },
    { 
    /* CursorVisible */      /* parameter CursorVisible */
        &__midl_frag31,
        { 
        /* CursorVisible */
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        56 /* 0x38 */,   /* Stack offset */
    },
    { 
    /* EventCount */      /* parameter EventCount */
        &__midl_frag31,
        { 
        /* EventCount */
            0,
            0,
            0,
            1,
            1,
            0,
            1,
            0,
            1,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [in], [out], Basetype, SimpleRef */
        (NDR64_UINT16) 0 /* 0x0 */,
        64 /* 0x40 */,   /* Stack offset */
    },
    { 
    /* Events */      /* parameter Events */
        &__midl_frag15,
        { 
        /* Events */
            1,
            1,
            0,
            0,
            1,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* MustSize, MustFree, [out] */
        (NDR64_UINT16) 0 /* 0x0 */,
        72 /* 0x48 */,   /* Stack offset */
    },
    { 
    /* long */      /* parameter long */
        &__midl_frag31,
        { 
        /* long */
            0,
            0,
            0,
            0,
            1,
            1,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [out], IsReturn, Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        80 /* 0x50 */,   /* Stack offset */
    }
};

static const __midl_frag1_t __midl_frag1 =
(NDR64_UINT32) 0 /* 0x0 */;
#ifdef __cplusplus
}
#endif


#include "poppack.h"


static const FormatInfoRef Sandbox_Ndr64ProcTable[] =
    {
    &__midl_frag2,
    &__midl_frag26
    };


#ifdef __cplusplus
namespace {
#endif
static const MIDL_STUB_DESC Sandbox_StubDesc = 
    {
    (void *)& Sandbox___RpcServerInterface,
    MIDL_user_allocate,
    MIDL_user_free,
    0,
    0,
    0,
    0,
    0,
    sandbox__MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x60001, /* Ndr library version */
    0,
    0x8010274, /* MIDL Version 8.1.628 */
    0,
    0,
    0,  /* notify & notify_flag routine table */
    0x2000001, /* MIDL flag */
    0, /* cs routines */
    (void *)& Sandbox_ServerInfo,   /* proxy/server info */
    0
    };
#ifdef __cplusplus
}
#endif

static const RPC_DISPATCH_FUNCTION Sandbox_NDR64__table[] =
    {
    NdrServerCallAll,
    NdrServerCallAll,
    0
    };
static const RPC_DISPATCH_TABLE Sandbox_NDR64__v1_0_DispatchTable = 
    {
    2,
    (RPC_DISPATCH_FUNCTION*)Sandbox_NDR64__table
    };

static const MIDL_SYNTAX_INFO Sandbox_SyntaxInfo [  2 ] = 
    {
    {
    {{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}},
    (RPC_DISPATCH_TABLE*)&Sandbox_v1_0_DispatchTable,
    sandbox__MIDL_ProcFormatString.Format,
    Sandbox_FormatStringOffsetTable,
    sandbox__MIDL_TypeFormatString.Format,
    0,
    0,
    0
    }
    ,{
    {{0x71710533,0xbeba,0x4937,{0x83,0x19,0xb5,0xdb,0xef,0x9c,0xcc,0x36}},{1,0}},
    (RPC_DISPATCH_TABLE*)&Sandbox_NDR64__v1_0_DispatchTable,
    0 ,
    (unsigned short *) Sandbox_Ndr64ProcTable,
    0,
    0,
    0,
    0
    }
    };


static const SERVER_ROUTINE Sandbox_ServerRoutineTable[] = 
    {
    (SERVER_ROUTINE)RpcSendFrame,
    (SERVER_ROUTINE)RpcSetCursorShape
    };

static const MIDL_SERVER_INFO Sandbox_ServerInfo = 
    {
    &Sandbox_StubDesc,
    Sandbox_ServerRoutineTable,
    sandbox__MIDL_ProcFormatString.Format,
    (unsigned short *) Sandbox_FormatStringOffsetTable,
    0,
    (RPC_SYNTAX_IDENTIFIER*)&_NDR64_RpcTransferSyntax_1_0,
    2,
    (MIDL_SYNTAX_INFO*)Sandbox_SyntaxInfo
    };
#if _MSC_VER >= 1200
#pragma warning(pop)
#endif


#endif /* defined(_M_AMD64)*/

