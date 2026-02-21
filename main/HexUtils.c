#include "HexUtils.h"

int is_big_endian()
{
    int i = 1;
    char c = (*(char*)&i);
    if (c)
    {
         //printf("this is litt小端\n");
         return 0;
    }
    else
    {
        //printf("this is big大端\n");
        return 1;
    }

}

unsigned int reverse_order_long (unsigned int long_bytes)
{
    unsigned int ret;
    char *dest_addr = (char *) &ret;
    char *src_addr = (char *) &long_bytes;

    dest_addr[0] = src_addr[3];
    dest_addr[1] = src_addr[2];
    dest_addr[2] = src_addr[1];
    dest_addr[3] = src_addr[0];
    return ret;
}

unsigned short reverse_order_short (unsigned short short_bytes)
{
    unsigned short ret;
    char *dest_addr = (char *) &ret;
    char *src_addr = (char *) &short_bytes;
    dest_addr[0] = src_addr[1];
    dest_addr[1] = src_addr[0];
    return ret;
}

unsigned int Htonl (unsigned int hostlong)
{
    if (is_big_endian())
    {
        return hostlong;
    }
    return reverse_order_long (hostlong);
}

unsigned short Htons (unsigned short hostshort)
{
    if (is_big_endian())
    {
        return hostshort;
    }
    return reverse_order_short (hostshort);
}

unsigned int Ntohl (unsigned int netlong)
{
    if (is_big_endian())
    {
        return netlong;
    }
    return reverse_order_long (netlong);
}

unsigned short Ntohs (unsigned short netshort)
{
    if (is_big_endian())
    {
        return netshort;
    }
    return reverse_order_short (netshort);
}

unsigned short network_to_host_short(unsigned short netshort)
{
    return Ntohs(netshort);
}
unsigned short host_to_network_short(unsigned short hostshort)
{
    return Ntohs(hostshort);
}

unsigned int network_to_host_int(unsigned int netshort)
{
    return Ntohl(netshort);
}
unsigned int host_to_network_int(unsigned int hostshort)
{
    return Ntohl(hostshort);
}

 //类型转换函数
unsigned short uchar_to_ushort(unsigned char *pChar)
{
 	unsigned short tshort;
	unsigned char *p = (unsigned char *)&tshort;
	p[0] = pChar[0];
	p[1] = pChar[1];
	return tshort;
}
 short char_to_short(char *pChar)
{
 	short tshort;
	char *p = (char *)&tshort;
	p[0] = pChar[0];
	p[1] = pChar[1];
	return tshort;
}
 
BOOL ushort_to_uchar(unsigned short num,unsigned char *pOutChar)
{
 	unsigned char *p = (unsigned char *)&num;
	pOutChar[0] = p[0];
	pOutChar[1] = p[1];
	return TRUE;
}
unsigned int uchar_to_uint(unsigned char *pChar)
{
 	unsigned int tIntNum;
	unsigned char *p = (unsigned char *)&tIntNum;
	for(int i=0;i<(int)sizeof(int);i++)
	{
		p[i] = pChar[i];
	}
	return tIntNum;
}
int char_to_int(char *pChar)
{
 	int tIntNum;
	char *p = (char *)&tIntNum;
	for(int i=0;i<(int)sizeof(int);i++)
	{
		p[i] = pChar[i];
	}
	return tIntNum;
}
BOOL uInt_to_uchar(unsigned int num,unsigned char *pChar)
{
 	unsigned char *p = (unsigned char *)&num;
	for(int i =0;i<(int)sizeof(int);i++)
	{
		
		pChar[i] = p[i];
	}

	return TRUE;
}
//char -> float 
float char_to_float(char *pChar)
{
	float fvalue = 0;
	memcpy((char *)&fvalue,pChar,sizeof(float));
	return fvalue;
}
//float ->char
BOOL float_to_char(float fvalue,char *pChar)
{
	char * pfchar = (char *)&fvalue;
	memcpy(pChar,pfchar,sizeof(float));
	return TRUE;
}
//大小端转换函数
unsigned short big_uchar_to_ushort(unsigned char *pChar)
{
	unsigned short tshort;
	unsigned char *p = (unsigned char *)&tshort;
	p[0] = pChar[1];
	p[1] = pChar[0];
	return tshort;
}
BOOL big_ushort_to_uchar(unsigned short num,unsigned char *pOutChar)
{
	unsigned char *p = (unsigned char *)&num;
	pOutChar[0] = p[1];
	pOutChar[1] = p[0];
	return TRUE;
}
short big_char_to_short(char *pChar)
{
	short tshort;
	char *p = (char *)&tshort;
	p[0] = pChar[1];
	p[1] = pChar[0];
	return tshort;
}
unsigned int big_uchar_to_uint(unsigned char *pChar)
{
	unsigned int tIntNum;
	unsigned char *p = (unsigned char *)&tIntNum;
	for(int i=0;i<(int)sizeof(int);i++)
	{
		p[i] = pChar[sizeof(int) - i-1];
	}
	return tIntNum;
}
int big_char_to_int(char *pChar)
{
	int tIntNum;
	char *p = (char *)&tIntNum;
	for(int i=0;i<(int)sizeof(int);i++)
	{
		p[i] = pChar[sizeof(int) - i-1];
	}
	return tIntNum;
}
BOOL big_uint_to_uchar(unsigned int num,unsigned char *pChar)
{
	unsigned char *p = (unsigned char *)&num;
	for(int i =0;i<(int)sizeof(int);i++)
	{
		pChar[i] = p[sizeof(int) - i-1];
	}

	return TRUE;
}
//bigchar -> float 
float big_char_to_float(char *pChar)
{
	float fvalue = 0;
	char *pfchar = (char *)&fvalue;
	for(int i = 0;i<sizeof(float);i++)
	{
		pfchar[i] = pChar[sizeof(float)-i-1];
	}
	
	return fvalue;
}
//float ->bigchar
BOOL big_float_to_char(float fvalue,char *pChar)
{
	char *pfchar = (char *)&fvalue;
	for(int i = 0;i<sizeof(float);i++)
	{
		pChar[i] = pfchar[sizeof(float)-i-1];
	}
	return TRUE;
}