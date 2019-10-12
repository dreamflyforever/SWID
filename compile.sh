find -name "*.d" | xargs rm  
find -name "*.o" | xargs rm
rm "*.a"

#gcc -O2 -g -c common.c rs_emon.c buffer.c linux.c signalAnalyserTimeMatch2.c tinycthread.c util.c voiceRecognizer.c voiceRecog.c kiss_fft.c kiss_fftr.c circleQueue.c my_poolAlloc.c my_vector.c

#ar rc -g libvoiceRecog_mips520.a common.o rs_emon.o buffer.o kiss_fft.o kiss_fftr.o linux.o signalAnalyserTimeMatch2.o tinycthread.o util.o voiceRecognizer.o voiceRecog.o circleQueue.o my_poolAlloc.o my_vector.o
#gcc -g voiceRecogDemo.c libvoiceRecog_mips520.a -L. -lm -lpthread
gcc -g voiceRecogDemo.c common.c rs_emon.c buffer.c linux.c signalAnalyserTimeMatch2.c tinycthread.c util.c voiceRecognizer.c voiceRecog.c kiss_fft.c kiss_fftr.c circleQueue.c my_poolAlloc.c my_vector.c -lm -lpthread -DLINUX
./a.out
