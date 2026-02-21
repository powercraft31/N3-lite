#include "StringUtils.h"
#include <ctype.h>

int StrToHexArray(char *str, unsigned char *out, unsigned int *outlen)
{
	    char *p = str;
	    char high = 0, low = 0;
	    int tmplen = strlen(p), cnt = 0;
	    tmplen = strlen(p);
	    while(cnt < (tmplen / 2))
	    {
	        high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
	 	 low = (*(++ p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
	        out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
	        p ++;
	        cnt ++;
	    }
	    if(tmplen % 2 != 0) out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
	   
	    if(outlen != NULL) *outlen = tmplen / 2 + tmplen % 2;
	    return tmplen / 2 + tmplen % 2;
}

void HexArrayToStr(char *ptr,unsigned char *buf,int len)
{
	for(int i = 0; i < len; i++)
	{
		sprintf(ptr, "%02x",buf[i]);
		ptr += 2;
	}
}

void RemoveWhitespace(char *str)
{
    char *dest = str;
    while (*str) {
        if (*str != ' ' && *str != '\t' && *str != '\n') {
            *dest++ = *str;
        }
        str++;
    }
    *dest = '\0';
}
int extract_number(const char *json_str, const char *key) 
{
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    const char *ptr = strstr(json_str, search_pattern);
    if (ptr == NULL) return -1;
    
    ptr += strlen(search_pattern);
    while (*ptr && !isdigit((unsigned char)*ptr) && *ptr != '-') ptr++;
    
    int num = 0;
    int sign = 1;
    
    if (*ptr == '-') {
        sign = -1;
        ptr++;
    }
    
    while (*ptr && isdigit((unsigned char)*ptr)) {
        num = num * 10 + (*ptr - '0');
        ptr++;
    }
    
    return num * sign;
}
char* extract_string(const char *json_str, const char *key, char *output, int max_len) 
{
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":\"", key);
    
    const char *ptr = strstr(json_str, search_pattern);
    if (ptr == NULL) {
        output[0] = '\0';  // ??????????
        return NULL;
    }
    
    ptr += strlen(search_pattern);  // ?????????
    
    int i = 0;
    // ??????????????????
    while (*ptr && *ptr != '"' && i < max_len - 1) {
        output[i++] = *ptr++;
    }
    output[i] = '\0';  // ????????
    
    return output;
}

float extract_float(const char *json_str, const char *key)
{
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);

    const char *ptr = strstr(json_str, search_pattern);
    if (ptr == NULL) {
        return 0.0f;
    }

    ptr += strlen(search_pattern);

    // 跳过空格
    while (*ptr && (*ptr == ' ' || *ptr == '\t')) {
        ptr++;
    }

    // 使用sscanf解析浮点数
    float value = 0.0f;
    if (sscanf(ptr, "%f", &value) == 1) {
        return value;
    }

    return 0.0f;
}

// 函数定义
char** Split(const char* str, char delimiter, int* count)
{
    const int MAX_PARTS = 100; // 最大分割数，根据需要调整
    char** parts = malloc(MAX_PARTS * sizeof(char*)); // 分配指针数组的内存
    *count = 0; // 初始化计数器为0
    char* temp = strdup(str); // 复制字符串以便分割

    // 将单个字符转换为以null结尾的字符串
    char delim_str[2] = {delimiter, '\0'};
    char* token = strtok(temp, delim_str); // 使用strtok分割字符串

    while (token != NULL && *count < MAX_PARTS) {
        parts[*count] = strdup(token); // 为每个部分分配内存并复制内容
        (*count)++; // 增加计数器
        token = strtok(NULL, delim_str); // 获取下一个token
    }
    free(temp); // 释放复制的字符串的内存
    return parts; // 返回指针数组和计数器值
}

void SplitTwoString(const char *source, const char *delimiter, char *part1, char *part2) 
{
    char *temp;
    // 查找分隔符
    temp = strstr(source, delimiter);
    if (temp != NULL) {
        // 分隔符之前的部分
        strncpy(part1, source, temp - source);
        part1[temp - source] = '\0'; // 确保part1以null终止
        // 分隔符之后的部分（不包括分隔符）
        strcpy(part2, temp + strlen(delimiter));
    } else {
        // 如果找不到分隔符，则整个字符串是第一部分，第二部分为空字符串
        strcpy(part1, source);
        part2[0] = '\0'; // 确保第二部分为空字符串
    }
    return;
}

void SubString(char *dest, const char *src, size_t start, size_t length) 
{
    if (start >= strlen(src) || length == 0) {
        *dest = '\0';  // 如果起始位置超出长度或长度为0，则返回空字符串
        return;
    }
    size_t src_len = strlen(src);
    if (start + length > src_len) {
        length = src_len - start;  // 如果请求的长度超出实际长度，则调整长度
    }
    strncpy(dest, src + start, length);
    dest[length] = '\0';  // 确保目标字符串以空字符结尾
}

void RemoveBeforeComma(char* request,char delimiter) 
{
    char* commaPos = strchr(request, delimiter);
    if (commaPos != NULL) {
        size_t remainingLen = strlen(commaPos + 1);
        memmove(request, commaPos + 1, remainingLen + 1);
    }
    return;
}
/**
 * 从原始字符串中删除所有出现的指定子串
 * @param str 原始字符串（会被修改）
 * @param substr 要删除的子串
 * @return 删除操作后的字符串长度，如果未找到子串返回原长度
 */
int RemoveSubstring(char *str, const char *substr) 
{
    if (str == NULL || substr == NULL || *substr == '\0') {
        return -1; // 无效参数
    }
    
    int count = 0; // 记录删除次数
    size_t subLen = strlen(substr);
    size_t strLen = strlen(str);
    
    if (subLen > strLen) {
        return strLen; // 子串比原串长，无需处理
    }
    
    char *pos = str;
    while ((pos = strstr(pos, substr)) != NULL) {
        // 计算要移动的字节数
        size_t remainingLen = strlen(pos + subLen);
        
        // 使用memmove移动内存（处理内存重叠）
        memmove(pos, pos + subLen, remainingLen + 1); // +1 包含结束符
        
        count++;
    }
    
    return strlen(str);
}
/**
 * 从字符串末尾删除指定长度的字符
 * @param str 原始字符串（会被修改）
 * @param n 要删除的字符数量
 * @return 删除后的字符串长度，如果n大于字符串长度则返回0
 */
size_t TruncateFromEnd(char *str, size_t n) 
{
    if (str == NULL) {
        return 0;
    }
    
    size_t len = strlen(str);
    
    // 如果要删除的长度大于等于字符串长度，直接清空字符串
    if (n >= len) {
        str[0] = '\0';
        return 0;
    }
    
    // 从末尾删除n个字符
    str[len - n] = '\0';
    return len - n;
}
// 函数定义
void ToUpperCase(char *str) 
{
    while (*str) 
    {   
        // 遍历字符串直到遇到空字符'\0'
        *str = toupper((unsigned char) *str); // 将小写字母转换为大写
         str++; // 移动到下一个字符
    }
}
/**
 * 字符串替换函数 - 直接修改原始字符串
 * @param original: 原始字符串（将被修改）
 * @param start: 开始替换的位置（从0开始）
 * @param length: 要替换的字符长度
 * @param replacement: 替换字符串
 * @return: 成功返回0，失败返回-1
 */
int StringReplace(char* original, int start, int length, const char* replacement) 
{
    // 参数有效性检查
    if (original == NULL || replacement == NULL) 
    {
        printf("错误：输入字符串不能为NULL\n");
        return -1;
    }
    
    int len_original = strlen(original);
    int len_replacement = strlen(replacement);
    
    // 检查起始位置是否有效
    if (start < 0 || start >= len_original) 
    {
        printf("错误：起始位置 %d 超出字符串范围 [0, %d]\n", start, len_original - 1);
        return -1;
    }
    
    // 检查替换长度是否有效
    if (length < 0) 
    {
        printf("错误：替换长度不能为负数\n");
        return -1;
    }
    
    // 计算实际要替换的长度（防止超出原字符串边界）
    int actual_length = length;
    if (start + length > len_original) 
    {
        actual_length = len_original - start;
    }
    
    // 检查替换后的总长度是否会超出原字符串缓冲区
    int remaining_length = len_original - (start + actual_length);
    int new_total_length = start + len_replacement + remaining_length;
    
    if (new_total_length >= len_original) 
    {
        // 如果替换后长度超过原字符串缓冲区，进行安全替换
        // 计算可以安全替换的最大长度
        int max_safe_length = len_original - start;
        if (len_replacement > max_safe_length) 
        {
            // 只能替换部分内容
        memmove(original + start, replacement, max_safe_length);
        printf("警告：替换字符串过长，只替换了前 %d 个字符\n", max_safe_length);
        return 0;
        }
    }
    
    // 执行替换操作
    // 1. 如果需要，先移动原字符串的剩余部分
    if (len_replacement != actual_length) 
    {
        int move_offset = len_replacement - actual_length;
        memmove(original + start + len_replacement, 
                original + start + actual_length, 
                remaining_length + 1); // +1 包含结束符
    }
    
    // 2. 复制替换字符串到指定位置
    memcpy(original + start, replacement, len_replacement);
    return 0;
}
void StrDelSpaceWrap(char *str)
{
    if (str == NULL) return;  // 如果传入的是空指针，直接返回

    char *read = str;  // 用于读取的指针
    char *write = str; // 用于写入的指针，初始化和读指针相同

    while (*read) { // 遍历字符串直到遇到空字符
        if (*read != ' ' && *read != '\n' && *read != '\r' && *read != '\t') {
            // 如果当前字符不是空格、换行、回车或制表符，则复制到write指针的位置
            *write++ = *read;
        }
        read++; // 移动读指针
    }
    *write = '\0'; // 在字符串末尾添加空字符，表示字符串结束
}

char* delete_chars_from_position(char* str, int start_pos, int delete_length) 
{
    // 参数有效性检查
    if (str == NULL || start_pos < 0 || delete_length <= 0) {
        return str;
    }
    
    int len = strlen(str);
    
    // 检查起始位置是否超出字符串长度
    if (start_pos >= len) {
        return str;
    }
    
    // 计算实际要删除的长度
    int actual_delete_length = delete_length;
    if (start_pos + delete_length > len) {
        actual_delete_length = len - start_pos;
    }
    
    // 如果实际删除长度为0，直接返回
    if (actual_delete_length == 0) {
        return str;
    }
    
    // 移动字符覆盖要删除的部分
    int source_index = start_pos + actual_delete_length;
    int target_index = start_pos;
    
    // 将删除位置后的字符前移
    while (source_index <= len) {
        str[target_index] = str[source_index];
        target_index++;
        source_index++;
    }
    
    return str;
}
//在字符串前面加上指定字符串
void prepend_string(char *dest, const char *src) 
{
    // 计算src的长度
    size_t src_len = strlen(src);
    // 计算dest当前内容的长度（不包括结尾的null字符）
    size_t dest_len = strlen(dest);
    // 确保dest有足够的空间来存储src和原始内容
    char temp[src_len + dest_len + 1]; // +1为结尾的null字符
    
    // 将src复制到temp的前面部分
    strncpy(temp, src, src_len);
    // 将dest复制到temp的后面部分
    strcpy(temp + src_len, dest);
    
    // 将temp的内容复制回dest（这会覆盖原始的dest内容）
    strcpy(dest, temp);
}
/**
 * 从字符串末尾开始截取指定长度的子串
 * @param dest 目标缓冲区（由调用者分配）
 * @param src 源字符串
 * @param start_from_end 从末尾开始的位置（从1开始计数）
 * @param length 要截取的长度
 * @return 成功返回0，失败返回-1
 */
int substr_from_end(char* dest, const char* src, int start_from_end, int length) {
    // 参数验证
    if (dest == NULL || src == NULL || start_from_end <= 0 || length < 0) {
        return -1;
    }
    
    // 获取源字符串长度
    size_t src_len = strlen(src);
    
    // 如果源字符串为空或起始位置超出字符串长度
    if (src_len == 0 || start_from_end > src_len) {
        dest[0] = '\0';
        return -1;
    }
    
    // 计算从末尾开始的起始索引（转换为从0开始的索引）
    size_t start_index = src_len - start_from_end;
    
    // 确保截取长度不超过可用长度
    size_t actual_length = length;
    if (start_index + length > src_len) {
        actual_length = src_len - start_index;
    }
    
    // 执行截取操作
    strncpy(dest, src + start_index, actual_length);
    dest[actual_length] = '\0'; // 确保字符串以null结尾
    
    return 0;
}
/*
@删除出现的所有子串
‌strstr‌：用于在字符串中查找第一次出现的指定子串。
‌memmove‌：用于将内存块从一个位置复制到另一个位置，可以处理重叠内存区域。这里用于将找到的子串之后的内容向前移动，覆盖该子串。
‌len_remove‌：是要删除的子串的长度。
‌len_buff‌：当前缓冲区（buff）的总长度。
‌len_rest‌：每次找到子串后，剩余未处理的字符串的长度。
*/
void remove_substring_preserve(char *buff, const char *to_remove) 
  {
      if (buff == NULL || to_remove == NULL) return;

      char *pos;
      int len_remove = strlen(to_remove);

      while ((pos = strstr(buff, to_remove)) != NULL) {
          // 计算从找到的位置到字符串末尾的长度（包括\0）
          int len_rest = strlen(pos + len_remove) + 1;
          // 将后面的内容向前移动，覆盖要删除的子串
          memmove(pos, pos + len_remove, len_rest);
      }
  }
