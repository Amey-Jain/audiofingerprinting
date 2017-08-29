# High speed synchronization tool
Subtitle files are based on presentation timings of video and are synchronized with audio. If a video is edited to make it short, remove commercials et cetra then subtitle file of original video may not work with it. This tools solves the problem and using original video, original subtitle file and edited video/audio snippet it creates a subtitle file for edited one. It uses audiofingerprinting to complete this task.

## Audiofingerprinting
Audio fingerprinting is a technique in digital signal processing that encodes audio data to some binary fingeprints. These fingerprints can then be used further for duplicate detection, watermarking an audio snippet, and in our case using to synchronize subtitles and edit subtitle files. This tool accepts two videos,original and edited and original subtitle file to create an edited subtitle file. You can get more details [here](https://github.com/Amey-Jain/audiofingerprinting#details).

## Usage
Command line interface is easy to use.
```
./audiof /path/to/subtitle_file /path/to/original_video /path/to/edited_file
```
Note: Edited file can be either audio or video.

## Getting started
Currently tools are built for linux only. To use tools clone github repository.
```
$ git clone https://github.com/Amey-Jain/audiofingerprinting.git
```
Although ffmpeg binaries are available we recommend you to build it from source,due to fact that they are changing always. You can install ffmpeg from source using
```
$ linux/install_ffmpeg.sh
```
We are using FFTW fft library due to its higher speed. You can get system binaries from organisation page(link given in dependencies) to install fftw3. Ubuntu users can enter command
```
$ sudo apt-get install libfftw3
```
Finally compile tools using
```
$ linux/compile.sh
```
Note: This project currently uses shell files to compile and manage dependencies. Anything customized to be done can be handled in those two files.

### Current status
Project is not completed yet. Currently it has stable database system ready for its use. Following functionalities are yet to be added:
* Matching module: Match fingerprints against each other and compare
* Subtitle editor: Using result set of matched fingerprints edit original subtitle file

## Scope
Apart from synchronizing subtitle files this project can be used in lot of other places. It can be used for duplicate song detection and water marking purposes also. It can also be upgraded to provide services like acoustid and musicbrainz in future.

## Dependencies
* [ffmpeg](https://www.ffmpeg.org/)
* [fftw3](http://www.fftw.org/)

## Resources
* Google research paper on audio fingerprinting using wavelets: https://research.google.com/pubs/archive/32685.pdf
* Duplicate songs detector: https://github.com/AddictedCS/soundfingerprinting

## Details 

