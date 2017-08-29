# Audiofingerprinting
Audio fingerprinting is a technique in digital signal processing that encodes audio data to some binary fingeprints. These fingerprints can then be used further for duplicate detection, watermarking an audio snippet, and in our case using to synchronize subtitles and edit subtitle files. This system accepts two videos,original and edited and original subtitle file to create an edited subtitle file. It then creates fingerprints for both original and edited videos and match them to create an edited subtitle file for edited video.
## Dependencies
* FFMPEG
* FFTW3
## About audio fingerprinting
Following are steps while fingerprinting.
### Preprocessing
Audio signal require some processes before we can start working on them. Inside audio and video containers, audio data is stored in compressed formats. So first audio data is taken out of container formats and uncompressed and encoded into PCM floats which are further mono sampled. All this preprocessing is done using ffmpeg library libavfilter. After pre processing we get snippets of audio data into PCM float format.
### Feature selection, overlapping and windowing
### Spectogram creation
### Min hashing
### Database insertion and matching

