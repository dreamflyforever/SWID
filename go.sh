/opt/x1800_sdk/audio_sdk_as182_upgrade/prebuilts/toolchains/mips-gcc520-32bit/bin/mips-linux-uclibc-gnu-gcc -O2 -c common.c rs_emon.c buffer.c linux.c signalAnalyserTimeMatch2.c tinycthread.c util.c voiceRecognizer.c voiceRecog.c kiss_fft.c kiss_fftr.c circleQueue.c my_poolAlloc.c my_vector.c

/opt/x1800_sdk/audio_sdk_as182_upgrade/prebuilts/toolchains/mips-gcc520-32bit/bin/mips-linux-gnu-gcc-ar  rc libvoiceRecog_mips520.a common.o rs_emon.o buffer.o kiss_fft.o kiss_fftr.o linux.o signalAnalyserTimeMatch2.o tinycthread.o util.o voiceRecognizer.o voiceRecog.o circleQueue.o my_poolAlloc.o my_vector.o


gcc -O2 common.c rs_emon.c buffer.c linux.c signalAnalyserTimeMatch2.c tinycthread.c util.c voiceRecognizer.c voiceRecog.c kiss_fft.c kiss_fftr.c circleQueue.c my_poolAlloc.c my_vector.c voiceRecogDemo.c -o main -lm -pthread -DLINUX

ar  rc libvoiceRecog_mips520.a common.o rs_emon.o buffer.o kiss_fft.o kiss_fftr.o linux.o signalAnalyserTimeMatch2.o tinycthread.o util.o voiceRecognizer.o voiceRecog.o circleQueue.o my_poolAlloc.o my_vector.o
