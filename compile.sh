gcc -c -g sub_reader.c sub_reader.h config.h;
gcc -c -g av_decoder.c av_decoder.h `pkg-config --cflags --libs libavformat libavfilter libavcodec libswresample` -ldl;
gcc -c -g test.c;
gcc -o test av_decoder.o sub_reader.o test.o `pkg-config --cflags --libs libavformat libavfilter libavcodec libswresample` -ldl;
