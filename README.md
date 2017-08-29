# High speed synchronization tool
Subtitle files are based on presentation timings of video and are synchronized with audio. If a video is edited to make it short, remove commercials et cetra then subtitle file of original video may not work with it. This tools solves the problem and using original video, original subtitle file and edited video/audio snippet it creates a subtitle file for edited one. It uses audiofingerprinting to complete this task.

## Audiofingerprinting
Audio fingerprinting is a technique in digital signal processing that encodes audio data to some binary fingeprints. These fingerprints can then be used further for duplicate detection, watermarking an audio snippet, and in our case using to synchronize subtitles and edit subtitle files. This tool accepts two videos,original and edited and original subtitle file to create an edited subtitle file.

## Get started
Currently tools are built for linux only. To use tools clone github repository.
```
$ git clone https://github.com/Amey-Jain/audiofingerprinting.git
```
Install ffmpeg from source using or you can use system ready binaries by editing linux/compile.sh file
```
$ linux/install_ffmpeg.sh
```
Install fftw3. Ubuntu users can enter command
```
$ sudo apt-get install libfftw3
```
Finally compile tools using
```
$ linux/compile.sh
```
Note: This project uses shell files to compile and manage dependencies. Anything customized to be done can be handled in those two files.

## Dependencies
* [ffmpeg](https://www.ffmpeg.org/)
* [fftw3](http://www.fftw.org/)

## Resources
* Google research paper on audio fingerprinting using wavelets: https://research.google.com/pubs/archive/32685.pdf
* Duplicate songs detector: https://github.com/AddictedCS/soundfingerprinting

