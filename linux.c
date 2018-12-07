#ifndef WIN32

#include "linux.h"

cchar* itoa(int value, cchar* result, int base) {
	cchar* ptr = result, *ptr1 = result, tmp_char;
	int tmp_value;

	if (base < 2 || base > 36) { *result = '\0'; return result; }

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
	} while ( value );

	if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while(ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr--= *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}

void Sleep(int _ms)
{
#if defined(SP5KOS_THREAD)
	APP_SLEEP_MS(50);
#else
	usleep(_ms * 1000);
#endif
}

inline cchar *_itoa_s(int _Value, cchar * _DstBuf, int _Size, int _Radix)
{
	return itoa(_Value, _DstBuf, _Radix);
}

inline cchar *___gcvt(double _val, int _numOfDigits, cchar *_result)
{
	sprintf(_result, "%f", _val);
	return _result;
}

inline cchar *_gcvt_s(cchar *_buf, int _bufSize, double _Value, int _NumOfDigits)
{
	return ___gcvt(_Value, _NumOfDigits, _buf);
	return _buf;
}

#endif

