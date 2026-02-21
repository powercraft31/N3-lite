#ifndef __VERIFICATION_H__
#define __VERIFICATION_H__
#include <string.h>
#include "types.h"




unsigned short modbusCRC16(unsigned char *data_value,unsigned short data_length);

BOOL modbusCRC16StdToString(char *str);

#endif
