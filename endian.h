#ifndef _ENDIAN_H_
#define _ENDIAN_H_

// #define EMUHOST_BIG_ENDIAN

/* Little-endian native platform, but big-endian simulated emulator */
#if !defined(__BIG_ENDIAN__) && defined(EMUHOST_BIG_ENDIAN)
#define VIRTUAL_BE
#endif

/* Direct memory access allowed
   Big-endian processor + big-endian emulator memory = YES
   Little-endian processor + little-endian emulator memory = YES
*/
#if (defined(__BIG_ENDIAN__) && defined(VIRTUAL_BE)) || (!defined(__BIG_ENDIAN__) && !defined(EMUHOST_BIG_ENDIAN))
#define DIRECT_MEMORY 1
#else
#define DIRECT_MEMORY 0
#endif

#define ENDIAN_SWAP_BYTE    (~0U & 7U & 3U)
#define ENDIAN_SWAP_HALF    (~0U & 6U & 2U)
#define ENDIAN_SWAP_BIMI    (~0U & 5U & 1U)
#define ENDIAN_SWAP_WORD    (~0U & 4U & 0U)

#define ADDR_SWAP_BYTE(address)    ((address) ^ ENDIAN_SWAP_BYTE)
#define ADDR_SWAP_HALF(address)    ((address) ^ ENDIAN_SWAP_HALF)
#define ADDR_SWAP_BIMI(address)    ((address) ^ ENDIAN_SWAP_BIMI)
#define ADDR_SWAP_WORD(address)    ((address) ^ ENDIAN_SWAP_WORD)

#if defined(__BIG_ENDIAN__) || defined(VIRTUAL_BE)
#define BES(address)    (address)
#define HES(address)    (address)
#define MES(address)    (address)
#define WES(address)    (address)
#else
#define BES(address)    ((address) ^ ENDIAN_SWAP_BYTE)
#define HES(address)    ((address) ^ ENDIAN_SWAP_HALF)
#define MES(address)    ((address) ^ ENDIAN_SWAP_BIMI)
#define WES(address)    ((address) ^ ENDIAN_SWAP_WORD)
#endif

/* This is to control how native pointer access works 
   Native big endian / big-endian emulator = no byteswapping
   Native little-endian / big-endian emulator = yes byteswapping
*/

// #if (DIRECT_MEMORY == 1)
// #define PTR_BYTE(address)    (address)
// #define PTR_HALF(address)    (address)
// #define PTR_BIMI(address)    (address)
// #define PTR_WORD(address)    (address)
// #else
#define PTR_BYTE(address)    ADDR_SWAP_BYTE(address)
#define PTR_HALF(address)    ADDR_SWAP_HALF(address)
#define PTR_BIMI(address)    ADDR_SWAP_BIMI(address)
#define PTR_WORD(address)    ADDR_SWAP_WORD(address)
// #endif

#define BYTESWAP_2(data) \
( (((data) >> 8) & 0x00FF) | (((data) << 8) & 0xFF00) ) 

#define BYTESWAP_4(data)   \
( (((data) >> 24) & 0x000000FF) | (((data) >>  8) & 0x0000FF00) | \
  (((data) <<  8) & 0x00FF0000) | (((data) << 24) & 0xFF000000) ) 


#if (DIRECT_MEMORY == 0)
#define REGVAL(x) BYTESWAP_4(x)
#define INSTWORD(x) BYTESWAP_4(x)
#define MEM16(x) BYTESWAP_2(x)
#define MEM32(x) BYTESWAP_4(x)
#else
#define REGVAL(x) (x)
#define INSTWORD(x) (x)
#define MEM16(x) (x)
#define MEM32(x) (x)
#endif

#endif // _ENDIAN_H_