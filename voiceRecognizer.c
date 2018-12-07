
#define _USE_MATH_DEFINES
#include <math.h>
#include "voiceRecognizer.h"
#include "memory.h"
#include "util.h"

#ifdef NO_INLINE
#define inline
#endif

const cchar * TAG_VoiceRecognizer = "voiceRecognizer";

#define DETECT_NOISE_SIGNAL

#define SAVE_WAVE_EXTRA_LEN 40 * 1024

#ifndef WIN32
#include "linux.h"
#endif

#define DefaultMinSaveSignalSize 100 * 1024
#define DefaultMinProcessSignalSize 100 * 1024
#define MinValidDetect 4
#define MAX_TRY_COUNT 2000

#define mytime time

int minProcessSignalSize = DefaultMinProcessSignalSize;
int minSaveSignalSize = DefaultMinSaveSignalSize;

int hisToryCacheSize = 1024 * 512;

static bool eccInited = false;

struct kiss_fft_out *fft_out_init(struct kiss_fft_out *_this, kiss_fft_cpx *_data, int _size)
{
	_this->out = _data;
	_this->outSize = _size;
	return _this;
}

inline float fft_out_amp(struct kiss_fft_out *_this, int _idx)
{
	return sqrt(_this->out[_idx].r * _this->out[_idx].r + _this->out[_idx].i * _this->out[_idx].i);
}

float *fft_out_amps(struct kiss_fft_out *_this, float *_amps, int _ampStartIdx, int _ampEndIdx)
{
	int i;
	float *amps = ((_amps == NULL)?((float *)_this->out):_amps);
	for(i = _ampStartIdx; i < _ampEndIdx; i ++)
	{
		amps[i] = sqrt(_this->out[i].r * _this->out[i].r + _this->out[i].i * _this->out[i].i);
	}
	return amps;
}

struct kissFFT *kiss_fft_init(struct kissFFT *_this, int _fftSize)
{
	_this->fftSize = _fftSize;
	_this->cfg = kiss_fftr_alloc(_fftSize, 0, NULL, NULL);
	_this->out = (kiss_fft_cpx *)mymalloc(sizeof(kiss_fft_cpx) *(_fftSize / 2 + 1));
	return _this;
}

void kiss_fft_finalize(struct kissFFT *_this)
{
	kiss_fft_free(_this->cfg);
	myfree(_this->out);
}

struct kiss_fft_out kiss_fft_execute(struct kissFFT *_this, ffttype *_in)
{
	struct kiss_fft_out out;
	kiss_fftr(_this->cfg, _in, _this->out);	
	return *fft_out_init(&out, _this->out, _this->fftSize / 2 + 1);
}

bool mrl_onAfterECC(struct MyRecognitionListener *_this, float _soundTime, int _recogStatus, int *_indexs, int _count)
{
	return _recogStatus == 0;
}

struct MyRecognitionListener *mrl_init(struct MyRecognitionListener *_this)
{
	_this->parent.onStartRecognition = mrl_onStartRecognition;
	_this->parent.onStopRecognition = mrl_onStopRecognition;
#ifdef SEARCH_SIMILAR_SIGNAL
	_this->parent.onStopRecognition2 = mrl_onStopRecognition2;
#endif
#if !(defined(FREQ_ANALYSE_SINGLE_MATCH) || defined(FREQ_ANALYSE_SINGLE_MATCH2))
	_this->parent.onMatchFrequency = NULL;
#endif

	_this->onAfterECC = mrl_onAfterECC;
	_this->rsCache = NULL;
	return _this;
}

void mrl_finalize(struct MyRecognitionListener *_this)
{
	void(*p)(void *) = NULL;
	if (_this->rsCache != NULL)_delete(_this->rsCache, p);
}

void mrl_onStartRecognition(struct RecognitionListener *_this, float _soundTime)
{
}

#define MAX_SIGNAL_SIZE MAX_CHARS
bool mrl_onStopRecognition(struct RecognitionListener *_this, float _soundTime, int _recogStatus, int *_indexs, int _count)
{
	struct MyRecognitionListener *this_ = (struct MyRecognitionListener *)_this;
	int RecogStatus = _recogStatus;
	int indexs[MAX_SIGNAL_SIZE];
	int signalLen = 0;
	int i;
	bool eccCheck;	
	asize2_1(_indexs, _count, indexs, MAX_SIGNAL_SIZE, "rsBuf", sizeof(rstype));
	if (_recogStatus == Recog_Success)
	{
		cchar currChar = 0, preChar = 0, prepreChar = 0;
		cchar realCurrChar = 0, realPreChar = 0, realPrePreChar = 0;

		for(i = 0; i < MAX_SIGNAL_SIZE && i < _count; i ++)
		{
			realCurrChar = currChar = adata(_indexs, i);
			if (i == 0 && currChar == DEFAULT_START_TOKEN)
			{
				continue;
			}
			else if (i == _count - 1 && currChar == DEFAULT_STOP_TOKEN)
			{
				continue;
			}
			if(currChar == DEFAULT_OVERLAP_TOKEN)
			{

				if (realPreChar == DEFAULT_OVERLAP2_TOKEN)
				{
					currChar = prepreChar;
				}
				else
				{
					currChar = preChar;
				}				
			}
			else if (currChar == DEFAULT_OVERLAP2_TOKEN)
			{
				currChar = prepreChar;
			}
			else
			{
				currChar = (cchar)adata(_indexs, i) - 1;
			}

			if (currChar < VOICE_CODE_MIN)
			{
				currChar = VOICE_CODE_MIN;
			}
			else if (currChar > VOICE_CODE_MAX)
			{
				currChar = VOICE_CODE_MAX;
			}
			adata(indexs, signalLen ++) = currChar;
			prepreChar = preChar;
			preChar = currChar;
			realPrePreChar = realPreChar;
			realPreChar = realCurrChar;
		}

		{
			rstype rsBuf[RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE];
			int indexsBeforeRS[MAX_SIGNAL_SIZE];
			int blockCount = (signalLen+(RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE-1))/(RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE);			
			int j, blockSize;
			rsInit();
			for (i = 0; i < blockCount; i ++)
			{
				memset(rsBuf, 0, sizeof(rsBuf));
				blockSize = ((i == (blockCount-1) && signalLen%(RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE) > 0)?signalLen%(RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE):(RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE));
				for (j = 0; j < blockSize; j ++)
				{
					adata(rsBuf, j) = adata(indexs, i * (RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE)+j);	
				}
				rsDecode(rsBuf, NULL, 0);
				for (j = 0; j < blockSize-RS_CORRECT_SIZE; j ++)
				{
					adata(indexsBeforeRS, i*RS_CORRECT_BLOCK_SIZE+j) = adata(indexs, i*(RS_CORRECT_BLOCK_SIZE+RS_CORRECT_SIZE)+j);
					adata(indexs, i*RS_CORRECT_BLOCK_SIZE+j) = adata(rsBuf, j);
				}
			}
			signalLen -= (blockCount * RS_CORRECT_SIZE);

			if (memcmp(indexs, indexsBeforeRS, sizeof(int) * signalLen) != 0)
			{
				cchar buffer[MAX_SIGNAL_SIZE+1];
				memset(buffer, 0, MAX_SIGNAL_SIZE+1);
				myprintf("rs recorrect data, before to after:\n");
				for (j = 0; j < signalLen; j ++)
				{
					sprintf_s(buffer + strlen(buffer), MAX_SIGNAL_SIZE - strlen(buffer), "%c", adata(hexChars, adata(indexsBeforeRS, j)));
				}
				myprintf("%s\n", buffer);
				memset(buffer, 0, MAX_SIGNAL_SIZE+1);
				for (j = 0; j < signalLen; j ++)
				{
					sprintf_s(buffer + strlen(buffer), MAX_SIGNAL_SIZE - strlen(buffer), "%c", adata(hexChars, adata(indexs, j)));
				}
				myprintf("%s\n", buffer);
			}
		}

		if (signalLen <= 4)
		{
			RecogStatus = Status_NotEnoughSignal;
		}
		else
		{

			eccCheck = mrl_decode(this_, indexs, signalLen);
#ifndef SILENCE
			myprintf("ecc check %s\n", (eccCheck?"ok":"fail"));
#endif
			if(!eccCheck)
			{
				RecogStatus = Status_ECCError;
			}
			else
			{
				signalLen = signalLen - 4;
			}
		}
	}
	else if(RecogStatus == Recog_RecogCountZero)
	{
		RecogStatus = Status_RecogCountZero;
	}

	return this_->onAfterECC(this_, _soundTime, RecogStatus, indexs, signalLen);
}

BlockRSCache *brc_init(BlockRSCache *_this)
{
	int i;
	for (i = 0; i < MAX_BLOCK_COUNT; i ++)
	{
		_this->blockRS[i].blockStatus = BLOCKRS_STATUS_CONTINUE;
		_this->blockRS[i].firstItemIdx = _this->blockRS[i].lastItemIdx = BRC_IDX_NULL;
		_this->blockRS[i].itemCount = 0;
	}
	memset(_this->items, 0, MAX_BLOCK_ITEM_COUNT * sizeof(BlockRSItem));
	_this->itemCount = 0;

	return _this;
}

inline cchar brc_blockStatus(BlockRSCache *_this, int _blockIdx)
{
	assert(_blockIdx >= 0 && _blockIdx < MAX_BLOCK_COUNT);
	return _this->blockRS[_blockIdx].blockStatus;
}

cchar brc_blockAddItem(BlockRSCache *_this, int _blockIdx, rstype *_rsBuf, cchar _validBlockSize, BlockRSItemEnum *_resultPos)
{
	assert(_blockIdx >= 0 && _blockIdx < MAX_BLOCK_COUNT);
	assert(_this->blockRS[_blockIdx].blockStatus == BLOCKRS_STATUS_CONTINUE);
	if (_this->blockRS[_blockIdx].blockStatus == BLOCKRS_STATUS_CONTINUE)
	{

		if (_rsBuf == NULL)
		{
			_this->blockRS[_blockIdx].blockStatus = BLOCKRS_STATUS_FINISHED;
		}
		else
		{
			bool exist = false;

			if(_this->blockRS[_blockIdx].lastItemIdx >= 0)
			{
				rstype *lastRSBuf = _this->items[_this->blockRS[_blockIdx].lastItemIdx].rsBuf;
				rstype *firstRSBuf = _this->items[_this->blockRS[_blockIdx].firstItemIdx].rsBuf;
				exist = memcmp(lastRSBuf, _rsBuf, (RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE) * sizeof(rstype)) == 0;
				if(!exist)exist = memcmp(firstRSBuf, _rsBuf, (RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE) * sizeof(rstype)) == 0;

				if(exist)return BLOCKRS_STATUS_EXIST;
			}
			if (_this->itemCount < MAX_BLOCK_ITEM_COUNT)
			{
				memcpy(_this->items[_this->itemCount].rsBuf, _rsBuf, (RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE) * sizeof(rstype));
				_this->items[_this->itemCount].validSize = _validBlockSize;
				_this->items[_this->itemCount].nextItemIdx = BRC_IDX_NULL;
				if(_resultPos != NULL)*_resultPos = _this->itemCount;
				if (_this->blockRS[_blockIdx].firstItemIdx == BRC_IDX_NULL)
					_this->blockRS[_blockIdx].firstItemIdx = _this->itemCount;
				else
				{
					assert(_this->blockRS[_blockIdx].lastItemIdx >= 0);
					_this->items[_this->blockRS[_blockIdx].lastItemIdx].nextItemIdx = _this->itemCount;
				}
				_this->blockRS[_blockIdx].lastItemIdx = _this->itemCount;
				_this->blockRS[_blockIdx].itemCount ++;
				_this->itemCount ++;
			}
			else
				_this->blockRS[_blockIdx].blockStatus = BLOCKRS_STATUS_ERROR;
		}
	}
	return _this->blockRS[_blockIdx].blockStatus;
}

inline short brc_blockItemCount(BlockRSCache *_this, int _blockIdx)
{
	return _this->blockRS[_blockIdx].itemCount;
}

BlockRSItemEnum brc_blockEnumItem(BlockRSCache *_this, int _blockIdx)
{
	assert(_blockIdx >= 0 && _blockIdx < MAX_BLOCK_COUNT);
	return BRC_IDX_INIT;
}

BlockRSItem *brc_blockEnumNext(BlockRSCache *_this, int _blockIdx, BlockRSItemEnum *_itemEnum)
{
	assert(_blockIdx >= 0 && _blockIdx < MAX_BLOCK_COUNT);
	if(*_itemEnum == BRC_IDX_INIT)*_itemEnum = _this->blockRS[_blockIdx].firstItemIdx;
	else if (*_itemEnum >= 0)
	{
		*_itemEnum = _this->items[*_itemEnum].nextItemIdx;
	}
	return brc_blockItem(_this, _itemEnum);
}

inline BlockRSItem *brc_blockItem(BlockRSCache *_this, BlockRSItemEnum *_itemEnum)
{
	BlockRSItem *result = NULL;
	if (*_itemEnum >= 0)
	{
		result = &_this->items[*_itemEnum];
	}
	return result;
}

int crc4CalCount = 0;

#ifdef SEARCH_SIMILAR_SIGNAL
struct LoopSignalHelper_
{
	struct TimeRangeSignal*signals;
	int signalsCount; 
	struct SignalBlock *blocks;
	int blockCount;
	int resultBuf[MAX_SIGNAL_SIZE];
	int resultLen;
	short blockLoopIdx[MAX_BLOCK_COUNT];
	BlockRSItemEnum currCacheEnums[MAX_BLOCK_COUNT];
	int triedCount;
	BlockRSCache *rsCache;
};
typedef struct LoopSignalHelper_ LoopSignalHelper;
LoopSignalHelper *lsh_init(LoopSignalHelper *_this, struct TimeRangeSignal*_signals, int _signalsCount, 
	struct SignalBlock *_blocks, int _blockCount, BlockRSCache *_rsCache)
{
	_this->signals = _signals;
	_this->signalsCount = _signalsCount;
	_this->blocks = _blocks;
	_this->blockCount = _blockCount;
	_this->rsCache = _rsCache;
	_this->resultLen = 0;
	_this->triedCount = 0;

	return _this;
}

cchar loopSignalInBlock(struct MyRecognitionListener *_this, LoopSignalHelper *_loopHelper, int _blockIdx, 
	cchar *_multiSimilarSignalInBlock, int  _multiSimilarSignalCountInBlock, int _multiSimilarSignalIdxInBlock, 
	rstype *_rsBufInBlock, rstype *_preRealRSBufRealOnly);

#define MAX_BLOCK_SIGNAL_COUNT (RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE+MAX_BLOCK_START_SIGNAL_MISS)

cchar loopBlock(struct MyRecognitionListener *_this, LoopSignalHelper *_loopHelper, int _blockIdx, rstype *_preRealRSBufRealOnly)
{

	if (_blockIdx >= _loopHelper->blockCount)
	{

		bool eccCheck = mrl_decode(_this, _loopHelper->resultBuf, _loopHelper->resultLen);

		if (eccCheck)
		{
			return 1;
		}
		if(_loopHelper->triedCount ++ > MAX_TRY_COUNT) return -1;	
		return false;
	}

		_loopHelper->blockLoopIdx[_blockIdx] = 0;

		if (_loopHelper->rsCache != NULL && brc_blockStatus(_loopHelper->rsCache, _blockIdx) == BLOCKRS_STATUS_FINISHED)
		{
			BlockRSItemEnum itemEnum = brc_blockEnumItem(_loopHelper->rsCache, _blockIdx);
			BlockRSItem *blockItem;
			rstype *blockRSBuf = NULL;
			cchar validBlockSize = 0;
			asize1_1(_loopHelper->resultBuf, MAX_SIGNAL_SIZE, "blockRSBuf", sizeof(rstype))
			while ((blockItem = brc_blockEnumNext(_loopHelper->rsCache, _blockIdx, &itemEnum)) != NULL)
			{
				short k, currResultlen = _loopHelper->resultLen;
				validBlockSize = blockItem->validSize;
				blockRSBuf = blockItem->rsBuf;
				areset2("blockRSBuf", blockRSBuf, (RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE));

				for (k = 0; k < validBlockSize - RS_CORRECT_SIZE; k ++)
				{
					adata(_loopHelper->resultBuf, _loopHelper->resultLen + k) = adata(blockRSBuf, k);
				}
				_loopHelper->blockLoopIdx[_blockIdx] ++;

				_loopHelper->resultLen = _loopHelper->resultLen + (validBlockSize - RS_CORRECT_SIZE);

				{

					int eccCheck = loopBlock(_this, _loopHelper, _blockIdx+1, blockRSBuf);
					if(eccCheck)return eccCheck;
				}

				_loopHelper->resultLen = currResultlen;
			}

		}
		else
		{
			asize2_2(_loopHelper->blocks, _loopHelper->blockCount, _loopHelper->signals, _loopHelper->signalsCount, "mulSimilarSignalInBlock", sizeof(cchar), "rsBufInBlock", sizeof(rstype))
				short blockSize = _loopHelper->blocks[_blockIdx].signalCount;

			cchar mulSimilarSignalInBlock[MAX_BLOCK_SIGNAL_COUNT];
			rstype rsBufInBlock[MAX_BLOCK_SIGNAL_COUNT];
			short mulSimilarCountInBlock = 0, i;
			short signalIdx;
			cchar result;
			areset2("mulSimilarSignalInBlock", mulSimilarSignalInBlock, MAX_BLOCK_SIGNAL_COUNT);
			areset2("rsBufInBlock", rsBufInBlock, MAX_BLOCK_SIGNAL_COUNT);
			assert(_blockIdx < _loopHelper->blockCount);
			for (i = 0; i < blockSize && i < MAX_BLOCK_SIGNAL_COUNT; i ++)
			{
				signalIdx = adata(_loopHelper->blocks, _blockIdx).startIdx + i;
				assert(adata(_loopHelper->signals, signalIdx).uidexs.idxes[0] >= 0);
				if(adata(_loopHelper->signals, signalIdx).uidexs.idxes[1] >= 0)
					adata(mulSimilarSignalInBlock, mulSimilarCountInBlock++) = (char)i;
				else
					adata(rsBufInBlock, i) = adata(_loopHelper->signals, signalIdx).uidexs.idxes[0];
			}

			result = loopSignalInBlock(_this, _loopHelper, _blockIdx, mulSimilarSignalInBlock, mulSimilarCountInBlock, 0,
				rsBufInBlock, _preRealRSBufRealOnly);
			if (_loopHelper->rsCache != NULL && brc_blockStatus(_loopHelper->rsCache, _blockIdx) == BLOCKRS_STATUS_CONTINUE)
				brc_blockAddItem(_loopHelper->rsCache, _blockIdx, NULL, 0, NULL);
			if(result)return result;
		}

		return false;

}

cchar loopSignalInBlock(struct MyRecognitionListener *_this, LoopSignalHelper *_loopHelper, int _blockIdx,
	cchar *_multiSimilarSignalInBlock, int  _multiSimilarSignalCountInBlock, int _multiSimilarSignalIdxInBlock, 
	rstype *_rsBufInBlock, rstype *_preRealRSBufRealOnly)
{
	asize7_4(_loopHelper->blocks, _loopHelper->blockCount, _preRealRSBufRealOnly, RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE, _rsBufInBlock, MAX_BLOCK_SIGNAL_COUNT, 
		_loopHelper->resultBuf, MAX_SIGNAL_SIZE, hexChars, 16, _multiSimilarSignalInBlock, _multiSimilarSignalCountInBlock, _loopHelper->signals, _loopHelper->signalsCount,
		"blockRSBuf", sizeof(rstype), "rsBuf", sizeof(rstype), "mayErrors", sizeof(cchar), "mayErrorsIdxInBlock", sizeof(cchar));

	if (_multiSimilarSignalIdxInBlock >= _multiSimilarSignalCountInBlock)
	{

		int blockSize = adata(_loopHelper->blocks, _blockIdx).signalCount;
		int validBlockSize = 0;
		int i, j;
		bool zeroSimilar = true;
		int erase[RS_CORRECT_SIZE];
		int eraseCount = 0;

		rstype rsBuf[RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE];
		cchar mayErrors[RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE];
		cchar mayErrorsIdxInBlock[RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE];
		cchar currChar = 0, preChar = 0, prepreChar = 0;
		cchar realCurrChar = 0, realPreChar = 0, realPrePreChar = 0;
		int k, rsBufIdx = 0, mayErrorIdx = 0, lastStartSignalCount = 0;
		areset2("rsBuf", rsBuf, RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE);
		areset2("mayErrors", mayErrors, RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE);
		areset2("mayErrorsIdxInBlock", mayErrorsIdxInBlock, RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE);
		if (blockSize <= RS_CORRECT_SIZE)
		{
			return 0;
		}
		if (_blockIdx > 0)
		{
			assert(_preRealRSBufRealOnly != NULL);
			preChar = adata(_preRealRSBufRealOnly, RS_CORRECT_BLOCK_SIZE+RS_CORRECT_SIZE-1);
			prepreChar = adata(_preRealRSBufRealOnly, RS_CORRECT_BLOCK_SIZE+RS_CORRECT_SIZE-1-1);
		}
		memset(mayErrorsIdxInBlock, -1, sizeof(mayErrorsIdxInBlock));

		for(k = 0; k < blockSize && k < MAX_BLOCK_SIGNAL_COUNT && rsBufIdx < (RS_CORRECT_BLOCK_SIZE + RS_CORRECT_SIZE); k ++)
		{
			realCurrChar = currChar = adata(_rsBufInBlock, k);

			if (currChar == 0)
			{				
				if (rsBufIdx == 0)
				{
					continue;
				}
				lastStartSignalCount ++;
			}
			else
				lastStartSignalCount = 0;

			if(currChar == DEFAULT_OVERLAP_TOKEN)
			{

				if (realPreChar == DEFAULT_OVERLAP2_TOKEN)
				{
					currChar = prepreChar;
				}
				else
				{
					currChar = preChar;
				}				
			}
			else if (currChar == DEFAULT_OVERLAP2_TOKEN)
			{
				currChar = prepreChar;
			}
			else
			{
				currChar = (cchar)adata(_rsBufInBlock, k) - 1;
			}

			if (currChar < VOICE_CODE_MIN || currChar > VOICE_CODE_MAX || trs_maybeError(_loopHelper->signals+(adata(_loopHelper->blocks, _blockIdx).startIdx+k)))
			{
				adata(mayErrorsIdxInBlock, mayErrorIdx) = k;
				adata(mayErrors, mayErrorIdx++) = rsBufIdx;
			}

			if (currChar < VOICE_CODE_MIN)
			{
				currChar = VOICE_CODE_MIN;

			}
			else if (currChar > VOICE_CODE_MAX)
			{
				currChar = VOICE_CODE_MAX;

			}
			adata(rsBuf, rsBufIdx++) = currChar;
			prepreChar = preChar;
			preChar = currChar;
			realPrePreChar = realPreChar;
			realPreChar = realCurrChar;
		}
		if (_blockIdx == _loopHelper->blockCount-1 && lastStartSignalCount > 0)
		{
			rsBufIdx = rsBufIdx - lastStartSignalCount;
			assert(mayErrorIdx >= lastStartSignalCount);
			mayErrorIdx = mayErrorIdx - lastStartSignalCount;
		}
		if (rsBufIdx <= RS_CORRECT_SIZE)
		{
			return 0;
		}
		if (rsBufIdx < (RS_CORRECT_BLOCK_SIZE+RS_CORRECT_SIZE))
		{

			if (_blockIdx == _loopHelper->blockCount-1)
			{
				memmove(rsBuf+RS_CORRECT_BLOCK_SIZE, rsBuf+rsBufIdx-RS_CORRECT_SIZE, RS_CORRECT_SIZE*sizeof(rstype));
				memset(rsBuf+rsBufIdx-RS_CORRECT_SIZE, 0, (RS_CORRECT_BLOCK_SIZE+RS_CORRECT_SIZE-rsBufIdx)*sizeof(rstype));

			}
			else if (rsBufIdx < RS_CORRECT_BLOCK_SIZE)
			{
				return 0;
			}
			else
			{

				int x, xc = (RS_CORRECT_BLOCK_SIZE+RS_CORRECT_SIZE-rsBufIdx);
				for (x = 0; x < xc; x ++)
				{
					adata(mayErrors, mayErrorIdx++) = rsBufIdx+x;
					adata(rsBuf, rsBufIdx+x) = VOICE_CODE_MIN;					
				}
			}
		}
		validBlockSize = ((_blockIdx>=_loopHelper->blockCount-1)?rsBufIdx:(RS_CORRECT_BLOCK_SIZE+RS_CORRECT_SIZE));

		for (i = -1; i < mayErrorIdx; i ++)
		{
			eraseCount = 0;

			if (!(i < 0 || mayErrorsIdxInBlock[i] < 0 
				|| (mayErrorsIdxInBlock[i] >= 0 && adata(_rsBufInBlock, mayErrorsIdxInBlock[i]) 
					== adata(_loopHelper->signals, adata(_loopHelper->blocks, _blockIdx).startIdx+mayErrorsIdxInBlock[i]).uidexs.idxes[0])))
			{
				continue;
			}
			if (i >= 0)
			{
				erase[0] = mayErrors[i];
				eraseCount = 1;
			}
			for (j = i; j < mayErrorIdx; j ++)
			{
				int jeraseCount = eraseCount;

				if (!(j < 0 || mayErrorsIdxInBlock[j] < 0 
					|| (mayErrorsIdxInBlock[j] >= 0 && adata(_rsBufInBlock, mayErrorsIdxInBlock[j]) 
					== adata(_loopHelper->signals, adata(_loopHelper->blocks, _blockIdx).startIdx+mayErrorsIdxInBlock[j]).uidexs.idxes[0])))
				{
					continue;
				}
				if (j > i && eraseCount == 1)
				{
					erase[1] = mayErrors[j];
					eraseCount = 2;
				}

						if (zeroSimilar || (i >= 0 && j >= 0 && i != j && eraseCount > RS_CORRECT_SIZE/2
							&& eraseCount >= min(mayErrorIdx, RS_CORRECT_SIZE)))
						{

							int recovery = -1;
							rstype blockRSBuf[RS_CORRECT_BLOCK_SIZE+RS_CORRECT_SIZE];
							memcpy(blockRSBuf, rsBuf, (RS_CORRECT_BLOCK_SIZE+RS_CORRECT_SIZE)*sizeof(rstype));
							areset2("blockRSBuf", blockRSBuf, RS_CORRECT_BLOCK_SIZE+RS_CORRECT_SIZE);
							if (zeroSimilar)
							{
								recovery = rsDecode(blockRSBuf, NULL, 0);
								zeroSimilar = false;
							}
							else
							{
								assert(eraseCount > RS_CORRECT_SIZE/2);

								recovery = rsDecode(blockRSBuf, erase, eraseCount);
							}
							if (recovery >= 0)
							{
								short currResultLen = _loopHelper->resultLen;
								_loopHelper->currCacheEnums[_blockIdx] = BRC_IDX_NULL;

								if (_loopHelper->rsCache != NULL && brc_blockStatus(_loopHelper->rsCache, _blockIdx) == BLOCKRS_STATUS_CONTINUE)
								{

									bool exist = (brc_blockAddItem(_loopHelper->rsCache, _blockIdx, blockRSBuf, (cchar)validBlockSize, 
										&_loopHelper->currCacheEnums[_blockIdx]) == BLOCKRS_STATUS_EXIST);
									if(exist)
										continue;
								}

								_loopHelper->blockLoopIdx[_blockIdx] ++;

								for (k = 0; k < validBlockSize - RS_CORRECT_SIZE; k ++)
								{
									adata(_loopHelper->resultBuf, _loopHelper->resultLen + k) = adata(blockRSBuf, k);
								}

								_loopHelper->resultLen = _loopHelper->resultLen + (validBlockSize - RS_CORRECT_SIZE);

								{

									char eccCheck = loopBlock(_this, _loopHelper, _blockIdx+1, blockRSBuf);
									if(eccCheck)return eccCheck;
								}

								_loopHelper->resultLen = currResultLen;
							}
						}

				eraseCount = jeraseCount;
				if(!zeroSimilar && !(i >= 0))break;
			}
		}		
	}
	else
	{
		short i, signalIdx = adata(_loopHelper->blocks, _blockIdx).startIdx + adata(_multiSimilarSignalInBlock, _multiSimilarSignalIdxInBlock);
		char searched;
		for (i = 0; i < MAX_RANGE_IDX_COUNT; i ++)
		{
			if(adata(_loopHelper->signals, signalIdx).uidexs.idxes[i] < 0)break;
			adata(_rsBufInBlock, adata(_multiSimilarSignalInBlock, _multiSimilarSignalIdxInBlock)) = adata(_loopHelper->signals, signalIdx).uidexs.idxes[i];
			searched = loopSignalInBlock(_this, _loopHelper, _blockIdx, _multiSimilarSignalInBlock, _multiSimilarSignalCountInBlock, 
				_multiSimilarSignalIdxInBlock+1, _rsBufInBlock, _preRealRSBufRealOnly);
			if(searched)return searched;
		}
	}

	return false;
}

bool mrl_onStopRecognition2(struct RecognitionListener *this_, float _soundTime, int _recogStatus, struct TimeRangeSignal* _indexs, int _count
	, struct SignalBlock *_blocks, int _blockCount)
{
	struct MyRecognitionListener *_this = (struct MyRecognitionListener *)this_;
	int RecogStatus = _recogStatus;

	LoopSignalHelper searchHelper;
	int signalLen = _count;
	if (_recogStatus == Recog_Success)
	{		
		if (signalLen <= 4 + RS_CORRECT_SIZE)
		{
			RecogStatus = Status_NotEnoughSignal;
		}
		else if (_count >= MAX_SIGNAL_SIZE)
		{
			RecogStatus = Status_TooMuchSignal;
		}

		if (RecogStatus == Recog_Success)
		{
			int eccCheck;

			if(_this->rsCache == NULL)_this->rsCache = _new(BlockRSCache, brc_init);
			else 
				brc_init(_this->rsCache);
			lsh_init(&searchHelper, _indexs, _count, _blocks, _blockCount, _this->rsCache);

			rsInit();
			eccCheck = loopBlock(_this, &searchHelper, 0, NULL);
			signalLen = searchHelper.resultLen;

#ifndef SILENCE
			myprintf("ecc check %s\n", (eccCheck>0?"ok":"fail"));
#endif

			if(!(eccCheck > 0))
			{
				RecogStatus = Status_ECCError;
			}
			else
			{
				signalLen = signalLen - 4;
			}
		}
	}
	else if(RecogStatus == Recog_RecogCountZero)
	{
		RecogStatus = Status_RecogCountZero;
	}

	return _this->onAfterECC(_this, _soundTime, RecogStatus, searchHelper.resultBuf, signalLen);
}
#endif

bool mrl_decode(struct MyRecognitionListener *_this, int *indexs, int signalLen)
{
	if(signalLen-4<=0)return false;
	else
	{
		unsigned short crc16 = calc_crc16(indexs, signalLen-4); 
		asize1(indexs, signalLen)
			unsigned short dataCrc16 = ((adata(indexs, signalLen-4) & 0xF) << 12) | ((adata(indexs, signalLen-3) & 0xF) << 8) | ((adata(indexs, signalLen-2) & 0xF) << 4) | (adata(indexs, signalLen-1) & 0xF);
		return crc16 == dataCrc16;
	}
}

struct FFTVoiceProcessor *fvp_init(struct FFTVoiceProcessor *_this,
	int _sampleRate, int _channel, int _bits, int _bufferSize, int _overlap)
{
	_this->parent.parent.process = fvp_process;
	_this->parent.parent.setFreqs = fvp_setFreqs;
	_this->parent.parent.setListener = fvp_setListener;
	_this->parent.parent.getListener = fvp_getListener;
#ifdef CONTROL_RECOGNIZER_INIT
	_this->parent.parent.isNoiseLevelInited = fvp_isNoiseLevelInited;
	_this->parent.parent.reInitNoiseLevel = fvp_reInitNoiseLevel;
#endif
	_this->parent.getAnalyserIndicator = fvp_getAnalyserIndicator;

	_this->analyserCount = 0;
	_this->channel = _channel;
	_this->bits = _bits;
	_this->sampleRate = _sampleRate;
	fvp_addSignalAnalyser(_this, _new(struct SignalAnalyser, sa_init, _sampleRate, _channel, _bits, _bufferSize, _overlap));
	kiss_fft_init(&_this->fft, _bufferSize);
	_this->floatOverlap = _overlap;
	_this->floatStepSize = (_bufferSize - _overlap);
	_this->firstBuffer = true;
	_this->byteOverlap = (_overlap * (((_bits + 7) / 8) * _channel));
	_this->byteStepSize = ((_bufferSize-_overlap) * (((_bits + 7) / 8) * _channel));
	_this->FFTSize = _bufferSize;
	_this->floatBuffer = (float *)mymalloc(sizeof(float) * _bufferSize);
	mymemset(_this->floatBuffer, 0, _bufferSize * sizeof(float));
	_this->outAmptitudes = (float *)mymalloc(sizeof(float) * (_this->FFTSize / 2 + 1));
	_this->converter = getConverter(_channel, _bits, true, false);
	return _this;
}

int fvp_addSignalAnalyser(struct FFTVoiceProcessor *_this, TFFTSignalAnalyser *_analyser)
{
	asize2(_this->analysers, FreqAnalyserCount, _this->analyserIndicators, FreqAnalyserCount);
	assert(_this->analyserCount < FreqAnalyserCount);
	adata(_this->analysers, _this->analyserCount) = _analyser;
	adata(_this->analyserIndicators, _this->analyserCount).isAnalyserProcess = true;
	adata(_this->analyserIndicators, _this->analyserCount).noDetect = 1;
	return _this->analyserCount ++;
}

void fvp_finalize(struct FFTVoiceProcessor *_this)
{
	int i;
	asize1(_this->analysers, _this->analyserCount);
	myfree(_this->floatBuffer);
	myfree(_this->outAmptitudes);
	myfree(_this->converter);
	for (i = 0; i < _this->analyserCount; i ++)
	{
		_delete(adata(_this->analysers, i), sa_finalize);
	}	
	kiss_fft_finalize(&_this->fft);
}

void fvp_setFreqs(struct VoiceProcessor *this_, int _freqsIdx, int* _freqs)
{
	struct FFTVoiceProcessor *_this = (struct FFTVoiceProcessor *)this_;
	asize1(_this->analysers, _this->analyserCount);
	if (_freqsIdx == _this->analyserCount)
	{
		TFFTSignalAnalyser *analyser = _new(TFFTSignalAnalyser, sa_init, _this->sampleRate, _this->channel, _this->bits, _this->FFTSize, _this->floatOverlap);
		fvp_addSignalAnalyser(_this, analyser);
		areset(_this->analysers, _this->analyserCount);
	}
	if(_freqsIdx < _this->analyserCount)
	{
		sa_setFreqs(adata(_this->analysers, _freqsIdx), _freqs);
	}
	else
	{
		throwError("analyser index not exists");
	}
}

void fvp_setListener(struct VoiceProcessor *this_, int _freqsIdx, struct RecognitionListener *_listener)
{
	struct FFTVoiceProcessor *_this = (struct FFTVoiceProcessor *)this_;
	asize1(_this->analysers, _this->analyserCount);
	if(_freqsIdx < _this->analyserCount)
	{
		sa_setListener(adata(_this->analysers, _freqsIdx), _listener);
	}
	else
	{
		throwError("analyser index not exists");
	}
}

struct RecognitionListener *fvp_getListener(struct VoiceProcessor *this_, int _freqsIdx)
{
	struct FFTVoiceProcessor *_this = (struct FFTVoiceProcessor *)this_;
	asize1(_this->analysers, _this->analyserCount);
	if(_freqsIdx < _this->analyserCount)
	{
		return sa_getListener(adata(_this->analysers, _freqsIdx));
	}
	else
	{
		throwError("analyser index not exists");
	}
}

#ifdef CONTROL_RECOGNIZER_INIT
bool fvp_isNoiseLevelInited(struct VoiceProcessor *this_, int _freqIdx, float *_noiseLevel)
{
	struct FFTVoiceProcessor *_this = (struct FFTVoiceProcessor *)this_;
	return sa_isInited(_this->analysers[_freqIdx], _noiseLevel);
}

void fvp_reInitNoiseLevel(struct VoiceProcessor *this_, int _freqIdx, float _noiseLevel)
{
	struct FFTVoiceProcessor *_this = (struct FFTVoiceProcessor *)this_;
	asize2(_this->analysers, _this->analyserCount, _this->analyserIndicators, _this->analyserCount);
	sa_reInit(_this->analysers[_freqIdx], _noiseLevel);
	adata(_this->analyserIndicators, _freqIdx).noDetect = (sa_isInited(adata(_this->analysers, _freqIdx), NULL)?0:1);
}
#endif

struct SignalAnalyserProcessIndicator *fvp_getAnalyserIndicator(struct SignalAnalyserVoiceProcessor *this_, int _idx)
{
	struct FFTVoiceProcessor *_this = (struct FFTVoiceProcessor *)this_;
	assert(_idx < _this->analyserCount);
	return _this->analyserIndicators + _idx;
}

int fftVoiceIdx = 0;
unsigned long fftIdxStartTime = 0;
unsigned long realProcessTime = 0;
unsigned long realFFTPrepareTime = 0;
unsigned long realFFTTime = 0;
unsigned long realAnalyseTime = 0;
unsigned long realAnalyseStartTime = 0;
unsigned long realAnalyseEndTime = 0;
unsigned long realAnalyseInnerTime = 0;
unsigned long realSqrtTime = 0;

void fvp_process(struct VoiceProcessor *this_, struct VoiceEvent *_event)
{
	struct FFTVoiceProcessor *_this = (struct FFTVoiceProcessor *)this_;
	unsigned long realProcessStart;
	unsigned long start;
	struct BufferData *data;
	unsigned long now;
	struct kiss_fft_out out;
	int minFreqIdx, maxFreqIdx, i, j;
	float *outAmptitudes = NULL;
	asize4(_this->analysers, _this->analyserCount, _this->analyserIndicators, _this->analyserCount, 
		_this->outAmptitudes, _this->FFTSize / 2 + 1, _this->floatBuffer, _this->FFTSize);

	if (((++fftVoiceIdx)%1000 == 0)
		|| (_event->data == NULL || bd_isNULL(_event->data)))
	{
		unsigned long now = getTickCount2();
	}

	if (_event->data == NULL || bd_isNULL(_event->data))
	{
		for (i = 0; i < _this->analyserCount; i ++)
		{
			sa_analyFFT(adata(_this->analysers, i), NULL);
		}		
		_this->firstBuffer = true;

		return;
	}

	realProcessStart = getTickCount2();
	start = realProcessStart;

	if(!_this->firstBuffer)mymemmove(_this->floatBuffer, _this->floatBuffer + _this->floatStepSize, _this->floatOverlap * sizeof(float));
	_this->converter->toFloatArray(bd_getData(_event->data) + (_this->firstBuffer?0:_this->byteOverlap), 
		_this->floatBuffer + (_this->firstBuffer?0:_this->floatOverlap), 
		(_this->firstBuffer?(_this->floatOverlap + _this->floatStepSize):_this->floatStepSize));		

	{

#if 0

# if 0
		float a0=0.54, a1=(1.0f - a0);
# else
		float a0=0.5, a1=0.5;
# endif

		float zoff = 0.0;
		for ( i=0; i<_this->FFTSize; i++ ) {			

			adata(_this->floatBuffer, i) *= (a0 - a1 * cos((2.0f*M_PI*((float)i+zoff)) / _this->FFTSize));
		}
#endif
	}

	data = _event->data;

	now = getTickCount2();
	realFFTPrepareTime += (now - start);
	start = now;

	out = kiss_fft_execute(&_this->fft, _this->floatBuffer);

	now = getTickCount2();
	realFFTTime += (now - start);
	start = now;
	for(j = 0; j < _this->analyserCount; j ++)
	{
		if(!adata(_this->analyserIndicators, j).isAnalyserProcess)continue;
		outAmptitudes = fft_out_amps(&out, _this->outAmptitudes, minFreqIdx = sa_getMinFreqIdx(adata(_this->analysers, j)), maxFreqIdx = sa_getMaxFreqIdx(adata(_this->analysers, j)));

	}
	now = getTickCount2();
	realSqrtTime += (now - start);
	start = now;

	for (i = 0; i < _this->analyserCount; i ++)
	{
		if(adata(_this->analyserIndicators, i).isAnalyserProcess)
		{
			sa_analyFFT(adata(_this->analysers, i), outAmptitudes);
			adata(_this->analyserIndicators, i).noDetect = (sa_isInited(adata(_this->analysers, i), NULL)?0:1);
		}
	}	
	_this->firstBuffer = false;
	now = getTickCount2();
}

struct MaybeSignal *ms_init(struct MaybeSignal *_this)
{
	ms_reset(_this);
	_this->isDiscoveryFinished = ms_isDiscoveryFinished;
	return _this;
}

void ms_reset(struct MaybeSignal *_this)
{
	_this->startPos = 0;
	_this->endPos = 0;
	_this->discoveryFinished = false;
	_this->recognizeFinished = false;
	_this->signalIdx = 0;
	_this->totalDetectCount = 1;
	_this->validDetectCount = 1;
	_this->startTime = 0;
	_this->endTime = 0;
}

void ms_onDetect(struct MaybeSignal *_this, bool _hasSignal)
{
	_this->totalDetectCount ++;
	if(_hasSignal)_this->validDetectCount ++;
}

bool ms_isDiscoveryFinished(struct MaybeSignal *_this)
{
	return _this->discoveryFinished;
}

struct MaybeSignalQueue *msq_init(struct MaybeSignalQueue *_this)
{
	cq_init(&_this->signalQueue, sizeof(struct MaybeSignal));
	ms_init(&_this->discardSignal);
	_this->currDiscoveryDiscarded = false;
	return _this;
}

void msq_finalize(struct MaybeSignalQueue *_this)
{
	cq_finalize(&_this->signalQueue);
}

void msq_clear(struct MaybeSignalQueue *_this)
{
	cq_clear(&_this->signalQueue);
	_this->currDiscoveryDiscarded = false;
}

static int MaybeSignalQueue_globalSignalIdx = 0;

struct MaybeSignal *msq_startDiscoverySignal(struct MaybeSignalQueue *_this, TPos _startPos, TPos _endPos)
{
	struct MaybeSignal *r = NULL;
	assert(_this->currDiscoveryDiscarded == false);
	if (cq_size(&_this->signalQueue) >= cq_capacity(&_this->signalQueue))
	{		
		r = &_this->discardSignal;
		ms_init(r);
		_this->currDiscoveryDiscarded = true;
	}
	else
	{
		struct MaybeSignal s;
		ms_init(&s);
		r = (struct MaybeSignal *)cq_push(&_this->signalQueue, &s);
	}
	r->signalIdx = MaybeSignalQueue_globalSignalIdx ++;
	r->startPos = _startPos;
	r->discoveryFinished = false;
	r->endPos = _endPos;
	r->startTime = getTickCount2();
	r->endTime = 0;
	return r;
}

struct MaybeSignal *msq_currDiscoveryingSignal(struct MaybeSignalQueue *_this)
{

	struct MaybeSignal * signal = NULL;
	if (_this->currDiscoveryDiscarded)
	{
		signal = &_this->discardSignal;
	}
	else
	{
		signal = cq_peekBotton(&_this->signalQueue);
	}
	if (signal && signal->discoveryFinished)
	{
		signal = NULL;
		assert(!_this->currDiscoveryDiscarded);
	}
	return signal;
}

void msq_endDiscoverySignal(struct MaybeSignalQueue *_this)
{
	struct MaybeSignal *signal;
	if (_this->currDiscoveryDiscarded)
	{
		signal = &_this->discardSignal;
		_this->currDiscoveryDiscarded = false;
	}
	else
	{
		signal = cq_peekBotton(&_this->signalQueue);
	}
	signal->discoveryFinished = true;
	signal->endTime = getTickCount2();
}

struct MaybeSignal *msq_startRecognizeSignal(struct MaybeSignalQueue *_this)
{
	struct MaybeSignal *signal = (struct MaybeSignal *)cq_peek(&_this->signalQueue);
	if(signal)
	{
		signal->recognizeFinished = false;

	}
	return signal;
}

void msq_endRecognizeSignal(struct MaybeSignalQueue *_this)
{
	struct MaybeSignal *signal = (struct MaybeSignal *)cq_peek(&_this->signalQueue);
	assert(signal != NULL && signal->recognizeFinished == false && signal->discoveryFinished == true);

	cq_pop(&_this->signalQueue);
}

struct FreqSignalDetector *fsd_init(struct FreqSignalDetector *_this, struct SignalDetector *_parentDetector, 
struct SignalAnalyserProcessIndicator *_analyserIndicator, int _sampleRate)
{
	_this->parentDetector = _parentDetector;
	_this->analyserIndicator = _analyserIndicator;
	_this->signalIdx = -1;
	_this->eventIdx = 0;
	_this->signalStartRelative = 0;

	_this->minNormalAmplitudeCount = 30;
	_this->minPeakAmplitudeTop2Avg = 4;
	_this->currNormalAmplitudeCount = 0;
	_this->currTempAmplitudeCount = 0;
	_this->sumNormalAmplitude = 0;
	_this->sumTempAmplitude = 0;
	_this->normalAmplitudeInited = false;
	_this->longTermAvgCount = 0;
	_this->longTermAvgAmp = 0;
	fsd_reset(_this);

	msq_init(&_this->maybeSignalQueue);
	strcpy(_this->maybeSignalQueue.name, "maybeSignalQueue");

	fsd_setFreqs(_this, DEFAULT_CODE_FREQUENCY);

	return _this;
}

void fsd_finalize(struct FreqSignalDetector *_this)
{
	msq_finalize(&_this->maybeSignalQueue);
}

void fsd_reset(struct FreqSignalDetector *_this)
{
	_this->preHasSignal = false;
	_this->prepreSignal = false;
	_this->preprepreSignal = false;
	_this->prepreprepreSignal = false;
}

void fsd_setFreqs(struct FreqSignalDetector *_this, int* _freqs)
{
	int i;
	asize1(_freqs, DEFAULT_CODE_FREQUENCY_SIZE);
	_this->minFrequency = MAX_INT;
	_this->maxFrequency = 0;
	for (i = 0; i < DEFAULT_CODE_FREQUENCY_SIZE; i ++)
	{
		if(adata(_freqs, i) < _this->minFrequency)
		{
			_this->minFrequency = adata(_freqs, i);
		}
		if(adata(_freqs, i) > _this->maxFrequency)
		{
			_this->maxFrequency = adata(_freqs, i);
		}
	}
	sprintf_s(_this->maybeSignalQueue.name, MAX_QUEUE_NAME_LENGTH, "maybeSignalQueue(%d-%d)", _this->minFrequency, _this->maxFrequency);
	_this->minFrequency = _this->minFrequency - FREQ_DISTANCE;
	_this->maxFrequency = _this->maxFrequency + FREQ_DISTANCE;
	if(_this->minFrequency < 0)_this->minFrequency = 0;
	if(_this->maxFrequency > _this->parentDetector->sampleRate/2)_this->maxFrequency = _this->parentDetector->sampleRate/2;
	_this->minFreqIdx = _this->minFrequency * SignalDetector_FFTSize / _this->parentDetector->sampleRate;
	_this->maxFreqIdx = _this->maxFrequency * SignalDetector_FFTSize / _this->parentDetector->sampleRate;
}

int fsdInitTime = 0;
void fsd_detect(struct FreqSignalDetector *_this, struct VoiceEvent *_event, struct kiss_fft_out *_fftOut, bool _ignore)
{
	float maxAmp = 0, max2Amp = 0;
	float sumAmp = 0;
	int i;
	float avgAmp = 0, max2avg = 0;
	enum DetectResult hasSignal_ = DR_Ignore;
	float *outAmptitudes = _this->parentDetector->outAmptitudes;
	asize1(outAmptitudes, SignalDetector_FFTSize_Default / 2 + 1);
	_this->hasNewSignal = false;
	if (!_ignore)
	{
		_this->signalStartRelative = 0;
		hasSignal_ = DR_No;

		for(i = _this->minFreqIdx; i < _this->maxFreqIdx; i ++)
		{

			adata(outAmptitudes, i) = fft_out_amp(_fftOut, i);
			if(adata(outAmptitudes, i) > maxAmp)maxAmp = adata(outAmptitudes, i);
			if(adata(outAmptitudes, i) < maxAmp && adata(outAmptitudes, i) > max2Amp)max2Amp = adata(outAmptitudes, i);
			sumAmp += adata(outAmptitudes, i);
		}
		sumAmp = sumAmp - maxAmp - max2Amp;

		if(sumAmp <= 0)sumAmp = 0.001f;
		avgAmp = sumAmp/(_this->maxFreqIdx - _this->minFreqIdx - 2);

		if(_this->longTermAvgCount < LONG_TERM_FSD_AVG_AMPLITUDE)
		{
			_this->longTermAvgAmp = (_this->longTermAvgAmp * _this->longTermAvgCount + avgAmp)/(_this->longTermAvgCount+1);
			_this->longTermAvgCount ++;
		}
		else
		{
			_this->longTermAvgAmp = (_this->longTermAvgAmp * (_this->longTermAvgCount-1) + avgAmp)/_this->longTermAvgCount;
		}
		max2avg = maxAmp/ ((_this->longTermAvgAmp > avgAmp)?avgAmp:_this->longTermAvgAmp);
		if(fsdInitTime == 0)fsdInitTime = getTickCount2();

		if(!_this->normalAmplitudeInited)
		{
			if(sumAmp > 0.005)
			{
				if (max2avg > 4.5)
				{

					_this->currNormalAmplitudeCount = -5;
					_this->sumNormalAmplitude = 0;
					pinfo2(TAG_VoiceRecognizer, "signalDetector init reset:%f at %d\n", max2avg, (_this->parentDetector->eventIdx-1));
				}
				else
				{
					if (_this->currNormalAmplitudeCount < 0)
					{
						_this->currNormalAmplitudeCount ++;
					}
					else
					{
						_this->sumNormalAmplitude += max2avg;
						if(++_this->currNormalAmplitudeCount >= _this->minNormalAmplitudeCount)
						{
							_this->minPeakAmplitudeTop2Avg = (_this->sumNormalAmplitude/_this->currNormalAmplitudeCount)*1.4;
							_this->normalAmplitudeInited = true;
						}
					}
					if (!_this->normalAmplitudeInited)
					{
						_this->sumTempAmplitude += max2avg;
						_this->currTempAmplitudeCount ++;
						_this->minPeakAmplitudeTop2Avg = (_this->sumTempAmplitude/_this->currTempAmplitudeCount) * 1.4;
					}
				}
			}
			hasSignal_ = DR_Ignore;
		}

		{
			float max2AvgTop5 = 0;
			hasSignal_ = (max2avg > _this->minPeakAmplitudeTop2Avg)?DR_Yes:DR_No;
#ifdef DETECT_NOISE_SIGNAL

			if (hasSignal_ == DR_Yes)
			{
#define TEST_NOISE_COUNT 4
				float *outAmptitudesTop5[TEST_NOISE_COUNT];
				float sumTop5 = 0, avgAmp = 0;
				int startSum = 1;
				topN((void **)(outAmptitudes + _this->minFreqIdx), _this->maxFreqIdx - _this->minFreqIdx, sizeof(float), ffloat_compare, (void **)outAmptitudesTop5, TEST_NOISE_COUNT);

				assert((*outAmptitudesTop5[0] - maxAmp)<0.0001f && (*outAmptitudesTop5[0] - maxAmp)>-0.0001f);

				if (abs(outAmptitudesTop5[1] - outAmptitudesTop5[0]) <= 1)
				{
					startSum = 2;
				}

				if (abs(outAmptitudesTop5[2] - outAmptitudesTop5[0]) <= 1)
				{
					if (startSum < 2)
					{
						outAmptitudesTop5[2] = outAmptitudesTop5[1];
						startSum = 2;
					}
					else
					{
						assert(startSum == 2);
						startSum = 3;
					}
				}
				for (i = startSum; i < TEST_NOISE_COUNT; i ++)
				{
					sumTop5 += *outAmptitudesTop5[i];
				}
				avgAmp = sumTop5/(TEST_NOISE_COUNT-startSum);
				max2AvgTop5 = *outAmptitudesTop5[0]/avgAmp;
				if (max2AvgTop5 < 2)
					hasSignal_ = DR_No;
			}
#endif
			if (hasSignal_ == DR_Yes && _this->parentDetector->eventIdx > 1)
			{
				_this->signalStartRelative = -SignalDetector_DetectEventDistance * _this->parentDetector->bufferDataSize;
			}
		}
	}

	if (hasSignal_ != DR_Ignore)
	{
		bool hasSignal = (hasSignal_ == DR_Yes);
		struct MaybeSignal *currSignal = msq_currDiscoveryingSignal(&_this->maybeSignalQueue);

		if (_this->eventIdx ++ > 160 && hasSignal_ != DR_Yes && _this->analyserIndicator->noDetect)
		{

			hasSignal_ = DR_Yes;
			hasSignal = true;
		}
		if(currSignal != NULL)ms_onDetect(currSignal, hasSignal);

		if(hasSignal)
		{
			if (_this->preHasSignal)
			{
				assert(!currSignal->discoveryFinished);
				currSignal->endPos = _event->eventStartPos + _event->byteStepSize;
			}
			else
			{
				if (_this->prepreSignal)
				{
					assert(!currSignal->discoveryFinished);
					currSignal->endPos = _event->eventStartPos + _event->byteStepSize;
				}
				else
				{
					if (_this->preprepreSignal)
					{
						assert(!currSignal->discoveryFinished);
						currSignal->endPos = _event->eventStartPos + _event->byteStepSize;
					}
					else
					{
						if (_this->prepreprepreSignal)
						{
							assert(!currSignal->discoveryFinished);
							currSignal->endPos = _event->eventStartPos + _event->byteStepSize;
						}
						else
						{
							TPos startPos = _event->eventStartPos;
							assert(currSignal == NULL || currSignal->discoveryFinished);
							pinfo(TAG_VoiceRecognizer, "first signal detected at %d\n", (_this->parentDetector->eventIdx-1));
							_this->signalIdx ++;
							if(_this->signalIdx < 2)
							{
#ifndef SILENCE
								myprintf("add 30k noise data to init recognizer\n", _this->signalIdx);
#endif
								startPos = startPos - SAVE_WAVE_EXTRA_LEN;
								if(startPos < 0)startPos = 0;
							}
							else
							{
								startPos = _event->eventStartPos + _this->signalStartRelative;
							}
							msq_startDiscoverySignal(&_this->maybeSignalQueue, startPos, _event->eventStartPos + _event->byteStepSize);

							_this->hasNewSignal = true;
						}
					}
				}
			}
		}
		else
		{
			if (_this->preHasSignal)
			{
				assert(!currSignal->discoveryFinished);
				currSignal->endPos = _event->eventStartPos + _event->byteStepSize;

			}
			else
			{
				if (_this->prepreSignal)
				{

					currSignal->endPos = _event->eventStartPos + _event->byteStepSize;
				}
				else
				{
					if (_this->preprepreSignal)
					{

						currSignal->endPos = _event->eventStartPos + _event->byteStepSize;
					}
					else
					{
						if (currSignal != NULL)
						{
							currSignal->endPos = _event->eventStartPos + _event->byteStepSize;
							msq_endDiscoverySignal(&_this->maybeSignalQueue);
						}
					}
				}
			}
		}

		_this->prepreprepreSignal = _this->preprepreSignal;
		_this->preprepreSignal = _this->prepreSignal;
		_this->prepreSignal = _this->preHasSignal;
		_this->preHasSignal = hasSignal;
	}
}

inline enum DetectResult fsd_hasSignal(struct FreqSignalDetector *_this)
{
	return _this->hasSignal_;
}

struct SignalDetector *sd_init(struct SignalDetector *_this, struct SignalAnalyserVoiceProcessor *_fvp, int _sampleRate, int _channel, int _bits, int _bufferSize, int _overlap)
{
	_this->parent.process = sd_process;
	_this->parent.setFreqs = sd_setFreqs;
	_this->parent.setListener = sd_setListener;
	_this->parent.getListener = sd_getListener;
#ifdef CONTROL_RECOGNIZER_INIT
	_this->parent.isNoiseLevelInited = vp_no_isNoiseLevelInited;
	_this->parent.reInitNoiseLevel = vp_no_reInitNoiseLevel;
#endif

	_this->fvp = _fvp;
	_this->sampleRate = _sampleRate;
	SignalDetector_FFTSize = SignalDetector_FFTSize_Default * _sampleRate/DEFAULT_SAMPLE_RATE;
	if(SignalDetector_FFTSize & 1)SignalDetector_FFTSize = SignalDetector_FFTSize-1;
	kiss_fft_init(&_this->fft, SignalDetector_FFTSize);
	mmsq_init(&_this->multiMaybeSignalQueue);
	_this->eventIdx = 0;
	_this->bufferSize = _bufferSize;
	_this->bufferDataSize = (_bufferSize - _overlap) * (((_bits + 7) / 8) * _channel);
	_this->floatBuffer = (float *)mymalloc(sizeof(float) * _this->bufferSize);
	_this->converter = getConverter(_channel, _bits, true, false);
	_this->normalAmplitudeInited = false;

	return _this;
}

void sd_finalize(struct SignalDetector *_this)
{
	int i;
	myfree(_this->floatBuffer);
	myfree(_this->converter);
	kiss_fft_finalize(&_this->fft);
	mmsq_finalize(&_this->multiMaybeSignalQueue);
	for (i = 0; i < _this->detectorCount; i ++)
	{
		_delete(_this->freqDetectors[i], fsd_finalize);
	}
}

void sd_reset(struct SignalDetector *_this)
{
	int i;
	asize1(_this->freqDetectors, _this->detectorCount);
	for (i = 0; i < _this->detectorCount; i ++)
	{
		fsd_reset(adata(_this->freqDetectors, i));
	}
}

void sd_setFreqs(struct VoiceProcessor *this_, int _freqsIdx, int* _freqs)
{
	struct SignalDetector *_this = (struct SignalDetector *)this_;
	asize1(_this->freqDetectors, _this->detectorCount);
	if (_freqsIdx == _this->detectorCount)
	{
		struct SignalAnalyserProcessIndicator *indicator = _this->fvp->getAnalyserIndicator(_this->fvp, _freqsIdx);
		sd_addFreqSignalDetector(_this, indicator);
		areset(_this->freqDetectors, _this->detectorCount);
	}
	else if(_freqsIdx > _this->detectorCount)
	{
		throwError("detector index not exists");
	}
	fsd_setFreqs(adata(_this->freqDetectors, _freqsIdx), _freqs);
}

void sd_setListener(struct VoiceProcessor *_this, int _freqsIdx, struct RecognitionListener *_listener)
{
}

struct RecognitionListener *sd_getListener(struct VoiceProcessor *_this, int _freqsIdx)
{
	return NULL;
}

void sd_addFreqSignalDetector(struct SignalDetector *_this, struct SignalAnalyserProcessIndicator *_analyserIndicator)
{
	asize1(_this->freqDetectors, FreqAnalyserCount);
	assert(_this->detectorCount < FreqAnalyserCount);
	adata(_this->freqDetectors, _this->detectorCount) = _new(struct FreqSignalDetector, fsd_init, _this, _analyserIndicator, _this->sampleRate);
	mmsq_addQueue(&_this->multiMaybeSignalQueue, &adata(_this->freqDetectors, _this->detectorCount)->maybeSignalQueue, _analyserIndicator);
	_this->detectorCount ++;
}

struct MultiMaybeSignalQueueWrapper *mmsq_init(struct MultiMaybeSignalQueueWrapper *_this)
{
	_this->queueCount = 0;
	_this->indicatePos = -1;
	_this->preEndPos = -1;
	_this->signalIdx = -1;
	_this->inRecognizingSignal = false;
	ms_init(&_this->maybeSignal);
	_this->maybeSignal.isDiscoveryFinished = mmsq_isDiscoveryFinished;
	mtx_init(&_this->m, mtx_plain);
	mymemset(_this->recogningMaySignals, 0, sizeof(_this->recogningMaySignals));
	return _this;
}

void mmsq_finalize(struct MultiMaybeSignalQueueWrapper *_this)
{
	mtx_destroy(&_this->m);
}

void mmsq_addQueue(struct MultiMaybeSignalQueueWrapper *_this, struct MaybeSignalQueue *_queue, struct SignalAnalyserProcessIndicator *_analyserIndicator)
{
	asize2(_this->maybeSignalQueues, FreqAnalyserCount, _this->analyserIndicators, FreqAnalyserCount);
	assert(_this->queueCount < FreqAnalyserCount);
	adata(_this->maybeSignalQueues, _this->queueCount) = _queue;
	adata(_this->analyserIndicators, _this->queueCount) = _analyserIndicator;
	_this->queueCount ++;
}

void mmsq_clear(struct MultiMaybeSignalQueueWrapper *_this)
{
	int i;
	asize1(_this->maybeSignalQueues, _this->queueCount);
	for (i = 0; i < _this->queueCount; i ++)
	{
		msq_clear(adata(_this->maybeSignalQueues, i));
	}
}

bool mmsq_isDiscoveryFinished(struct MaybeSignal *this_)
{
	struct MultiMaybeSignalQueueWrapper *_this = (struct MultiMaybeSignalQueueWrapper *)this_;
	asize1(_this->recogningMaySignals, _this->queueCount);
	if(_this->queueCount == 1)
		return this_->discoveryFinished;
	else
	{			
		int i;
		bool result = true;	
		for (i = 0; i < _this->queueCount; i ++)
		{
			if (result && adata(_this->recogningMaySignals, i) != NULL && !adata(_this->recogningMaySignals, i)->discoveryFinished)
			{
				result = false;
			}
			if (adata(_this->recogningMaySignals, i) != NULL && adata(_this->recogningMaySignals, i)->endPos > _this->maybeSignal.endPos)
			{
				_this->maybeSignal.endPos = adata(_this->recogningMaySignals, i)->endPos;
			}
			if(adata(_this->recogningMaySignals, i) != NULL && adata(_this->recogningMaySignals, i)->validDetectCount > _this->maybeSignal.validDetectCount)
			{
				_this->maybeSignal.validDetectCount = adata(_this->recogningMaySignals, i)->validDetectCount;
			}
		}
		_this->maybeSignal.discoveryFinished = result;
		return result;
	}
}

void mmsq_indicate(struct MultiMaybeSignalQueueWrapper *_this, TPos _processDataStart, TPos _processDataEnd)
{
	if (_this->queueCount > 1)
	{
		int i;
		struct MaybeSignal *currSignal;
		asize2(_this->recogningMaySignals, _this->queueCount, _this->analyserIndicators, _this->queueCount);
		_this->indicatePos = _processDataEnd;
		for (i = 0; i < _this->queueCount; i ++)
		{
			currSignal = adata(_this->recogningMaySignals, i);
			adata(_this->analyserIndicators, i)->isAnalyserProcess = (currSignal != NULL 
				&& (currSignal->discoveryFinished == false || (currSignal->endPos - currSignal->startPos) > minProcessSignalSize)
				&& _processDataEnd > currSignal->startPos
				&& _processDataStart < currSignal->endPos);
			if (adata(_this->recogningMaySignals, i) != NULL && adata(_this->recogningMaySignals, i)->endPos > _this->maybeSignal.endPos)
			{
				_this->maybeSignal.endPos = adata(_this->recogningMaySignals, i)->endPos;

			}
		}
	}
}

void mmsq_endAllDiscoveryingSignals(struct MultiMaybeSignalQueueWrapper *_this)
{
	int i;
	asize2(_this->recogningMaySignals, _this->queueCount, _this->maybeSignalQueues, _this->queueCount);
	for (i = 0; i < _this->queueCount; i ++)
	{
		struct MaybeSignal *currDiscoverySignal = msq_currDiscoveryingSignal(adata(_this->maybeSignalQueues, i));
		if (currDiscoverySignal != NULL && !currDiscoverySignal->discoveryFinished)
		{

			msq_endDiscoverySignal(adata(_this->maybeSignalQueues, i));
		}
	}
}

struct MaybeSignal *mmsq_startRecognizeSignal(struct MultiMaybeSignalQueueWrapper *_this)
{
	asize2(_this->recogningMaySignals, _this->queueCount, _this->maybeSignalQueues, _this->queueCount);
	assert(_this->inRecognizingSignal == false);
	if (_this->queueCount == 1)
	{
		struct MaybeSignal *tempMaybeSignal = msq_startRecognizeSignal(adata(_this->maybeSignalQueues, 0));
		_this->inRecognizingSignal = tempMaybeSignal != NULL;
		return tempMaybeSignal;
	}
	else
	{
		int i, minNewSignalIdx = -1;
		struct MaybeSignal *tempSignal, *minNewSignal = NULL;
		int signalCount = 0;		

		for (i = 0; i < _this->queueCount; i ++)
		{			
			tempSignal = msq_startRecognizeSignal(adata(_this->maybeSignalQueues, i));
			adata(_this->recogningMaySignals, i) = NULL;
			if (tempSignal != NULL)
			{
				adata(_this->recogningMaySignals, i) = tempSignal;

				if (minNewSignalIdx < 0 || minNewSignal->startPos > tempSignal->startPos)
				{
					minNewSignalIdx = i;
					minNewSignal = tempSignal;
				}
				signalCount ++;
			}
		}
		if (minNewSignal != NULL)
		{
			assert(minNewSignalIdx >= 0);
			_this->inRecognizingSignal = true;
			ms_reset(&_this->maybeSignal);
			_this->maybeSignal.signalIdx = ++_this->signalIdx;
			_this->maybeSignal.startPos = minNewSignal->startPos;
			_this->maybeSignal.endPos = minNewSignal->endPos;
			_this->maybeSignal.startTime = minNewSignal->startTime;
			_this->maybeSignal.recognizeStartTime = minNewSignal->recognizeStartTime;

			if(signalCount > 1)			
			{
				for (i = 0; i < _this->queueCount; i ++)
				{
					if(adata(_this->recogningMaySignals, i) != NULL)
					{
						if(adata(_this->recogningMaySignals, i)->startPos <= _this->maybeSignal.endPos)
						{
							if(adata(_this->recogningMaySignals, i)->endPos > _this->maybeSignal.endPos)
							{
								_this->maybeSignal.endPos = adata(_this->recogningMaySignals, i)->endPos;
								if(i > 0)i = 0;
							}
						}
						else
						{
							adata(_this->recogningMaySignals, i) = NULL;
						}
					}
				}
			}
			_this->recognizeSyned = false;
			return &_this->maybeSignal;
		}
	}

	return NULL;
}

void mmsq_synRecognizeSignal(struct MultiMaybeSignalQueueWrapper *_this)
{
	if(_this->queueCount > 1 && !_this->recognizeSyned && !_this->maybeSignal.discoveryFinished)
	{
		int i;
		struct MaybeSignal *tempSignal;
		bool allSyned = true;
		asize2(_this->recogningMaySignals, _this->queueCount, _this->maybeSignalQueues, _this->queueCount);
		for (i = 0; i < _this->queueCount; i ++)
		{
			if(adata(_this->recogningMaySignals, i) == NULL)
			{
				tempSignal = msq_startRecognizeSignal(adata(_this->maybeSignalQueues, i));
				if(tempSignal != NULL)
				{
					if(tempSignal->startPos <= _this->maybeSignal.endPos)
					{
						adata(_this->recogningMaySignals, i) = tempSignal;
						if(tempSignal->endPos > _this->maybeSignal.endPos)
						{
							_this->maybeSignal.endPos = tempSignal->endPos;
						}
					}
				}
				else
				{
					allSyned = false;
				}
			}
		}
		_this->recognizeSyned = allSyned;
	}
}

void mmsq_endRecognizeSignal(struct MultiMaybeSignalQueueWrapper *_this)
{
	asize2(_this->recogningMaySignals, _this->queueCount, _this->maybeSignalQueues, _this->queueCount);
	assert(_this->inRecognizingSignal == true);
	if (_this->queueCount == 1)
	{
		msq_endRecognizeSignal(adata(_this->maybeSignalQueues, 0));
		_this->inRecognizingSignal = false;
	}
	else
	{
		int i;
		for (i = 0; i < _this->queueCount; i ++)
		{
			assert(adata(_this->recogningMaySignals, i) == NULL || adata(_this->recogningMaySignals, i)->endPos <= _this->indicatePos);
			if(adata(_this->recogningMaySignals, i) != NULL)
			{
				assert(adata(_this->recogningMaySignals, i) == msq_startRecognizeSignal(adata(_this->maybeSignalQueues, i)));
				assert(adata(_this->recogningMaySignals, i)->endPos <= _this->maybeSignal.endPos);
				msq_endRecognizeSignal(adata(_this->maybeSignalQueues, i));
			}
		}
		_this->inRecognizingSignal = false;
		_this->preEndPos = _this->maybeSignal.endPos;
	}
}

void sd_process(struct VoiceProcessor *this_, struct VoiceEvent *_event)
{
	struct SignalDetector *_this = (struct SignalDetector *)this_;
	struct BufferData *data;
	struct kiss_fft_out out;
	int i;
	bool ignore = true;
	asize1(_this->freqDetectors, _this->detectorCount);
	_this->hasNewSignal = false;
	if(!_this->normalAmplitudeInited)
	{
		bool allInited = true;
		for (i = 0; i < _this->detectorCount; i ++)
		{
			if(!adata(_this->freqDetectors, i)->normalAmplitudeInited)
			{
				allInited = false;
				break;
			}
		}
		_this->normalAmplitudeInited = allInited;
	}

	if ((_event->data == NULL || bd_isNULL(_event->data))
		|| (_this->eventIdx++)%SignalDetector_DetectEventDistance == 0 
		|| !_this->normalAmplitudeInited)
	{
		ignore = false;
		data = _event->data;

		_this->converter->toFloatArray(bd_getData(data), _this->floatBuffer, SignalDetector_FFTSize);
		out = kiss_fft_execute(&_this->fft, _this->floatBuffer);		
	}
	for (i = 0; i < _this->detectorCount; i ++)
	{
		fsd_detect(adata(_this->freqDetectors, i), _event, (ignore?NULL:&out), ignore);
		_this->hasNewSignal = _this->hasNewSignal || adata(_this->freqDetectors, i)->hasNewSignal;
	}
}

struct PreprocessVoiceProcessor *pvp_init(struct PreprocessVoiceProcessor *_this, int _sampleRate, int _channel, int _bits, int _bufferSize, int _overlap)
{
	struct SignalAnalyserVoiceProcessor *realProcessor = NULL;

	_this->parent.process = pvp_process;
	_this->parent.setFreqs = pvp_setFreqs;
	_this->parent.setListener = pvp_setListener;
	_this->parent.getListener = pvp_getListener;
#ifdef CONTROL_RECOGNIZER_INIT
	_this->parent.isNoiseLevelInited = pvp_isNoiseLevelInited;
	_this->parent.reInitNoiseLevel = pvp_reInitNoiseLevel;
#endif

	realProcessor = (struct SignalAnalyserVoiceProcessor *)_new(struct FFTVoiceProcessor,  fvp_init,
		_sampleRate, _channel, _bits, _bufferSize, _overlap);
	_this->realProcessor = realProcessor;

	mtx_init(&_this->m, mtx_plain);
	cnd_init(&_this->cond);
	cb_init(&_this->voiceHistory, hisToryCacheSize);
	_this->sampleRate = _sampleRate;
	_this->channel = _channel;
	_this->bits = _bits;
	_this->bufferSize = _bufferSize;
	_this->overlap = _overlap;
	_this->stopped = false;
	_this->listener = NULL;
	_this->realProcessing = false;
	_this->firstBuffer = true;
	_this->allowDiscardSignal = true;
	_this->signalIdx = -1;
	_this->floatOverlap = _this->overlap;
	_this->floatStepSize = _this->bufferSize - _this->floatOverlap;
	_this->frameSize = ((_this->bits + 7) / 8) * _this->channel;
	_this->byteOverlap = _this->floatOverlap * _this->frameSize;
	_this->byteStepSize = _this->floatStepSize * _this->frameSize;

	sd_init(&_this->signalDetector, realProcessor, _sampleRate, _channel, _bits, _bufferSize, _overlap);
	sd_addFreqSignalDetector(&_this->signalDetector, realProcessor->getAnalyserIndicator(realProcessor, 0));
	_this->maybeSignalQueue = &_this->signalDetector.multiMaybeSignalQueue;

	_this->realProcessNULL = true;
	thrd_create(&_this->realProcessThread, pvp_realProcess, _this);
	_this->realProcessNULL = false;
	return _this;
}

void pvp_finalize(struct PreprocessVoiceProcessor *_this)
{
	sd_finalize(&_this->signalDetector);
	cb_finalize(&_this->voiceHistory);
	mtx_destroy(&_this->m);
	cnd_destroy(&_this->cond);
	_delete((struct FFTVoiceProcessor *)_this->realProcessor, fvp_finalize);
}

void pvp_process(struct VoiceProcessor *this_, struct VoiceEvent *_event)
{
	struct PreprocessVoiceProcessor *_this = (struct PreprocessVoiceProcessor *)this_;
	TPos eventStartPos;
	if (_event->data == NULL || bd_isNULL(_event->data))
	{

		mmsq_endAllDiscoveryingSignals(_this->maybeSignalQueue);
		_this->stopped = true;
		cnd_broadcast(&_this->cond);
		while(!_this->realProcessThreadQuited)
		{
			mysleep(50);

		}
		return;
	}

	if(_this->realProcessNULL)
	{

		_this->stopped = false;
		_this->realProcessing = false;
		_this->firstBuffer = true;
		sd_reset(&_this->signalDetector);

		mmsq_clear(_this->maybeSignalQueue);
		thrd_create(&_this->realProcessThread, pvp_realProcess, _this);
		_this->realProcessNULL = false;
	}

	eventStartPos = cb_write(&_this->voiceHistory, bd_getData(_event->data) + (_this->firstBuffer?0:_this->byteOverlap), (_this->firstBuffer?bd_getMaxBufferSize(_event->data):_this->byteStepSize));
	_event->eventStartPos = eventStartPos;
	_event->byteStepSize = _this->byteStepSize;
	_this->firstBuffer = false;

	sd_process((struct VoiceProcessor *)(&_this->signalDetector), _event);
	if (_this->signalDetector.hasNewSignal)
	{

		cnd_broadcast(&_this->cond);
	}
}

void pvp_setFreqs(struct VoiceProcessor *this_, int _freqsIdx, int* _freqs)
{
	struct PreprocessVoiceProcessor *_this = (struct PreprocessVoiceProcessor *)this_;	
	_this->realProcessor->parent.setFreqs((struct VoiceProcessor *)_this->realProcessor, _freqsIdx, _freqs);
	sd_setFreqs((struct VoiceProcessor *)&_this->signalDetector, _freqsIdx, _freqs);
}

void pvp_setListener(struct VoiceProcessor *this_, int _freqsIdx, struct RecognitionListener *_listener)
{
	struct PreprocessVoiceProcessor *_this = (struct PreprocessVoiceProcessor *)this_;
	_this->realProcessor->parent.setListener((struct VoiceProcessor *)_this->realProcessor, _freqsIdx, _listener);
}

struct RecognitionListener *pvp_getListener(struct VoiceProcessor *this_, int _freqsIdx)
{
	struct PreprocessVoiceProcessor *_this = (struct PreprocessVoiceProcessor *)this_;
	return _this->realProcessor->parent.getListener((struct VoiceProcessor *)_this->realProcessor, _freqsIdx);
}

#ifdef CONTROL_RECOGNIZER_INIT
bool pvp_isNoiseLevelInited(struct VoiceProcessor *this_, int _freqIdx, float *_noiseLevel)
{
	struct PreprocessVoiceProcessor *_this = (struct PreprocessVoiceProcessor *)this_;
	return _this->realProcessor->parent.isNoiseLevelInited((struct VoiceProcessor *)_this->realProcessor, _freqIdx, _noiseLevel);
}

void pvp_reInitNoiseLevel(struct VoiceProcessor *this_, int _freqIdx, float _noiseLevel)
{
	struct PreprocessVoiceProcessor *_this = (struct PreprocessVoiceProcessor *)this_;
	_this->realProcessor->parent.reInitNoiseLevel((struct VoiceProcessor *)_this->realProcessor, _freqIdx, _noiseLevel);
}
#endif

int pvp_realProcess(void *_p)
{
	struct PreprocessVoiceProcessor *_this = (struct PreprocessVoiceProcessor *)_p;
	struct BufferData data;
	struct VoiceEvent event;
	int floatOverlap, floatStepSize;
	int byteOverlap, byteStepSize;
	struct MaybeSignal *signal;
	TPos signalEndPos;
	bool readDataEnd;
	TPos recognizePos, processPos;
	int bufferDataPos = 0, bufferDataStart = 0, readDataLen;
	struct FFTVoiceProcessor *fftVoiceProcessor = NULL;
	TFFTSignalAnalyser *analyser;
	int realReadLen;
	unsigned long now;

#ifndef SILENCE
#ifdef LINUX
	myprintf("real voice recognizer thread start:%d\n", getpid());
#endif
#endif

	minProcessSignalSize = (int)(((long long)DefaultMinProcessSignalSize) * _this->sampleRate/44100);
	minSaveSignalSize = (int)(((long long)DefaultMinSaveSignalSize) * _this->sampleRate/44100);
	bd_init(&data, _this->bufferSize * (((_this->bits + 7) / 8) * _this->channel));	
	floatOverlap = _this->floatOverlap;
	floatStepSize = _this->floatStepSize;
	byteOverlap = _this->byteOverlap;
	byteStepSize = _this->byteStepSize;
	_this->realProcessThreadQuited = false;

	while(!_this->stopped)
	{
#ifndef COND_NO_MUTEX
		mtx_lock(&_this->m);
#endif
		cnd_wait(&_this->cond, &_this->m);
#ifndef COND_NO_MUTEX
		mtx_unlock(&_this->m);
#endif

		_this->realProcessing = true;

		signal = mmsq_startRecognizeSignal(_this->maybeSignalQueue);
		while(signal)
		{
			while(signal->signalIdx > 2 
				&& !signal->isDiscoveryFinished(signal) 
				&& ((signal->endPos - signal->startPos) < minProcessSignalSize
				|| (signal->validDetectCount <= MinValidDetect)
				|| (((float)signal->validDetectCount)/signal->totalDetectCount <= 0.6)
				))
				mysleep(5);
			mmsq_synRecognizeSignal(_this->maybeSignalQueue);
			if (signal->signalIdx > 2 
				&& signal->isDiscoveryFinished(signal) 
				&& ((signal->endPos - signal->startPos) < minProcessSignalSize 
				|| (signal->validDetectCount <= MinValidDetect)
				|| (((float)signal->validDetectCount)/signal->totalDetectCount <= 0.6)
				))
			{
				mmsq_indicate(_this->maybeSignalQueue, signal->startPos, signal->endPos);
				mmsq_endRecognizeSignal(_this->maybeSignalQueue);
				signal = mmsq_startRecognizeSignal(_this->maybeSignalQueue);
				continue;
			}

			readDataEnd = false;
			processPos = signal->startPos;
			recognizePos = processPos;
			bufferDataPos = bufferDataStart = 0;
			readDataLen = bd_getMaxBufferSize(&data) - bufferDataPos;

			fftVoiceProcessor = NULL;
			pinfo(TAG_VoiceRecognizer, "signal range %d start to process\n", signal->signalIdx);

			while(true)
			{
				readDataLen = bd_getMaxBufferSize(&data) - bufferDataPos;
				signalEndPos = signal->endPos - recognizePos;
				if (signalEndPos < readDataLen)
				{
					readDataLen = (int)signalEndPos;
				}
				assert(readDataLen >= 0);
				assert(readDataLen + bufferDataPos <= bd_getMaxBufferSize(&data));
				realReadLen = cb_read(&_this->voiceHistory, recognizePos, bd_getData(&data) + bufferDataPos, readDataLen);
				if (realReadLen < 0)
				{
					myprintf("error error: signal %d(finished:%d)(start:%lld-curr:%lld-end:%lld) is out of date(%lld-%lld) ------------------\n", signal->signalIdx, signal->isDiscoveryFinished(signal), signal->startPos, processPos, signal->endPos, cb_pos(&_this->voiceHistory)-cb_size(&_this->voiceHistory), cb_pos(&_this->voiceHistory));

					assert(recognizePos < (cb_pos(&_this->voiceHistory)-cb_size(&_this->voiceHistory)));
					bufferDataPos = bufferDataStart = 0;
					processPos = recognizePos = signal->endPos;

				}
				if (realReadLen <= 0)
				{
					if (signal->isDiscoveryFinished(signal) && signal->endPos <= recognizePos)
					{
						assert(processPos <= recognizePos);
						mmsq_indicate(_this->maybeSignalQueue, processPos, signal->endPos);
						vevent_reset(&event, NULL);
						_this->realProcessor->parent.process((struct VoiceProcessor *)_this->realProcessor, &event);
						now = getTickCount2();
						pinfo(TAG_VoiceRecognizer, "signal range %d process success\n\n",signal->signalIdx);

						break;
					}
					else
					{
						mysleep(20);
						continue;
					}
				}

				bufferDataPos += realReadLen;
				recognizePos += realReadLen;
				assert(bufferDataPos <= bd_getMaxBufferSize(&data));
				if (bufferDataPos >= bd_getMaxBufferSize(&data))
				{
					assert(bufferDataPos == bd_getMaxBufferSize(&data));
					mmsq_indicate(_this->maybeSignalQueue, processPos, processPos + byteStepSize);

					vevent_reset(&event, &data);

					_this->realProcessor->parent.process((struct VoiceProcessor *)_this->realProcessor, &event);

					mymemmove(bd_getData(&data), bd_getData(&data) + byteStepSize, byteOverlap);
					processPos += (bd_getMaxBufferSize(&data) - bufferDataStart);
					assert(processPos == recognizePos);
					bufferDataPos = bufferDataStart = byteOverlap; 
					readDataLen = bd_getMaxBufferSize(&data) - bufferDataPos;
				}
			}
			mmsq_endRecognizeSignal(_this->maybeSignalQueue);
			signal = mmsq_startRecognizeSignal(_this->maybeSignalQueue);
		}
		_this->realProcessing = false;
	}
	_this->realProcessThreadQuited = true;
	bd_finalize(&data);
	_this->realProcessNULL = true;

	return 0;
}

struct VoiceRecognizer *vrr_init(struct VoiceRecognizer *_this, enum ProcessorType _processorType, int _sampleRate, int _channel, int _bits, int _bufferSize, int _overlap)
{
	b_init2(&_this->buffer, DEFAULT_BUFFER_COUNT, _bufferSize * (((_bits + 7) / 8) * _channel), true);
	_this->bufferSize = _bufferSize;
	_this->overlap = _overlap;
	_this->channel = _channel;
	_this->bits = _bits;
	_this->writer = NULL;
	_this->parent.freeBuffer = vrr_freeBuffer;
	_this->parent.getBuffer = vrr_getBuffer;
	_this->parent.tryGetBuffer = vrr_tryGetBuffer;
	_this->currBuffer = NULL;
	_this->currBufferPos = 0;

	_this->stopped = true;
	_this->signalContinueTime = 0;
	_this->paused = false;
	_this->freqsCount = 1;
#ifdef BUFFER_NO_OVERLAP	
	_this->bufDataSize = _bufferSize * (((_bits + 7) / 8) * _channel);
	_this->bufDataOverlap = _overlap * (((_bits + 7) / 8) * _channel);
	_this->bufDataPos = 0;
	bd_init(&_this->bufData, _this->bufDataSize);
#endif
	_this->processorType = _processorType;
	if (_processorType == PreprocessVoiceProcessorType)
	{
		_this->voiceProcessor = (struct VoiceProcessor *)_new(struct PreprocessVoiceProcessor, pvp_init, _sampleRate, _channel, _bits, _bufferSize, _overlap);
	}
	else if (_processorType == FFTVoiceProcessorType)
	{
		_this->voiceProcessor = (struct VoiceProcessor *)_new(struct FFTVoiceProcessor, fvp_init, _sampleRate, _channel, _bits, _bufferSize, _overlap);
	}

	return _this;
}

void vrr_finalize(struct VoiceRecognizer *_this)
{
	int i;
#ifdef BUFFER_NO_OVERLAP	
	bd_finalize(&_this->bufData);
#endif	
	b_finalize(&_this->buffer);
	if(_this->writer != NULL)_delete(_this->writer, bdw_finalize);
	if (_this->processorType == PreprocessVoiceProcessorType)
	{
		_delete((struct PreprocessVoiceProcessor *)_this->voiceProcessor, pvp_finalize);
	}
	else if (_this->processorType == FFTVoiceProcessorType)
	{
		_delete((struct FFTVoiceProcessor *)_this->voiceProcessor, fvp_finalize);
	}
}

struct BufferData *vrr_getBuffer(struct BufferSource *__this)
{
	struct VoiceRecognizer *_this = (struct VoiceRecognizer *)__this;
	return b_getEmpty(&_this->buffer);
}

struct BufferData *vrr_tryGetBuffer(struct BufferSource *__this)
{
	struct VoiceRecognizer *_this = (struct VoiceRecognizer *)__this;
	return b_tryGetEmpty(&_this->buffer);
}

void vrr_freeBuffer(struct BufferSource *__this, struct BufferData *data)
{
	struct VoiceRecognizer *_this = (struct VoiceRecognizer *)__this;
	b_putFull(&_this->buffer, data);
}

struct BufferDataWriter *vrr_getBufferWriter(struct VoiceRecognizer *_this, bool _create)
{
	if (_create && _this->writer == NULL)
	{
		_this->writer = _new(struct BufferDataWriter, bdw_init, (struct BufferSource *)_this, _this->channel, _this->bits, _this->bufferSize, _this->overlap, false);
	}
	return _this->writer;
}

int bufferIdx = 0;
int runIdx;
void vrr_run(struct VoiceRecognizer *_this)
{
	struct VoiceEvent event;
	struct BufferData *data;

	vrr_setStopped(_this, false);
	b_reset(&_this->buffer);	
	while (true)
	{
		bool paused = false;
		data = b_getFull(&_this->buffer);

		if(_this->paused)
		{
			unsigned long now = getTickCount2();
			paused = now < _this->signalContinueTime;
			if(!paused)_this->paused = false;
		}
		if(!(paused && !bd_isNULL(data)))
		{
			vevent_reset(&event, data);
			_this->voiceProcessor->process(_this->voiceProcessor, &event);
		}
		if(!bd_isNULL(data))
		{
			b_putEmpty(&_this->buffer, data);
		}
		else break;
	}
	vrr_setStopped(_this, true);

}

void vrr_runOnce(struct VoiceRecognizer *_this)
{
	struct BufferData *data = b_tryGetFull(&_this->buffer);
	if(data != NULL)
	{
		struct VoiceEvent event;

		bufferIdx ++;
		if(!bd_isNULL(data))
		{
#ifdef BUFFER_NO_OVERLAP
			char *bufDataAddr = bd_getData(&_this->bufData);
			char *dataAddr = bd_getData(data);
			int dataPos = 0, dataMaxLen = bd_getMaxBufferSize(data);
			int canCopy = 0, i;

			while(dataPos < dataMaxLen)
			{
				canCopy = min(dataMaxLen-dataPos, _this->bufDataSize-_this->bufDataPos);
				mymemcpy(bufDataAddr + _this->bufDataPos, dataAddr + dataPos, canCopy);
				_this->bufDataPos += canCopy;
				dataPos += canCopy;
				if (_this->bufDataPos >= _this->bufDataSize)
				{
					vevent_reset(&event, &_this->bufData);
					_this->voiceProcessor->process(_this->voiceProcessor, &event);	
					mymemmove(bufDataAddr, bufDataAddr + _this->bufDataSize - _this->bufDataOverlap, _this->bufDataOverlap);
					_this->bufDataPos = _this->bufDataOverlap;
				}
			}				
#else
			vevent_reset(&event, data);
			_this->voiceProcessor->process(_this->voiceProcessor, &event);
#endif
		}
		if(!bd_isNULL(data))
		{
			b_putEmpty(&_this->buffer, data);
		}
	}
}

void vrr_pause(struct VoiceRecognizer *_this, int _microSeconds)
{
	if(_microSeconds > 0)
	{
		_this->signalContinueTime = getTickCount2() + (unsigned long)_microSeconds;
		_this->paused = true;
	}
	else
	{
		_this->paused = false;
	}
}

inline struct BufferSource *vrr_getBufferSource(struct VoiceRecognizer *_this)
{
	return (struct BufferSource *)_this;
}

int vrr_writeData(struct VoiceRecognizer *_this, char *_buf, int _bufSize)
{
	int copySize, bufPos = 0, maxBufSize = 0;
	struct BufferSource *bufferSource = vrr_getBufferSource(_this);
	if(_this->currBuffer != NULL)maxBufSize = bd_getMaxBufferSize(_this->currBuffer);
	while (_bufSize > bufPos)
	{
		if(_this->currBuffer == NULL)
		{
			_this->currBuffer = bufferSource->getBuffer(bufferSource);
			maxBufSize = bd_getMaxBufferSize(_this->currBuffer);
			_this->currBufferPos = 0;
		}
		copySize = ((_bufSize - bufPos) > (maxBufSize - _this->currBufferPos)?(maxBufSize - _this->currBufferPos):(_bufSize - bufPos));
		mymemcpy(bd_getData(_this->currBuffer) + _this->currBufferPos, _buf + bufPos, copySize);
		bufPos += copySize;
		_this->currBufferPos += copySize;
		if (_this->currBufferPos >= maxBufSize)
		{
			bufferSource->freeBuffer(bufferSource, _this->currBuffer);
			_this->currBuffer = NULL;
		}
	}
	return bufPos;
}

void vrr_nextWriteBuff(struct VoiceRecognizer *_this, char **_buf, int *_bufSize)
{
	struct BufferSource *bufferSource = vrr_getBufferSource(_this);
	if(_this->currBuffer != NULL)bufferSource->freeBuffer(bufferSource, _this->currBuffer);
	if (_buf != NULL)
	{
		_this->currBuffer = bufferSource->tryGetBuffer(bufferSource);
		if(_this->currBuffer != NULL)
		{
			bd_reset(_this->currBuffer);
			*_buf = bd_getData(_this->currBuffer);
			*_bufSize = bd_getMaxBufferSize(_this->currBuffer);
		}
		else
		{
			*_buf = NULL;
			*_bufSize = 0;
		}
	}
}

void vrr_flush(struct VoiceRecognizer *_this)
{
	if (_this->currBuffer != NULL)
	{
		struct BufferSource *bufferSource = vrr_getBufferSource(_this);
		bufferSource->freeBuffer(bufferSource, _this->currBuffer);
		_this->currBuffer = NULL;
	}
}

struct RecognitionListener* vrr_getListener(struct VoiceRecognizer *_this, int _idx)
{
	assert(_idx < _this->freqsCount);
	return _this->voiceProcessor->getListener(_this->voiceProcessor, _idx);
}

void vrr_setListener(struct VoiceRecognizer *_this, int _idx, struct RecognitionListener* _listener)
{
	assert(_idx < _this->freqsCount);
	_this->voiceProcessor->setListener(_this->voiceProcessor, _idx, _listener);
}

bool vrr_isStopped(struct VoiceRecognizer *_this)
{
	return _this->stopped;
}

void vrr_setStopped(struct VoiceRecognizer *_this, bool _stopped)
{
	_this->stopped = _stopped;
}

void vrr_setFreqs(struct VoiceRecognizer *_this, int _idx, int* _freqs, int _freqCount)
{
	if (_freqCount != DEFAULT_CODE_FREQUENCY_SIZE)
	{
		throwError("invalidate freqs setting");
	}
	_this->voiceProcessor->setFreqs(_this->voiceProcessor, _idx, _freqs);
	if(_idx == _this->freqsCount)
	{
		_this->freqsCount ++;
	}
	assert(_idx < _this->freqsCount);
}

#ifdef CONTROL_RECOGNIZER_INIT
bool vrr_isInited(struct VoiceRecognizer *_this, int _freqIdx, float *_noiseLevel)
{
	return _this->voiceProcessor->isNoiseLevelInited(_this->voiceProcessor, _freqIdx, _noiseLevel);
}

void vrr_reInit(struct VoiceRecognizer *_this, int _freqIdx, float _noiseLevel)
{
	_this->voiceProcessor->reInitNoiseLevel(_this->voiceProcessor, _freqIdx, _noiseLevel);
}
#endif

int vrr_getFreqsCount(struct VoiceRecognizer *_this)
{
	return _this->freqsCount;
}

void AudioFloatConversion16SL_toFloatArray(cchar *in_buff, float* out_buff, int out_len)
{
	short temp;
	int ox = 0, ix;
	asize2(in_buff, out_len * 2, out_buff, out_len);
	for (ox = 0, ix = 0; ox < out_len; ox++) {
		temp = ((short) (((adata(in_buff, ix)) & 0xFF) | ((adata(in_buff, ix+1)) << 8)));
		adata(out_buff, ox) = temp * (1.0f / 32767.0f);
		ix += 2;
	}
}

void AudioShortConversion16SL_toLong(char *in_buff, long *out_buff, int out_len)
{
	short temp;
	int ox = 0, ix;
	asize2(in_buff, out_len * 2, out_buff, out_len);
	for (ox = 0, ix = 0; ox < out_len; ox++) {
		temp = ((short) (((adata(in_buff, ix)) & 0xFF) | ((adata(in_buff, ix+1)) << 8)));

		adata(out_buff, ox) = temp;
		ix += 2;
	}
}

void AudioShortConversion16SL_toShort(cchar *in_buff, short *out_buff, int out_len)
{
	int ox = 0, ix;
	asize2(in_buff, out_len * 2, out_buff, out_len);
	for (ox = 0, ix = 0; ox < out_len; ox++) {
		adata(out_buff, ox) = ((short) (((adata(in_buff, ix)) & 0xFF) | ((adata(in_buff, ix+1)) << 8)));
		ix += 2;
	}
}

struct AudioConverter *getConverter(int _channel, int _bits, bool _signed, bool _bigEndian)
{
	if(!_bigEndian && _signed && (_bits > 8 && _bits <= 16))
	{
		struct AudioConverter *floatConverter16SL = (struct AudioConverter *)mymalloc(sizeof(struct AudioConverter));
		floatConverter16SL->toFloatArray = AudioFloatConversion16SL_toFloatArray;
		floatConverter16SL->toShort = AudioShortConversion16SL_toShort;
		floatConverter16SL->toLong = AudioShortConversion16SL_toLong;
		return floatConverter16SL;
	}
	throwError("not support audio converter");
	return NULL;
}

struct CircleBuffer *cb_init(struct CircleBuffer *_this, int _bufSize)
{
	_this->buf = (cchar *)mymalloc(sizeof(cchar) * _bufSize);
	_this->bufSize = _bufSize;
	_this->pos = 0;
	return _this;
}

void cb_finalize(struct CircleBuffer *_this)
{
	if (_this->buf != NULL)
	{
		myfree(_this->buf);
	}
}

TPos cb_write(struct CircleBuffer *_this, cchar *_buf, int _bufLen)
{
	int bufpos = _this->pos % _this->bufSize;
	TPos start = _this->pos;
	if ((_this->bufSize - bufpos) >= _bufLen)
	{
		mymemcpy(_this->buf + bufpos, _buf, _bufLen);
	}
	else
	{

		int firstPart = _this->bufSize - bufpos;
		mymemcpy(_this->buf + bufpos, _buf, firstPart);
		mymemcpy(_this->buf, _buf + firstPart, _bufLen - firstPart);
	}
	_this->pos += _bufLen;
	return start;
}

int cb_read(struct CircleBuffer *_this, TPos _startPos, cchar *_buf, int _bufSize)
{
	TPos currpos = _this->pos;
	if (_startPos >= 0 && _startPos >= currpos - _this->bufSize && _startPos <= currpos)
	{
		if (_startPos == currpos)
		{
			return 0;
		}
		else
		{
			int startPos = _startPos % _this->bufSize;
			TPos _endPos = _startPos + _bufSize;
			if (_startPos + _bufSize > currpos)
			{
				_bufSize = (int)(currpos - _startPos);
			}
			if (startPos + _bufSize <= _this->bufSize)
			{
				mymemcpy(_buf, _this->buf + startPos, _bufSize);
			}
			else
			{
				int firstPart = _this->bufSize - startPos;
				mymemcpy(_buf, _this->buf + startPos, firstPart);
				mymemcpy(_buf + firstPart, _this->buf, _bufSize - firstPart);
			}
			return _bufSize;
		}
	}
	return -1;
}

inline TPos cb_pos(struct CircleBuffer *_this)
{
	return _this->pos;
}
inline TPos cb_size(struct CircleBuffer *_this)
{
	return _this->bufSize;
}

struct BufferDataWriter *bdw_init(struct BufferDataWriter *_this, struct BufferSource *_bufferSource, int _channel, int _bits, int _bufferSize, int _overlap, bool _fillFloatBuffer)
{
	int frameSize;
	_this->writeType = WriteTypeNone;
	_this->bufferSource = _bufferSource;
	_this->currBuffer = NULL;
	_this->firstBuffer = true;

	_this->currBufferFilled = 0;
	_this->floatOverlap = _this->overlap = _overlap;
	_this->floatStepSize = _bufferSize - _this->floatOverlap;
	_this->floatBufferSize = _bufferSize;

	frameSize = ((_bits + 7) / 8) * _channel;
	_this->byteOverlap = _this->floatOverlap * frameSize;
	_this->byteStepSize = _this->floatStepSize * frameSize;
	_this->bufferSize = _this->byteOverlap + _this->byteStepSize;
	_this->buffer = (cchar *)mymalloc(sizeof(cchar) * _this->bufferSize);
	_this->converter = getConverter(_channel, _bits, true, false);
	return _this;
}

void bdw_finalize(struct BufferDataWriter *_this)
{
	if (_this->buffer != NULL)
	{
		myfree(_this->buffer);
	}

	myfree(_this->converter);
}

int bufferDebugIdx = 0;
int totalWrittenBytes = 0;
int bdw_write(struct BufferDataWriter *_this, cchar *_data, int _dataLen)
{
	int dataWrote = 0;
	bool getNewBuffer = false;
	cchar *bufferData;
	int bufferDataLen;

	if (_this->writeType == WriteTypeNone)
	{
		_this->writeType = WriteTypeShort;
	}
	if (_this->writeType != WriteTypeShort)
	{
		throwError("only one type data can write");
	}

	totalWrittenBytes += _dataLen;

	do
	{
		if (_this->currBuffer == NULL)
		{
			_this->currBuffer = _this->bufferSource->getBuffer(_this->bufferSource);
			getNewBuffer = true;
			bd_reset(_this->currBuffer);
			if(!_this->firstBuffer)
			{
				mymemmove(_this->buffer, _this->buffer + _this->byteStepSize, _this->byteOverlap);
				_this->currBufferFilled = _this->byteOverlap; 
			}
		}

		if ((_dataLen - dataWrote) >= (_this->bufferSize - _this->currBufferFilled))
		{

			mymemcpy(_this->buffer + _this->currBufferFilled, _data + dataWrote, _this->bufferSize - _this->currBufferFilled);
			dataWrote += (_this->bufferSize - _this->currBufferFilled);

			bufferData = bd_getData(_this->currBuffer); 
			bufferDataLen = bd_getMaxBufferSize(_this->currBuffer);
			assert(bufferDataLen == _this->bufferSize);
			mymemcpy(bufferData, _this->buffer, bufferDataLen);
			bd_setFilledSize(_this->currBuffer, bufferDataLen);

			_this->bufferSource->freeBuffer(_this->bufferSource, _this->currBuffer);

			_this->currBuffer = NULL;
			_this->currBufferFilled = 0;
		}
		else
		{
			mymemcpy(_this->buffer + _this->currBufferFilled, _data + dataWrote, _dataLen - dataWrote);
			_this->currBufferFilled += (_dataLen - dataWrote);
			dataWrote = _dataLen;
		}

		if (getNewBuffer)
		{
			_this->firstBuffer = false;
		}
	}while (dataWrote < _dataLen);

	return dataWrote;
}

void bdw_flush(struct BufferDataWriter *_this)
{
	cchar *bufferData;
	int bufferDataLen;
	if (_this->currBuffer != NULL && _this->writeType == WriteTypeShort)
	{
		bufferData = bd_getData(_this->currBuffer); 
		bufferDataLen = bd_getMaxBufferSize(_this->currBuffer);
		assert(bufferDataLen == _this->bufferSize);
		mymemset(bufferData, 0, bufferDataLen);
		mymemcpy(bufferData, _this->buffer, _this->currBufferFilled);

		_this->bufferSource->freeBuffer(_this->bufferSource, _this->currBuffer);
		_this->currBuffer = NULL;
		_this->currBufferFilled = 0;
	}
}

inline void vevent_reset(struct VoiceEvent *_this, struct BufferData *_data)
{
	_this->data = _data;
}

#ifdef CONTROL_RECOGNIZER_INIT
bool vp_no_isNoiseLevelInited(struct VoiceProcessor *_this, int _freqIdx, float *_noiseLevel)
{

	assert(0);
	return false;
}

void vp_no_reInitNoiseLevel(struct VoiceProcessor *_this, int _freqIdx, float _noiseLevel)
{

	assert(0);
}
#endif

