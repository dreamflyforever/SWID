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

//ʶ��ʼ�ص�����
void recognizerStart(void *_listener, float _soundTime)
{
	printf("------------------recognize start\n");
}

//ʶ������ص�����
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
	vr_runRecognizer(_recognizer);
	return 0;
}

//¼�����ص�����
int recorderShortWrite(void *_writer, const void *_data, unsigned long _sampleCout)
{
	char *data = (char *)_data;
	void *recognizer = _writer;
	return vr_writeData(recognizer, data, ((int)_sampleCout)*2);
}

int freqs[]= {15000,15200,15400,15600,15800,16000,16200,16400,16600,16800,17000,17200,17400,17600,17800,18000,18200,18400,18600};;

#include <stdlib.h>
void startRecog()
{
	int i, ccc;
	int baseFreq = 16000;
	for(i = 0; i < sizeof(freqs)/sizeof(int); i ++)
	{
		freqs[i] = baseFreq + i * 150;
	}

	//����ʶ�����������ü�����
	recognizer = vr_createVoiceRecognizer2(MemoryUsePriority,44100);
	vr_setRecognizeFreqs(recognizer, freqs, sizeof(freqs)/sizeof(int));
	//����ʶ�������
	vr_setRecognizerListener(recognizer,NULL,recognizerStart,recognizerEnd);

	//��ʼ¼������¼������ͨ��vr_writeDataд��ʶ�������ο�recorderShortWrite����
	
	printf("_beginthread recog thread\n");
	//��ʼʶ��
#ifdef WIN32
	_beginthread(runRecorderVoiceRecognize, 0, recognizer);
#else
	pthread_t ntid;
	pthread_create(&ntid, NULL, runRecorderVoiceRecognize, recognizer);
#endif
	printf("recognizer start\n");
	printf("press any key to stop\n");

	scanf_s("%c", &ccc);
	
	//����������ֹͣ¼����ֹͣ��ʶ��������pcm����

	//֪ͨʶ����ֹͣ�����ȴ�ʶ���������˳�
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

	//����ʶ����
	vr_destroyVoiceRecognizer(recognizer);
}

int main(int argc, char *argv[])
{
	startRecog();
	return 0;
}

