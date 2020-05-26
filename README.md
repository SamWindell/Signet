# Signet
### Command line utilities for manipulating audio samples

These are very simple programs that I have written to edit audio samples for use in a sampler. There are no library dependencies external to this repo. The tools have to be built from source at the moment. Run CMake to generate a configuration for your preferred build tool (Visual Studio Solution, makefile, etc.), and then build using that.

The tools share a common command line interface. They can work on both single audio files and whole directories. All tools can be run with the argument `--help` to see the full set of options.

### Current tools
#### Zero crossing offsetter
Offsets the start of an audio file to the nearest zero-crossing. Optionally appends the samples that it skipped to the end of the file. This is useful when used with samples that are seamless loops.
#### Normaliser 
Normalise a sample to a particular decibel level. Can be used on a directory, optionally recursively. With a directory you can normalise the samples to a common gain.
