#include <stdio.h>
#ifdef WIN32
#include <Windows.h>
#include <process.h>
#else
#include<pthread.h>
#include <unistd.h>
#define scanf_s scanf 
#endif
#include "voiceRecog.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define true 1
#define false 0
void *recognizer=NULL;

const char *recorderRecogErrorMsg(int _recogStatus)
{
	char *r = (char *)"unknow error";
	switch(_recogStatus)
	{
	case VR_ECCError:
		r = (char *)"ecc error";
		break;
	case VR_NotEnoughSignal:
		r = (char *)"not enough signal";
		break;
	case VR_NotHeaderOrTail:
		r = (char *)"signal no header or tail";
		break;
	case VR_RecogCountZero:
		r = (char *)"trial has expires, please try again";
		break;
	}
	return r;
}

//识别开始回调函数
void recognizerStart(void *_listener, float _soundTime)
{
	printf("------------------recognize start\n");
}

//识别结束回调函数
void recognizerEnd(void *_listener, float _soundTime, int _recogStatus, char *_data, int _dataLen)
{
	struct SSIDWiFiInfo wifi;
	struct WiFiInfo macWifi;
	int i;
	char s[100];
	if (_recogStatus == VR_SUCCESS)
	{		
		enum InfoType infoType = vr_decodeInfoType(_data, _dataLen);
		if(infoType == IT_SSID_WIFI)
		{
			vr_decodeSSIDWiFi(_recogStatus, _data, _dataLen, &wifi);
			printf("ssid:%s, pwd:%s\n", wifi.ssid, wifi.pwd);
		}
		else if(infoType == IT_STRING)
		{
			vr_decodeString(_recogStatus, _data, _dataLen, s, sizeof(s));
			printf("string:%s\n", s);

		}
		else if(infoType == IT_WIFI)
		{
			vr_decodeWiFi(_recogStatus, _data, _dataLen, &macWifi);
			printf("mac wifi:");
			for (i = 0; i < macWifi.macLen; i ++)
			{
				printf("0x%.2x ", macWifi.mac[i] & 0xff);
			}
			printf(", %s\n", macWifi.pwd);
		}
		else
		{
			printf("------------------recognized data:%s\n", _data);
		}

	}
	else
	{
		printf("------------------recognize invalid data, errorCode:%d, error:%s\n", _recogStatus, recorderRecogErrorMsg(_recogStatus));
		printf("string:%s\n", _data);
	}
}

void *runRecorderVoiceRecognize( void * _recognizer) 
{
	while (1) {
		vr_runRecognizer(_recognizer);
		sleep(1);
	}
	return 0;
}

//录音机回调函数
int recorderShortWrite(void *_writer, const void *_data, unsigned long _sampleCout)
{
	char *data = (char *)_data;
	void *recognizer = _writer;
	return vr_writeData(recognizer, data, ((int)_sampleCout)*2);
}

int freqs[]= {15000,15200,15400,15600,15800,16000,16200,16400,16600,16800,17000,17200,17400,17600,17800,18000,18200,18400,18600};;

#include <stdlib.h>
int test();
void startRecog()
{
	int i, ccc;
	int baseFreq = 4000;
	for(i = 0; i < sizeof(freqs)/sizeof(int); i ++)
	{
		freqs[i] = baseFreq + i * 150;
	}
#if 0
	//创建识别器，并设置监听器
	recognizer = vr_createVoiceRecognizer2(MemoryUsePriority, 16000);
	vr_setRecognizeFreqs(recognizer, freqs, sizeof(freqs)/sizeof(int));
	//设置识别监听器
	vr_setRecognizerListener(recognizer,NULL,recognizerStart,recognizerEnd);
#endif
	//开始录，并把录音数据通过vr_writeData写到识别器，参考recorderShortWrite函数
	
	printf("_beginthread recog thread\n");
	//开始识别

	pthread_t ntid_record;
	pthread_create(&ntid_record, NULL, test, NULL);
	sleep(2);

#ifdef WIN32
	_beginthread(runRecorderVoiceRecognize, 0, recognizer);
#else
	pthread_t ntid;
	pthread_create(&ntid, NULL, runRecorderVoiceRecognize, recognizer);
#endif
	printf("recognizer start\n");
	printf("press any key to stop\n");
	//scanf_s("%c", &ccc);
	printf("enter voice stop\n");
#if 0	
	//可以在这里停止录音，停止往识别器输送pcm数据
	//通知识别器停止，并等待识别器真正退出
	vr_stopRecognize(recognizer);
	do 
	{		
		printf("recognizer is quiting\n");
#ifdef WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	} while (!vr_isRecognizerStopped(recognizer));

	//销毁识别器
	vr_destroyVoiceRecognizer(recognizer);
#endif
}

int main(int argc, char *argv[])
{
	//startRecog();
	while (1) {
		test();
		sleep(10);
		printf("idle...\n");
	};
	return 0;
}

struct WavData
{
	short channels, bits_per_sample;    // 16
	unsigned int sample_rate, avg_bytes_sec; // 32
	char *data;
	unsigned int size;
};

void recorderRecognizerStart(void *_listener, float _soundTime)
{
	printf("----recognize start\n");
}

void recorderRecognizerEnd(void *_listener, float _soundTime, int _recogStatus, char *_data, int _dataLen)
{
	int i;
	char s[100];
	if (_recogStatus == VR_SUCCESS)
	{
		enum InfoType infoType = vr_decodeInfoType(_data, _dataLen);
		if(infoType == IT_STRING)
		{
			vr_decodeString(_recogStatus, _data, _dataLen, s, sizeof(s));
			printf("string:%s\n", s);
		}			
		else if(infoType == IT_SSID_WIFI)
		{
			struct SSIDWiFiInfo wifi;
			vr_decodeSSIDWiFi(_recogStatus, _data, _dataLen, &wifi);
			printf(">>>>>>>>>>>>ssid:%s, pwd:%s<<<<<<<<<<\n", wifi.ssid, wifi.pwd);
		}
		else
		{
			printf("----recognized data:%s\n", _data);
		}
	}
	else
	{
		printf("----recognize invalid data, errorCode:%d, error:%s\n", _recogStatus, recorderRecogErrorMsg(_recogStatus));
	}
}

void createVoiceRecog(int argc, char* argv[])
{
	int sampleRate = 16000;
	int baseFreq = 4000;
	int i;
	for(i = 0; i < sizeof(freqs)/sizeof(int); i ++)
	{
		freqs[i] = baseFreq + i * 150;
	}

	recognizer = vr_createVoiceRecognizer2(CPUUsePriority, sampleRate);
	vr_setRecognizeFreqs(recognizer, freqs, sizeof(freqs)/sizeof(int));
	vr_setRecognizerListener(recognizer, NULL, recorderRecognizerStart, recorderRecognizerEnd);
}

#define min(X, Y)	((X) > (Y) ? (Y) : (X))

int test()
{
	int fd;
	printf(">>>>>>>>>>>>>>%s %d<<<<<<<<<<<<<<\n", __func__, __LINE__);
	struct WavData wavData;
	int pos = 0, bufPos = 0;
	char *buf = NULL;
	int bufSize = 0, canCpy = 0;
	char *wavDataPtr = NULL;
	int playCount = 0;
	int size;
	pos = 0;

	createVoiceRecog(0, NULL);	
	printf("voice recognizer ready\r\n");	

	fd = open("./1.pcm", O_RDONLY);
	if (fd < 0) {
		printf("[%s %d] open error, fd: %d\n", __func__, __LINE__, fd);
		return 0;
	}

	wavData.data = malloc(720000);
	memset(wavData.data, 0, 720000);
	wavData.channels = 1;
	wavData.bits_per_sample = 16000;
	wavData.avg_bytes_sec = 32;
	wavData.size = read(fd, wavData.data, 720000);
	if (wavData.data > 0)
	{
		int bytesPerSample = ((wavData.bits_per_sample+7)/8);
		wavDataPtr = wavData.data;
		while (true)
		{
			if (buf == NULL)
			{
				vr_nextWriteBuff(recognizer, &buf, &bufSize);
				bufSize = bufPos = 0;
			}
			if (buf != NULL)
			{
				if (pos < wavData.size)
				{
					canCpy = min(wavData.size - pos, bufSize);
					memcpy(buf, wavData.data + pos, canCpy);
					pos += canCpy;
					bufPos += canCpy;
					if (bufPos >= bufSize)
					{
						buf = NULL;
						vr_nextWriteBuff(recognizer, &buf, &bufSize);
					}
				}
				else
				{
					//break;
					if(playCount ++ < 1000)pos = 0;
					else
						break;
					printf("play %d times\n", playCount);
				}
			}

			vr_runOnceRecognizer(recognizer);
		}
		free(wavDataPtr);
		return true;
	}

	vr_destroyVoiceRecognizer(recognizer);
	recognizer = NULL;
	free(wavData.data);
	close(fd);
	return 0;
}
