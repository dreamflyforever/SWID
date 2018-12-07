#ifndef UTIL_H
#define UTIL_H
#include <stdio.h>
#include <string.h>
#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const cchar hexChars[16];

#ifndef min
#define	min(a,b)	((a) < (b) ? (a) : (b))
#define	max(a,b)	((a) > (b) ? (a) : (b))
#endif

#define myabs(x) ((x)>0?(x):-(x))

void asign(void *_pdes, const void *_psource, int _sizeofEle);

int searchImin(int *_array, int _size);
int searchFmin(float *_array, int _size);

long long __llmin(long long _x, long long _y);
long long __llmax(long long _x, long long _y);
void swapi(int _left, int _right);
void swapui(unsigned int _left, unsigned int _right);
void swapp(void * _left, void * _right);
unsigned long getTickCount2(void);
void mysleep(int _ms);
const int myround(const double x);

int fint_compare(const void * m, const void* n);
int ffloat_compare(const void * m, const void* n);
int fshort_compare(const void * m, const void* n);

int mybinarySearch_(const void * _Key, const void * _Base,
	unsigned int _NumOfElements, unsigned int _SizeOfElements,
	int (*_PtFuncCompare)(const void *, const void *));

int topNIdx(const void *_elements, int _numOfElements, int _sizeOfElements, 
	int (*_fCompare)(const void *, const void *), int *_results, int _resultN);
int topN(const void *_elements, int _numOfElements, int _sizeOfElements, 
	int (*_fCompare)(const void *, const void *), void **_results, int _resultN);

unsigned short calc_crc16(int*, unsigned short);
unsigned short calc_step_crc16(int *buf, unsigned short len, unsigned short _crc);
unsigned short calc_crc16_2(int* pchMsg, unsigned short wDataLen);
unsigned short CRCCCITT(int* pDataIn, unsigned short iLenIn);
unsigned char crc4_cal2(int *data, int len);

int hexChar2Int(cchar _c);

int char64ToInt(cchar _c);

cchar int2Char64(int _hex);

void bitSet(cchar *_desBits, int _start, int _end, cchar _sourceBits);

cchar bitGet(cchar _sourceBits, int _start, int _end);

void bitsSet(cchar *_bits, int _bitsStart, int _bitsEnd, cchar _c);

cchar bitsGet(cchar *_bits, int _bitsStart, int _bitsEnd);

struct WavWriter
{
	int sampleRate, dataSize;
	FILE *fp;
};
struct WavWriter *ww_init(struct WavWriter *_this, const char *_fileName, int _sampleRate);
int ww_write(struct WavWriter *_this, cchar *_data, int _size);
void ww_finalize(struct WavWriter *_this);

cchar *generateWaveHead(cchar *_header, int dataSize, int _sampleRate);
void generateWave(cchar *pFileName, cchar *data, int size, int _sampleRate);
void saveFile(cchar *_fileName, cchar *_data, int _size);

#if 1

typedef int TopNType;
struct TopNSearcher
{
	TopNType *datas;
	int *topNIdx;
	int dataCount, topNCount;
	int topNMinIdx;
};
struct TopNSearcher *tns_init(struct TopNSearcher *_this, int _dataCount, int _topNCount);
void tns_finalize(struct TopNSearcher *_this);
void tns_add(struct TopNSearcher *_this, int _dataIdx, TopNType _addValue);
void tns_set(struct TopNSearcher *_this, int _dataIdx, TopNType _value);
int *tns_topNIdx(struct TopNSearcher *_this);
TopNType *tns_data(struct TopNSearcher *_this);

typedef short RangeTopNType;
struct RangeTopNSearcher
{
	RangeTopNType *datas;
	int *topNIdx;
	int rangeCount, topNCount, currDataCount;
	bool needDelMem;
};
struct RangeTopNSearcher *rtns_init(struct RangeTopNSearcher *_this, int _rangeCount, int _topNCount);
struct RangeTopNSearcher *rtns_init2(struct RangeTopNSearcher *_this, RangeTopNType *_datas, int _rangeCount, int *_topNIdx, int _topNCount);
void rtns_finalize(struct RangeTopNSearcher *_this);
void rtns_add(struct RangeTopNSearcher *_this, RangeTopNType _value);
int *rtns_topNIdx(struct RangeTopNSearcher *_this);
int rtns_idx(struct RangeTopNSearcher *_this);
RangeTopNType rtns_data(struct RangeTopNSearcher *_this, int _idx);

#endif

#ifdef __cplusplus
}
#endif

#endif

