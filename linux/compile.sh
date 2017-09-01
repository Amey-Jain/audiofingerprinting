gcc -c -g sub_reader.c sub_reader.h config.h -I$HOME/ffmpeg_build/include;
gcc -c -g av_decoder.c av_decoder.h config.h -I$HOME/ffmpeg_build/include -I/usr/local/include -I$HOME/ffmpeg_build/include/libavutil -L/usr/local/lib -L$HOME/ffmpeg_build/lib -lavformat -lavfilter -lavcodec -lavutil -lswresample -lswscale -lpthread -lm -lz -ldl;
gcc -c -g spectogram.c spectogram.h config.h  -I$HOME/ffmpeg_build/include -I$HOME/ffmpeg_build/include/libavutil -L$HOME/ffmpeg_build/lib -lavutil -lfftw3 -lm ;
gcc -c -g lsh_db.c lsh_db.h;
#gcc -c -g test.c -I$HOME/ffmpeg_build/include;
gcc -c -g main.c -I$HOME/ffmpeg_build/include;
gcc -o ./tulyakalan av_decoder.o spectogram.o sub_reader.o main.o lsh_db.o -I$HOME/ffmpeg_build/include -I/usr/local/include -L$HOME/ffmpeg_build/lib -L/usr/local/lib  -lavformat -lbz2 -lavfilter -lpostproc -lass -lswresample -lavcodec -ltheoraenc -ltheoradec -lfdk-aac -lmp3lame -lopus -lvorbisfile -lvorbisenc -lvorbis -lvpx -lx264 -lavutil -lva -lva-drm -lva-x11 -lvdpau -lX11 -lswresample -lswscale -lpthread -lfreetype -lfftw3 -lm -lz -ldl; 
echo $?;
