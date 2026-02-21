#ifndef __STRING_UTILS_H__
#define __STRING_UTILS_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
// 字符串转16进制数组
int StrToHexArray(char *str, unsigned char *out, unsigned int *outlen);

/****************************************************************************
函数名: hex_to_str
功能: 16进制转字符串
参数: ptr 输出字符串指针 buf 输入数据 len 输入数据长度
返回: 无
*****************************************************************************/
void HexArrayToStr(char *ptr,unsigned char *buf,int len);
// 删除字符串中的空白字符
void RemoveWhitespace(char *str);
// 从json中提取整数值
int extract_number(const char *str,const char *prefix);
// 从json中提取字符串值
char* extract_string(const char *json_str, const char *key, char *output, int max_len);
// 从json中提取浮点数值
float extract_float(const char *json_str, const char *key);

/********************************************************
	*@Function name:Split
	*@Description:根据指定的字符，分割原始字符串，
	*@input param:str 原始字符串
	*@input param:delimiter 分割符
    *@output param:count 分割出来多少个字符串
	*@Return:字符串数组指针
********************************************************************************/
char** Split(const char* str, char delimiter, int* count);
/********************************************************
	*@Function name:SplitTwoString
	*@Description:将指定的原始字符串，根据分割符分割为2部分
	*@input param:source 原始字符串
	*@input param:delimiter 分割符
    *@output param:part1 字符串1
    *@output param:part2 字符串2
	*@Return:字符串数组指针
********************************************************************************/
void SplitTwoString(const char *source, const char *delimiter, char *part1, char *part2);

/********************************************************
	*@Function name:SubString
	*@Description:截取指定的字符串
	*@input param:src 原始字符串
	*@input param:start 开始位置
    *@input param:length 长度
    *@dest param:dest 截取后的字符串
	*@Return:
********************************************************************************/
void SubString(char *dest, const char *src, size_t start, size_t length);
/********************************************************
	*@Function name:RemoveBeforeComma
	*@Description:从第0个字节的位置开始删除，一直删除到delimiter分割符
	*@input param:request 原始字符串
	*@input param:delimiter 分割符
    *@dest param:dest 截取后的字符串
	*@Return:
********************************************************************************/
void RemoveBeforeComma(char* request,char delimiter);
/**
 * 从原始字符串中删除所有出现的指定子串
 * @param str 原始字符串（会被修改）
 * @param substr 要删除的子串
 * @return 删除操作后的字符串长度，如果未找到子串返回原长度
 */
int RemoveSubstring(char *str, const char *substr);
/**
 * 从字符串末尾删除指定长度的字符
 * @param str 原始字符串（会被修改）
 * @param n 要删除的字符数量
 * @return 删除后的字符串长度，如果n大于字符串长度则返回0
 */
size_t TruncateFromEnd(char *str, size_t n);

// 函数定义
/********************************************************
	*@Function name:ToUpperCase
	*@Description:将存在的全部小写字母转换为大写字母
	*@input param:str 原始字符串
	*@Return:
********************************************************************************/
void ToUpperCase(char *str);
/**
 * 字符串替换函数 - 直接修改原始字符串
 * @param original: 原始字符串（将被修改）
 * @param start: 开始替换的位置（从0开始）
 * @param length: 要替换的字符长度
 * @param replacement: 替换字符串
 * @return: 成功返回0，失败返回-1
 */
int StringReplace(char* original, int start, int length, const char* replacement);

/**
 * 删除所有空格换行/制表符
 * @param str: 原始字符串（将被修改）
 */
void StrDelSpaceWrap(char *str);

/**
 * 从字符串指定位置开始删除指定长度的字符
 * @param str 原始字符串（会被直接修改）
 * @param start_pos 开始删除的位置（从0开始计数）
 * @param delete_length 要删除的字符长度
 * @return 修改后的字符串指针
 */
char* delete_chars_from_position(char* str, int start_pos, int delete_length);

//在字符串前面加上指定字符串
void prepend_string(char *dest, const char *src);
/**
 * 从字符串末尾开始截取指定长度的子串
 * @param dest 目标缓冲区（由调用者分配）
 * @param src 源字符串
 * @param start_from_end 从末尾开始的位置（从1开始计数）
 * @param length 要截取的长度
 * @return 成功返回0，失败返回-1
 */
int substr_from_end(char* dest, const char* src, int start_from_end, int length);

/*
@删除出现的所有子串
‌strstr‌：用于在字符串中查找第一次出现的指定子串。
‌memmove‌：用于将内存块从一个位置复制到另一个位置，可以处理重叠内存区域。这里用于将找到的子串之后的内容向前移动，覆盖该子串。
‌len_remove‌：是要删除的子串的长度。
‌len_buff‌：当前缓冲区（buff）的总长度。
‌len_rest‌：每次找到子串后，剩余未处理的字符串的长度。
*/
void remove_substring_preserve(char *buff, const char *to_remove);

#endif
