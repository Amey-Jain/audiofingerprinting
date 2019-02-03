# Amey Jain - Google summer of code 2017
# High speed synchronization tools
Subtitle files are based on presentation timings of video and are synchronized with audio. If a video is edited to make it short, remove commercials et cetra then subtitle file of original video may not work with it. This tools solves the problem and using original video, original subtitle file and edited video/audio snippet it creates a subtitle file for edited one. It uses audiofingerprinting to complete this task.

# Tools proposed and developed
There were two proposed tools
Tool A: Creates an edited subtitle file with the help of original video, edited video and original subtitle file
This tool uses audio fingerprinting to perform its work. Fingerprints for both videos are created first and then are matched against each other to create edited subtitle file. 
Tool B: Creates an edited subtitle file with the help of edited audio/video in absence of original video
This tool doesn't have audio for original video. Timing and duration of audio packets is the base on which edited subtitle file is created. Audio packets have a duration and presentation time stamp on the basis of which we edit original subtitle file.

Developed:
Developed is a single binary named *tulyakalan* (hindi for 'synchronization'). It can select between tools A and B using flags.

## Audiofingerprinting
Audio fingerprinting is a technique in digital signal processing that encodes audio data to some binary fingeprints. These fingerprints can then be used further for duplicate detection, watermarking an audio snippet, and in our case using to synchronize subtitles and edit subtitle files. This tool accepts two videos,original and edited and original subtitle file to create an edited subtitle file. You can get more details [here](https://github.com/Amey-Jain/audiofingerprinting#details).

## Usage
Command line interface is easy to use for tool A.
```
./tulyakalan -a /path/to/subtitle_file /path/to/original_video /path/to/edited_file
```
Note: Edited file can be either audio or video.

Command line usage for tool B.
```
./tulyakalan -b /path/to/original_subtitle_file /path/to/edited/audio
```

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
* (Tool A) Matching module: Match fingerprints against each other and compare- DONE
* (Tool A) Subtitle editor: Using result set of matched fingerprints edit original subtitle file
* (Tool B) Under development

### TODO
* Make a .org file for each todo and treat it as a sprint
 - [ ] Documenting source code and theory (archives/documentation-feb-2019.org)
## Scope
Apart from synchronizing subtitle files this project can be used in lot of other places. It can be used for duplicate song detection and water marking purposes also. It can also be upgraded to provide services like acoustid and musicbrainz in future.

## Dependencies
* [ffmpeg](https://www.ffmpeg.org/)
* [fftw3](http://www.fftw.org/)

## Resources
* [Original proposal](https://summerofcode.withgoogle.com/serve/5209594140819456/)
* [Google research paper on audio fingerprinting using wavelets](https://research.google.com/pubs/archive/32685.pdf)
* [Duplicate songs detector](https://github.com/AddictedCS/soundfingerprinting)
* [Google summer of code project link](https://summerofcode.withgoogle.com/projects/#5903046541312000)

## Details
### Tool A
For audiofingerprinting a feature needs to be selected from audio. Audio can have various features like pitch, frequency, tone etc. Each feature has its own charachteristics. Also to mention features selected are closer to perceptional features of audio(how humans perceive sound). Other are mathematical features.
#### Pre processing
Audio is present in songs/videos is in different container formats, with multiple channel and in compressed form. Pre processing takes audio file and performs following steps:
1. Decode it to PCM float samples
2. Mono sample it (convert into 1 channel)
3. Downsample it to frequency of 5512 Hz
Downsampling to 5512 Hz is done as it is general range of voice frequency in daily life. 
#### Overlapping and windowing
Taking samples from pre processing, these samples are first made to a frame of 2000 milli seconds length of sound. On this frame of audio, 371 ms long samples are taken every 11.6 seconds, which completes overlapping process. These frames of 371 ms length are then passed to filter of hanning window to cut out excessive and irrelevent information. Final product in this stage is frames of 371 ms with hann window applied. This 371 time frame is input for a sub-fingerprint, which together will create one fingerprint block.
#### FFT
This is most expensive step of all. In this step on each sub-fingerprint block, FFT is applied. The output being mirror image is cut to half
#### Spectogram creation and wavelets
All sub-fingerprint blocks are joined together to form on single large block of spectrum for a fingerprint block. This block is then treated as an image and wavelets are extracted from it. Out of these top 100 wavelets are kept and rest are discarded.
#### Minhashing and locality sensitive hashing
The top 100 wavelts are then encoded as binary signatures which are converted to signature of length 100 bytes. This 100 byte signature is divided into 25 hash tables with index of subtitles.  
#### Retrivel
With optimised settings of system we simply search audio snippets of edited file through out the data base of original file fingerprints. Whichever best match is found, it is reflected back in edited subtitle file.

### Tool B
Tool B simply uses timing and duration of audio packets and subtitles to create an edited subtitle file. Basic principle is to match duration of subtitles and audio packets.