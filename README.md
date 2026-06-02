# Waveplay
Command line audio player specifically for uncompressed PCM wave files.

- Waveplay can play mono, stereo, 8 bit and 16 bit formats. 24 bit is played as 16 bit, and 32 bit is not supported.
- Playback is at a 44100Hz samplerate, but Waveplay can handle different samplerates of your wave files.
- While Waveplay is playing a track, it will immediately load the next track to provide gapless playback.
- Waveplay can also mix your playlist into a random order.

# Inspiration
Compressed formats require special libraries to decompress them leading to dependancies, and some formats are encoded leading to further dependancies. Hard drive capacity has increased to the point that storing audio files in an uncompressed PCM format can seem reasonable, and can make it very easy to read and play them.

#Usage

`waveplay FILES`

Simple, isn't it.
