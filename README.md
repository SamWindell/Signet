# Signet
### Command line utilities for manipulating audio samples

Signet is a cross-platform command line tool with a few subcommands that I have written to edit audio samples for use in a sampler. There are no library dependencies external to this repo. Signet has to be built from source at the moment. However, this process is designed to be simple. Run CMake to generate a configuration for your preferred build tool (Visual Studio Solution, makefile, etc.), and then just build using that.

Signet can work on both single audio files and whole directories. Run with the argument `--help` or `--help-all` to see the full set of options.

### Examples
`signet MyAudioFile.wav norm 3`

`signet MyFolder fade in 0.5s out 2s log`

### Current tools
#### Zero crossing offsetter
Offsets the start of an audio file to the nearest zero-crossing. Optionally appends the samples that it skipped to the end of the file. This is useful when used with samples that are seamless loops.
#### Normaliser 
Normalise a sample to a particular decibel level. Can be used on a directory, optionally recursively. With a directory you can normalise the samples to a common gain.
#### Fader
Fade the start and/or end of the audio. The fade length must be specified. There are options for the shape of the fade curve.
