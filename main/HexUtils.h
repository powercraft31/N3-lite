#ifndef __HEX_UTILS_H__
#define __HEX_UTILS_H__
#include "types.h"
#include <string.h>
#include <stdio.h>

//判断本机是大端不是，是返回true,不是返回 false
int is_big_endian();
//int类型的数据大小端逆序
unsigned int reverse_order_long (unsigned int long_bytes);
//unsigned short 类型的大小端逆序
unsigned short reverse_order_short (unsigned short short_bytes);
//类似于linux 的htonl 本机字节序转换成网络字节序unsigned int 
unsigned int Htonl (unsigned int hostlong);
//类似于linux 的htons 本机字节序转换成网络字节序unsigned short 
unsigned short Htons (unsigned short hostshort);
//类似于linux 的ntohl 网络字节序转换成本机字节序unsigned int 
unsigned int Ntohl (unsigned int netlong);
//类似于linux 的ntohs 网络字节序转换成本机字节序unsigned short
unsigned short Ntohs (unsigned short netshort);
//网络转到本地字节序unsigned short netshort 类型
unsigned short network_to_host_short(unsigned short netshort);
//本地字节序转到网络unsigned short netshort 类型
unsigned short host_to_network_short(unsigned short hostshort);
//网络转到本地字节序unsigned int 类型
unsigned int network_to_host_int(unsigned int netshort);
//本地字节序转到网络unsigned int hostshort 类型
unsigned int host_to_network_int(unsigned int hostshort);


//类型转换函数
unsigned short uchar_to_ushort(unsigned char *pChar);
short char_to_short(char *pChar);
BOOL ushort_to_uchar(unsigned short num,unsigned char *pOutChar);
unsigned int uchar_to_uint(unsigned char *pChar);
int char_to_int(char *pChar);

BOOL uInt_to_uchar(unsigned int num,unsigned char *pChar);
//char -> float 
float char_to_float(char *pChar);
//float ->char
BOOL float_to_char(float fvalue,char *pChar);
//大小端转换函数大端转换
//uchar ->ushort 
unsigned short big_uchar_to_ushort(unsigned char *pChar);
//ushort->uchar  
BOOL big_ushort_to_uchar(unsigned short num,unsigned char *pOutChar);
//char -> short

short big_char_to_short(char *pChar);

//uchar -> uint
unsigned int big_uchar_to_uint(unsigned char *pChar);
//char ->int
int big_char_to_int(char *pChar);

//uint ->uchar
BOOL big_uint_to_uchar(unsigned int num,unsigned char *pChar);
//bigchar -> float 
float big_char_to_float(char *pChar);
//float ->bigchar
BOOL big_float_to_char(float fvalue,char *pChar);

#endif
