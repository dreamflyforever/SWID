#include <assert.h>
#include <stdio.h>
#include "util.h"
#include "common.h"
#include "memory.h"

#ifdef WIN32
#include <windows.h>
#elif defined(FWL_THREAD)
#elif (defined(STM32_M4_THREAD) || defined(STM32_F103_THREAD))
#include <time.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#endif

#ifdef NO_INLINE
#define inline
#endif

const cchar hexChars[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

unsigned long prePerforceTime = 0;
unsigned long currPerforceTime = 0;

inline void asign(void *_pdes, const void *_psource, int _sizeofEle)
{
	if (_pdes == NULL)return;
	if (_sizeofEle == sizeof(void *))
	{
		if (_psource == NULL)
			*((void **)_pdes) = NULL;
		else
		{
			*((void **)_pdes) = *((void **)_psource);

#ifdef WIN32
			assert(memcmp(_pdes, _psource, _sizeofEle) == 0);
#endif
		}
	}
	else
	{
		memcpy(_pdes, _psource, _sizeofEle);
	}
}

inline long long __llmin(long long _x, long long _y)
{
	long long x = _x, y = _y;
	return (x < y)?x:y;
}

inline long long __llmax(long long _x, long long _y)
{
	long long x = _x, y = _y;
	return (x > y)?x:y;
}

inline void swapi(int _left, int _right)
{
	int tmp = _left;
	_left = _right;
	_right = tmp;
}

inline void swapui(unsigned int _left, unsigned int _right)
{
	unsigned int tmp = _left;
	_left = _right;
	_right = tmp;
}

inline void swapp(void * _left, void * _right)
{
	void * tmp = _left;
	_left = _right;
	_right = tmp;
}
inline const int myround(const double x) {
	return ((int)(x + 0.5));
}

int searchImin(int *_array, int _size)
{
	int minpos = -1;
	while (--_size>=0)
	{
		if(minpos < 0 || _array[_size] < _array[minpos])minpos = _size;
	}
	return minpos;
}

int searchFmin(float *_array, int _size)
{
	int minpos = -1;
	while (--_size>=0)
	{
		if(minpos < 0 || _array[_size] < _array[minpos])minpos = _size;
	}
	return minpos;
}

#if defined(STM32_M4_THREAD)
extern u32 getSTMF4TickCount();
#endif
unsigned long getTickCount2(void)
{
	unsigned long currentTime;
#ifdef WIN32
	currentTime = GetTickCount();
#elif defined(SP5KOS_THREAD)
	currentTime = appMsTimeGet();
#elif defined(FWL_THREAD)
	currentTime = 0;
#elif defined(STM32_M4_THREAD) || defined(STM32_F103_THREAD)
	currentTime = 0;
#else
	struct timeval current;
	gettimeofday(&current, NULL);
	currentTime = ((unsigned long)current.tv_sec) * 1000 + current.tv_usec/1000;
#endif
	return currentTime;
}

void mysleep(int _ms)
{
#ifdef WIN32
	Sleep(_ms);
#elif defined(SP5KOS_THREAD)
	APP_SLEEP_MS(_ms);
#elif defined(FWL_THREAD)
#elif defined(STM32_THREAD) || defined(STM32_M4_THREAD) || defined(STM32_F103_THREAD) || defined(SP5KOS_THREAD)

#elif defined(FREE_RTOS)
	vTaskDelay(_ms);
#else
	usleep(_ms * 1000);
#endif
}

unsigned long mytime()
{
#if defined(STM32_THREAD) || defined(STM32_M4_THREAD) || defined(STM32_F103_THREAD) || defined(SP5KOS_THREAD)
	return getTickCount2();
#else
	return time(NULL);
#endif
}

int fint_compare(const void * m, const void* n){  
	return *(int *)m - *(int *)n; 
}  

int ffloat_compare(const void * m, const void* n)
{

	float r = *(float *)m - *(float *)n;
	return (r > 0)?1:((r < 0)?-1:r);
}

int fshort_compare(const void * m, const void* n)
{
	return *(short *)m - *(short *)n; 
}

int mybinarySearch_(const void * _Key, const void * _Base, 
	unsigned int _NumOfElements, unsigned int _SizeOfElements,
	int (*_PtFuncCompare)(const void *, const void *))
{
	int lo = 0;
	int hi = _NumOfElements - 1;

	while (lo <= hi) {
		int mid = (lo + hi) >> 1;
		int compareVal = _PtFuncCompare(_Key, ((cchar *)_Base + _SizeOfElements * mid));

		if (compareVal > 0) {
			lo = mid + 1;
		} else if (compareVal < 0) {
			hi = mid - 1;
		} else {
			return mid;
		}
	}
	return ~lo;
}

int topNIdx(const void *_elements, int _numOfElements, int _sizeOfElements, 
	int (*_fCompare)(const void *, const void *), int *_results, int _resultN)
{
	int resultCount = 0;
	int i, j;
	cchar *element = NULL;
	for (i = 0; i < _numOfElements; i ++)
	{
		element = (cchar *)_elements + _sizeOfElements * i;
		for (j = resultCount - 1; j >= 0; j --)
		{
			if (_fCompare(element, (cchar *)_elements+_sizeOfElements*_results[j]) > 0)
			{
				if (j+1 < _resultN)
				{
					_results[j+1] = _results[j];
				}
			}
			else
				break;
		}

		if(j+1 < _resultN)
		{
			_results[j+1] = i;
			if (resultCount < _resultN)
			{
				resultCount ++;
			}
		}
	}

	return resultCount;
}

int topN(const void *_elements, int _numOfElements, int _sizeOfElements, 
	int (*_fCompare)(const void *, const void *), void **_results, int _resultN)
{

	int *results = (int *)_results;
	int r = topNIdx(_elements, _numOfElements, _sizeOfElements, _fCompare, results, _resultN);

	int i;
	for (i = r-1; i >= 0; i --)
	{
		_results[i] = (cchar *)_elements + _sizeOfElements*results[i];
	}
	return r;
}

static unsigned short      crctab[] =
{
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0

};

unsigned short calc_crc16(int *buf, unsigned short len)
{

    return calc_step_crc16(buf, len, 0xFFFF);
}

unsigned short calc_step_crc16(int *buf, unsigned short len, unsigned short _crc)
{
	unsigned short crc = _crc;
	unsigned short i;

	for (i=0; i<len; i++) {
		crc = ((crc << 8) & 0xFFFF) ^ crctab[(crc >> 8) ^ (buf[i] & 0xFF)];
	}

	return crc;
}

#ifndef MIN_SIZE
const unsigned short wCRCTalbeAbs[] =
{
	0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401, 0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400
};

unsigned short calc_crc16_2(int* pchMsg, unsigned short wDataLen)
{
	unsigned short wCRC = 0xFFFF;
	unsigned short i;
	unsigned char chChar;

	for (i = 0; i < wDataLen; i++)
	{
		chChar = ((*pchMsg++) & 0xFF);
		wCRC = wCRCTalbeAbs[(chChar ^ wCRC) & 15] ^ (wCRC >> 4);
		wCRC = wCRCTalbeAbs[((chChar >> 4) ^ wCRC) & 15] ^ (wCRC >> 4);
	}

	return wCRC;
}

unsigned short CRCCCITT(int* pDataIn, unsigned short iLenIn)
{
	unsigned short wTemp = 0;
	unsigned short wCRC = 0xffff;
	unsigned short i, j;
	for(i = 0; i < iLenIn; i++)
	{
		for(j = 0; j < 8; j++)
		{
			wTemp = (((pDataIn[i] & 0xff) << j) & 0x80 ) ^ ((wCRC & 0x8000) >> 8);
			wCRC <<= 1;
			if(wTemp != 0)
			{
				wCRC ^= 0x1021;
			}
		}
	}
	return wCRC;
}

typedef unsigned char uint8_t;

uint8_t CRC4_Table[16]= {0,13,7,10,14,3,9,4,1,12,6,11,15,2,8,5};

uint8_t crc4_cal(uint8_t *data,uint8_t len)
{
	uint8_t result  = 0x03;
	uint8_t tableNo = 0;
	int i = 0;
	for( ;i < len; i++)
	{
		tableNo = result ^ data[i];
		result  = CRC4_Table[tableNo];
	}
	return result;
}

unsigned char crc4_cal2(int *data, int len)
{
	uint8_t result  = 0x03;
	uint8_t tableNo = 0;
	int i = 0;
	for( ;i < len; i++)
	{
		tableNo = result ^ data[i];
		result  = CRC4_Table[tableNo];
	}
	return result;
}

uint8_t CRC6_Table[64]= { 0,  25, 50, 43, 61, 36, 15, 22, 35, 58, 17,  8, 30,  7, 44 ,53,
	31,  6, 45, 52, 34, 59, 16,  9, 60, 37, 14, 23,  1, 24, 51, 42,
	62, 39, 12, 21,  3, 26, 49, 40, 29,  4, 47, 54, 32, 57, 18, 11,
	33, 56, 19, 10, 28,  5, 46, 55,  2, 27, 48, 41, 63, 38, 13, 20};

uint8_t crc6_cal(uint8_t *data,uint8_t len)
{

	uint8_t result  = 0x15;

	uint8_t tableNo = 0;
	int i = 0;

	tableNo = result ^ 0;
	result  = CRC6_Table[tableNo];

	for(i = 0; i < len; i++)
	{
		tableNo = result ^ data[i];
		result  = CRC6_Table[tableNo];
	}

	return result;
}

typedef unsigned int uint_len;

uint8_t crc4_itu(uint8_t *data, uint_len length)  
{  
    uint8_t i;  
    uint8_t crc = 0;
    while(length--)  
    {  
        crc ^= *data++;
        for (i = 0; i < 8; ++i)  
        {  
            if (crc & 1)  
                crc = (crc >> 1) ^ 0x0C;
            else  
                crc = (crc >> 1);  
        }  
    }  
    return crc;  
}  

uint8_t crc5_epc(uint8_t *data, uint_len length)  
{  
    uint8_t i;  
    uint8_t crc = 0x48;
    while(length--)  
    {  
        crc ^= *data++;
        for ( i = 0; i < 8; i++ )  
        {  
            if ( crc & 0x80 )  
                crc = (crc << 1) ^ 0x48;
            else  
                crc <<= 1;  
        }  
    }  
    return crc >> 3;  
}  

uint8_t crc5_itu(uint8_t *data, uint_len length)  
{  
    uint8_t i;  
    uint8_t crc = 0;
    while(length--)  
    {  
        crc ^= *data++;
        for (i = 0; i < 8; ++i)  
        {  
            if (crc & 1)  
                crc = (crc >> 1) ^ 0x15;
            else  
                crc = (crc >> 1);  
        }  
    }  
    return crc;  
}  

uint8_t crc5_usb(uint8_t *data, uint_len length)  
{  
    uint8_t i;  
    uint8_t crc = 0x1F;
    while(length--)  
    {  
        crc ^= *data++;
        for (i = 0; i < 8; ++i)  
        {  
            if (crc & 1)  
                crc = (crc >> 1) ^ 0x14;
            else  
                crc = (crc >> 1);  
        }  
    }  
    return crc ^ 0x1F;  
}  
  
#endif

int hexChar2Int(cchar _c)
{
	if(_c >= '0' && _c <= '9')
	{
		return _c - '0';
	}
	else if(_c >= 'a' && _c <= 'z')
	{
		return _c - 'a' + 10;
	}
	else if(_c >= 'A' && _c <= 'Z')
	{
		return _c - 'A' + 10;
	}
	return -1;
}

int char64ToInt(cchar _c)
{
	if (_c >= 'A' && _c <= 'Z')
	{
		return _c - 'A';
	}
	else if (_c >= 'a' && _c <= 'z')
	{
		return _c - 'a' + 26;
	}
	else if(_c >= '0' && _c <= '9')
	{
		return _c - '0' + 52;
	}
	else if(_c == '_')
	{
		return 62;
	}
	else if(_c == '-')
	{
		return 63;
	}
	return -1;
}

cchar int2Char64(int _hex)
{
	if (_hex >= 0 && _hex < 26)
	{
		return 'A' + _hex;
	}
	else if (_hex >= 26 && _hex < 52)
	{
		return 'a' + _hex - 26;
	}
	else if (_hex >= 52 && _hex < 62)
	{
		return '0' + _hex - 52;
	}
	else if(_hex == 62)
	{
		return '_';
	}
	else if(_hex == 63)
	{
		return '-';
	}
	return -1;
}

inline void bitSet(cchar *_desBits, int _start, int _end, cchar _sourceBits)
{
	cchar x = (_start > 0?(0xff >> _start):0xff) & (_end < 8?(0xff << (8 - _end)):0xff);
	*_desBits = (*_desBits & ~x) | ((_sourceBits << (8 - _end)) & x);
}

inline cchar bitGet(cchar _sourceBits, int _start, int _end)
{
	return (_end < 8?(_sourceBits >> (8 - _end)):_sourceBits) & (0xff >> (8 - (_end - _start)));
}

void bitsSet(cchar *_bits, int _bitsStart, int _bitsEnd, cchar _c)
{
	int byteStart, byteEnd;
	int startPos, endPos;
	cchar startSource, endSource;
	if ((byteStart = _bitsStart/8) == (byteEnd = _bitsEnd/8))
	{
		bitSet(_bits + byteStart, _bitsStart%8, _bitsEnd%8, _c);
	}
	else
	{
		assert(byteEnd == byteStart + 1);
		startPos = _bitsStart%8;
		endPos = _bitsEnd%8;
		startSource = _c >> endPos;
		endSource = _c & (0xff >> (8 - endPos));
		bitSet(_bits+byteStart, startPos, 8, startSource);
		bitSet(_bits+byteEnd, 0, endPos, endSource);
	}
}

cchar bitsGet(cchar *_bits, int _bitsStart, int _bitsEnd)
{
	int byteStart, byteEnd;
	int startPos, endPos;
	cchar startSource, endSource;
	if ((byteStart = _bitsStart/8) == (byteEnd = _bitsEnd/8))
	{
		return bitGet(_bits[byteStart], _bitsStart%8, _bitsEnd%8);
	}
	else
	{
		assert(byteEnd == byteStart + 1);
		startPos = _bitsStart%8;
		endPos = _bitsEnd%8;
		startSource = bitGet(_bits[byteStart], startPos, 8);
		endSource = bitGet(_bits[byteEnd], 0, endPos);
		return startSource << endPos | endSource;
	}
}

#ifndef MIN_SIZE
typedef unsigned int UInt32;
typedef unsigned short UInt16;

#define WAVE_FORMAT_PCM     1
#define PCM_CHANNEL 1
#define PCM_BITPERSEC 16
#define PCM_SAMPLE DEFAULT_SAMPLE_RATE

typedef struct tWAVEFORMATEXX
{
    UInt16        wFormatTag;
    UInt16        nChannels;
    UInt32       nSamplesPerSec;
    UInt32       nAvgBytesPerSec;
    UInt16        nBlockAlign;
    UInt16        wBitsPerSample;
    UInt16        cbSize;

} WAVEFORMATEXX, *PWAVEFORMATEXX;

struct WavWriter *ww_init(struct WavWriter *_this, const char *_fileName, int _sampleRate)
{
	char head[44];
	_this->sampleRate = _sampleRate;
	_this->dataSize = 0;
	generateWaveHead(head, 0, _sampleRate);
	_this->fp = fopen(_fileName, "wb");
	fwrite(head, 1, sizeof(head), _this->fp);

	return _this;
}

int ww_write(struct WavWriter *_this, cchar *_data, int _size)
{
	_this->dataSize += _size;
	return fwrite(_data, sizeof(cchar), _size, _this->fp);
}

void ww_finalize(struct WavWriter *_this)
{
	char head[44];
	fseek(_this->fp, 0, SEEK_SET);
	generateWaveHead(head, _this->dataSize, _this->sampleRate);
	fwrite(head, 1, sizeof(head), _this->fp);
	fclose(_this->fp);
}

cchar *generateWaveHead(cchar *_header, int dataSize, int _sampleRate)
{
	cchar *waveHeader = _header;
	WAVEFORMATEXX fmt;
	memset(waveHeader, 0, 44);
	strncpy(waveHeader, "RIFF", 4);
	*((int *)&waveHeader[4]) = 36 + dataSize;
	strncpy(&waveHeader[8], "WAVE", 4);
	strncpy(&waveHeader[12], "fmt ", 4);
	*((int *)&waveHeader[16]) = 16;
	fmt.wFormatTag = WAVE_FORMAT_PCM;
	fmt.nChannels = PCM_CHANNEL;
	fmt.nSamplesPerSec = _sampleRate;
	fmt.wBitsPerSample = PCM_BITPERSEC;
	fmt.nBlockAlign = fmt.nChannels * fmt.wBitsPerSample / 8;
	fmt.nAvgBytesPerSec = fmt.nBlockAlign * fmt.nSamplesPerSec;
	memcpy(&waveHeader[20], &fmt, 16);
	strncpy(&waveHeader[36], "data", 4);
	*((int *)&waveHeader[40]) = dataSize;
	
	return _header;
}

void generateWave(cchar *pFileName, cchar *data, int size, int _sampleRate)
{
	FILE *pFile = fopen(pFileName, "wb");
	cchar waveHeader[64];
	if (NULL == pFile)
		return;
	generateWaveHead(waveHeader, size, _sampleRate);
	fwrite(waveHeader, 1, 44, pFile);
	fwrite(data, 1, size, pFile);
	fclose(pFile);
}

void saveFile(cchar *_fileName, cchar *_data, int _size)
{
	FILE *pFile = fopen(_fileName, "wb");
	fwrite(_data, 1, _size, pFile);
	fclose(pFile);
}

#endif

#if 1

struct TopNSearcher *tns_init(struct TopNSearcher *_this, int _dataCount, int _topNCount)
{
	int i;
	_this->topNMinIdx = 0;
	_this->topNCount = _topNCount;
	_this->dataCount = _dataCount;
	_this->topNIdx = (int *)mymalloc(_this->topNCount * sizeof(int));
	_this->datas = (TopNType *)mymalloc(_this->dataCount * sizeof(TopNType));
	mymemset(_this->datas, 0, _this->dataCount * sizeof(TopNType));
	mymemset(_this->topNIdx, 0, _this->topNCount * sizeof(int));

	for (i = 0; i < _this->topNCount; i ++)
	{
		_this->topNIdx[i] = i;
	}

#if 0
	{
		int i;
		cchar buf[128];
		memset(buf, 0, sizeof(buf));
		for (i = 0; i < _this->dataCount; i ++)
		{
			sprintf_s(buf+strlen(buf),sizeof(buf)-strlen(buf), "%2d,", i);
		}
		myprintf("%s\n", buf);
	}
#endif

	return _this;
}

void tns_finalize(struct TopNSearcher *_this)
{
	myfree(_this->datas);
	myfree(_this->topNIdx);
}

inline void tns_set(struct TopNSearcher *_this, int _dataIdx, TopNType _value)
{
	tns_add(_this, _dataIdx, _value-_this->datas[_dataIdx]);
}

void tns_add(struct TopNSearcher *_this, int _dataIdx, TopNType _addValue)
{
	int i;
	TopNType oldVal = _this->datas[_dataIdx], minVal = _this->datas[_this->topNIdx[_this->topNMinIdx]];
	assert(_dataIdx >= 0 && _dataIdx < _this->dataCount);
	_this->datas[_dataIdx] += _addValue;
	if (_addValue > 0)
	{

		if (_this->datas[_dataIdx] > minVal)
		{
			int myTopNIdx = -1;

			if(oldVal <= minVal)
			{

				for (i = 0; i < _this->topNCount; i ++)
				{
					if (_this->topNIdx[i] == _dataIdx)
					{
						myTopNIdx = i;
						break;
					}
				}

				if (myTopNIdx < 0)
				{
					_this->topNIdx[_this->topNMinIdx] = _dataIdx;
				}
			}
			if (_this->topNIdx[_this->topNMinIdx] == _dataIdx)
			{

				_this->topNMinIdx = 0;
				for (i = 1; i < _this->topNCount; i ++)
				{
					if (_this->datas[_this->topNIdx[i]] < _this->datas[_this->topNIdx[_this->topNMinIdx]])
					{
						_this->topNMinIdx = i;
					}
				}
			}
		}
	}
	else if (_addValue < 0)
	{

		if (oldVal >= minVal
			&& _this->datas[_dataIdx] < minVal)
		{

			int myTopNIdx = -1;
			for (i = 0; i < _this->topNCount; i ++)
			{
				if (_this->topNIdx[i] == _dataIdx)
				{
					myTopNIdx = i;
					break;
				}
			}
			if (myTopNIdx >= 0)
			{

				int newMaxIdx = -1;
				for (i = 0; i < _this->dataCount; i ++)
				{
					if (_this->datas[i] > _this->datas[_dataIdx] 
						&& _this->datas[i] <= minVal)
					{

						int j;
						bool iIsInTopN = false;
						for (j = 0; j < _this->topNCount; j ++)
						{
							if (i == _this->topNIdx[j])
							{
								iIsInTopN = true;
								break;
							}
						}
						if (!iIsInTopN)
						{
							if (newMaxIdx < 0 || _this->datas[i] > _this->datas[newMaxIdx])
							{
								newMaxIdx = i;
							}
						}
					}
				}
				if (newMaxIdx >= 0)
				{
					_this->topNIdx[myTopNIdx] = newMaxIdx;
					if (myTopNIdx != _this->topNMinIdx && _this->datas[_this->topNIdx[myTopNIdx]] < minVal)
					{
						_this->topNMinIdx = myTopNIdx;
					}
				}
			}
		}
	}
}

int *tns_topNIdx(struct TopNSearcher *_this)
{

#if 0
	int i;
	cchar buf[128];
	memset(buf, 0, sizeof(buf));
	for (i = 0; i < _this->dataCount; i ++)
	{
		sprintf_s(buf+strlen(buf),sizeof(buf)-strlen(buf), "%2d,", _this->datas[i]);
	}
	sprintf_s(buf+strlen(buf),sizeof(buf)-strlen(buf), "=>");
	for (i = 0; i < _this->topNCount; i ++)
	{
		sprintf_s(buf+strlen(buf),sizeof(buf)-strlen(buf), "%2d,", _this->topNIdx[i]);
	}
	myprintf("%s\n", buf);
#endif

	return _this->topNIdx;
}

inline TopNType *tns_data(struct TopNSearcher *_this)
{
	return _this->datas;
}

struct RangeTopNSearcher *rtns_init(struct RangeTopNSearcher *_this, int _rangeCount, int _topNCount)
{
	_this->datas = (RangeTopNType *)mymalloc(_rangeCount * sizeof(RangeTopNType));
	_this->topNIdx = (int *)mymalloc(_topNCount * sizeof(int));
	_this->rangeCount = _rangeCount;
	_this->topNCount = _topNCount;
	_this->currDataCount = 0;
	_this->needDelMem = true;

	return _this;
}

struct RangeTopNSearcher *rtns_init2(struct RangeTopNSearcher *_this, RangeTopNType *_datas, int _rangeCount, 
	int *_topNIdx, int _topNCount)
{
	_this->datas = _datas;
	_this->topNIdx = _topNIdx;
	_this->rangeCount = _rangeCount;
	_this->topNCount = _topNCount;
	_this->currDataCount = 0;
	_this->needDelMem = false;

	return _this;
}

void rtns_finalize(struct RangeTopNSearcher *_this)
{
	if (_this->needDelMem)
	{
		myfree(_this->datas);
		myfree(_this->topNIdx);
	}
}

void rtns_add(struct RangeTopNSearcher *_this, RangeTopNType _value)
{
	bool needReSearch = (_this->currDataCount == 0 || _value > _this->datas[_this->topNIdx[0]%_this->rangeCount])
		|| (_this->currDataCount >= _this->rangeCount && _this->topNIdx[0] == _this->currDataCount - _this->rangeCount);
	_this->datas[_this->currDataCount%_this->rangeCount] = _value;
	_this->currDataCount ++;
	assert(_this->topNCount == 1);
	if (needReSearch)
	{

		int i, maxIdx = _this->currDataCount - _this->rangeCount;
		RangeTopNType maxVal;
		if(maxIdx < 0)maxIdx = 0;
		maxVal = _this->datas[maxIdx%_this->rangeCount];
		for (i = maxIdx+1; i < _this->currDataCount; i ++)
		{
			if (_this->datas[i%_this->rangeCount] > maxVal)
			{
				maxIdx = i;
				maxVal = _this->datas[maxIdx%_this->rangeCount];
			}
		}
		_this->topNIdx[0] = maxIdx;
	}
}

inline int *rtns_topNIdx(struct RangeTopNSearcher *_this)
{
	return _this->topNIdx;
}

inline int rtns_idx(struct RangeTopNSearcher *_this)
{
	return _this->currDataCount;
}

inline RangeTopNType rtns_data(struct RangeTopNSearcher *_this, int _idx)
{
	assert(_idx >= _this->currDataCount-_this->rangeCount && _idx < _this->currDataCount);
	return _this->datas[_idx%_this->rangeCount];
}

#endif

