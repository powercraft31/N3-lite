#include "Crc16Xom.h"
#include "StringUtils.h"
#include "DeBug.h"
#include "HexUtils.h"

unsigned short modbusCRC16(unsigned char *data_value,unsigned short data_length)
{
    short i;
    unsigned int crc_value=0xffff;
    while(data_length--)
    {
        crc_value ^= *data_value++;
        for(i=0;i<8;i++)
        {
            if(crc_value&0x0001)
            crc_value=(crc_value>>1)^0xa001;
            else
            crc_value=crc_value>>1;
            }
    }
    return(crc_value);
}
BOOL modbusCRC16StdToString(char *str)
{
	unsigned char out[128] = {0};
	unsigned int outlen;
	int strLen = strlen(str);
	//dPrint(DEBUG,"modbusCRC16StdToString str:%s, strlen=%d\n",str, strLen);

	// 去除非十六进制字符（保留 0-9, A-F, a-f）
	char cleaned[256] = {0};
	int cleanedIdx = 0;
	for(int i = 0; i < strLen; i++) {
		char c = str[i];
		if((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
			cleaned[cleanedIdx++] = c;
		}
	}
	cleaned[cleanedIdx] = '\0';

	//dPrint(DEBUG,"Cleaned string: %s, length=%d\n", cleaned, cleanedIdx);

	// 检查清理后的字符串长度是否为偶数
	if(cleanedIdx % 2 != 0) {
		dPrint(DERROR,"Hex string length is odd after cleaning: %d\n", cleanedIdx);
		return FALSE;
	}

	StrToHexArray(cleaned, out, &outlen);
	//dPrint(DEBUG,"After StrToHexArray: outlen=%d\n", outlen);
	//HexPrint(out,outlen);
	if(outlen <2)
	{
		dPrint(DERROR,"modbusCRC16StdToString error\n");
		return FALSE;
	}
	unsigned short crc16Data = modbusCRC16(out,outlen-2);
	//dPrint(DEBUG,"modbus crc16 = %d\n", crc16Data);
	char crcBuff[2] = {0};
	big_ushort_to_uchar(crc16Data,(unsigned char *)crcBuff);
	out[outlen-2] = crcBuff[0];
	out[outlen-1] = crcBuff[1];
	//再转为字符串
    memset(str,0,strlen(str));
	HexArrayToStr(str,out,outlen);
	//HexPrint(out,outlen);
	ToUpperCase(str);
	//dPrint(DEBUG,"modbusCRC16StdToString ToUpperCase:%s\n",str);
	return TRUE;
}