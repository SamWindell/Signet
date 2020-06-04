# Signet
### Command line program for manipulating audio samples - still a work-in-progess

Signet is a command line program designed to make sample library development easier. Our needs in this domain involve these areas:
- Bulk processing large amounts of samples, almost exclusively lossless audio formats (WAV, FLAC and AIFF). This processing is not 'creative', but instead it is preparation for professional playback such as fading, adding offsets, trimming, noise removal, applying gain, etc.
- Organising many audio files - whether that be renaming them, or sorting into subfolders. This is often needed so that samples can be easily understood by a sampler.

### Limitations
- Currently only supports reading and writing WAV and FLAC files.
- Any metadata in the file is discarded - such as loop markers in a WAV file.

### Building
To get Signet, you currently have to build it from source. However, this process is designed to be simple for those familiar with building C++ programs. There are no library dependencies external to this repo. Run CMake to generate a configuration for your preferred build tool (Visual Studio Solution, makefile, etc.), and then build using that.

### Current subcommands
Each subcommand has it's own set of options. To show these add `--help` after the subcommand. For example: `signet norm --help`. You can also run `--help-all` on signet itself to see more info: `signet --help-all`.

- `zcross-offset`. Zero crossing offsetter. Offsets the start of an audio file to the nearest zero-crossing. Optionally appends the samples that it skipped to the end of the file; this is useful when used with samples that are seamless loops.
- `norm`. Normalise a sample to a particular decibel level. Can be used on a directory, optionally recursively. With a directory you can normalise the samples to a common gain.
- `fade`. Fade the start and/or end of the audio. The fade length must be specified. There are options for the shape of the fade curve.
- `convert`. Converts a file's sample rate and bit depth. Uses the high quality resampler r8brain.
- `silence-remove`. Removes silence from the end, start or both start and end of the sample.
- `trim`. Removes a chunk of specified length from the end, start or both start and end of the sample.
- `pitch-detect`. Prints the detected pitch of the sample.
- `tune`. Change the pitch of the sample by slowing it down or speeding it up.

### Usage
Signet is run from the command line. You first specify the input filename. This can be either a single filename or a filename pattern (glob). You can also add multiple inputs by comma separating them. You then must specify one or more subcommands. These are the utilites that process the files. They will process the file(s) in the order that they are specified in the command.

By default, signet overwrites the files that it processes. If you are just processing a single file, you can specify another file-name after the input file to be the output file; this cannot work if your input is a folder or a pattern.

If you make a mistake and want to undo any files that you have just overwritten, you can use the `--load-backup` command. This system is quite primitive - it simply stores a copy of each overwritten file. This backup is reset if you run another command. There is no backup history, it just works as a single undo. The backup system works if you have just written multiple files, in which case they are all reinstated.

For example if you did not want to processes this file:
`signet filename.wav norm -3`

You can run this command to reinstate the file prior to normalising it to 3db:
`signet --load-backup`

### Examples

`signet filename.wav fade in 1s`

`signet *.wav norm -3`

`signet filename1.wav,filename2.flac norm -3`

`signet "sampler/session 1/*.wav,sampler/session 2/*-unprocessed.wav" zcross-offset 100ms`

`signet input-filename.flac output-filename.flac fade out 10s`
