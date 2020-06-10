# Signet
### Command-line program for editing audio files (still a work-in-progress)

Signet is a command-line program designed for bulk editing audio files. It features common editing functions such as normalisation and fade-out, but also organisation functions such as renaming files.
This tool was made to make sample library development easier. In this domain, we often have to edit hundreds of separate audio files in preparation of turning them into a playable virtual instrument. As well as this, we often need to rename the files so that a sampler can map them.

Signet tries to make this often repetitive process more automatic.

### Limitations
- Currently only supports reading and writing WAV and FLAC files.
- Any metadata in the file is discarded - such as loop markers in a WAV file.

### Building
To get Signet, you currently have to build it from the source code. However, this process is designed to be simple for those familiar with building C++ programs. There are no library dependencies external to this repo. Just run CMake to generate a configuration for your preferred build tool (Visual Studio Solution, makefile, etc.), and then build using that. 

A C++17 compiler is required. Tested with MSVC 16.5.1 and Apple Clang 11.0.0.

### Current subcommands
Each subcommand has it's own set of options. To show these add `--help` after the subcommand. For example: `signet norm --help`. You can also run `--help-all` on signet itself to see more info: `signet --help-all`.

- `zcross-offset`: Zero crossing offsetter. Offsets the start of an audio file to the nearest zero-crossing. Optionally appends the samples that it skipped to the end of the file; this is useful when used with samples that are seamless loops.
- `norm`: Normalise to a particular decibel level. Can optionally normalise all files to a common gain, and can optionally determine the peak value using RMS.
- `fade`: Fade the start and/or end of the audio. This has subcommands `in` and `out`. Each of those requires the length of the fade, and an optional shape. The default shape is sine.
- `convert`: Converts a file's sample rate and bit depth. Uses the high quality resampler, r8brain.
- `silence-remove`: Removes silence from the end, start or both the start and end of the sample. You can optionally set the dB level to which anything below should be considered silent.
- `trim`. Removes audio from the end, start or both the start and end of the sample.
- `detect-pitch`: Prints the detected pitch of the sample.
- `tune`. Change the pitch of the sample by slowing it down or speeding it up. Specify the pitch change in cents.
- `auto-tune`: Tunes the samples to the nearest musical note. Uses the same tuner as the tune subcommand.
- `rename`: File renamer. Change the name of the file with regex, prefix or suffix. You can also use special substitution variables, for example adding the detected pitch or note.
- `folderise`: Use regex to pattern-match files and move them into folders.

### Usage
Synopsis: `signet in-files subcommand subcommand-options`

#### Display help text
Run signet with the argument `--help` to see information about the available options. Run with `--help-all` to see all the available subcommands. You can also add `--help` after a subcommand to see the options of that subcommand specifically. For example:

`signet --help`
`signet --help-all`
`signet file.wav fade --help`
`signet file.wav fade in --help`

#### Input files
You must first specify the input file(s). This is a single argument, but you can pass in multiple inputs by comma-separating them. The input can be one of 3 types:

- A single file such as `file.wav`. 
- A directory such as `sounds/unprocessed`. In this case Signet will search for all audio files in that directory and process them all. You can specify the option `--recursive-folder-search` to make this also search subfolders.
- A glob-style filename pattern. You can use `*` to match any non-slash character and use `**` to match any character. So essentially use `**` to signify recursively searching folders. For example `*.wav` will match any file that has a `.wav` extension in the current folder. `unprocessed/\*\*/\*.wav` will match any file with the `.wav` extension in the `unprocessed` folder and any subfolder of it.

Input files are processed and then saved back to file (overwritten). Signet features a simple undo option that will restore any files that you overwrote in the last call.

#### Exclude files
You can exclude certain files from being processed by prefixing them with a dash. For example `file.\*,-\*.wav` will match all files in the current directory that start with `file`, except those with the `.wav` extension.

#### Subcommands
Next, you must specify what subcommand to run. A subcommand is the effect that should be applied to the file(s).

Each effect has its own set of arguments; these are described later on.

You can use multiple subcommands in the same call by simply specifying them one after the other. The effects of each subcommand will be applied to the file(s) in the order that they appear.

#### Output file
Signet overwrites the input files so specifying an output file is not normally necessary. There is one exception. If you have just specified a single input file, you can specify a single output file as a second argument. For example:

`signet in-file.wav out-file.wav`

#### Backups (Undo)
Signet has a simple backup system. It stores a copy of each file before it overwrites it. This backup only saves the files from the last call to Signet. In other words, you cannot go back in history other than the change you just made.

To restore all files that you just overwrote, call signet again with the option `--load-backup`. For example `signet --load-backup`.


### Examples

`signet filename.wav fade in 1s`

`signet *.wav norm -3`

`signet filename1.wav,filename2.flac norm -3`

`signet "sampler/session 1/*.wav,sampler/session 2/*-unprocessed.wav" zcross-offset 100ms`

`signet input-filename.flac output-filename.flac fade out 10s`
