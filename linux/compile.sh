gcc -c -g sub_reader.c sub_reader.h config.h -I$HOME/ffmpeg_build/include;
gcc -c -g av_decoder.c av_decoder.h -I$HOME/ffmpeg_build/include -I/usr/local/include -L/usr/local/lib -L$HOME/ffmpeg_build/lib -lavformat -lavfilter -lavcodec -lavutil -lswresample -lswscale -lpthread -lm -lz -ldl;
gcc -c -g test.c -I$HOME/ffmpeg_build/include;
gcc -o test av_decoder.o sub_reader.o test.o -I$HOME/ffmpeg_build/include -I/usr/local/include -L$HOME/ffmpeg_build/lib -L/usr/local/lib  -lavformat -lbz2 -lavfilter -lpostproc -lass -lswresample -lavcodec -ltheoraenc -ltheoradec -lfdk-aac -lmp3lame -lopus -lvorbisfile -lvorbisenc -lvorbis -lvpx -lx264 -lavutil -lva -lva-drm -lva-x11 -lvdpau -lX11 -lswresample -lswscale -lpthread -lfreetype -lm -lz -ldl; 
