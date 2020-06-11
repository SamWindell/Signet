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

### Usage
Synopsis: `signet in-files subcommand subcommand-options`

#### Display help text
Care has been taken to ensure the help text is comprehensive and understandable. Run signet with the argument `--help` to see information about the available options. Run with `--help-all` to see all the available subcommands. You can also add `--help` after a subcommand to see the options of that subcommand specifically. For example:

`signet --help`
`signet --help-all`
`signet file.wav fade --help`
`signet file.wav fade in --help`

#### Input files
You must first specify the input file(s). This is a single argument, but you can pass in multiple inputs by comma-separating them. Each comma separated section can be one of 3 types:

- A single file such as `file.wav`.
- A directory such as `sounds/unprocessed`. In this case Signet will search for all audio files in that directory and process them all. You can specify the option `--recursive-folder-search` to make this also search subfolders.
- A glob-style filename pattern. You can use `*` to match any non-slash character and use `**` to match any character. So essentially use `**` to signify recursively searching folders. For example `*.wav` will match any file that has a `.wav` extension in the current folder. `unprocessed/\*\*/\*.wav` will match any file with the `.wav` extension in the `unprocessed` folder and any subfolder of it.

Input files are processed and then saved back to file (overwritten). Signet features a simple undo option that will restore any files that you overwrote in the last call.

#### Exclude files
You can exclude certain files from being processed by prefixing them with a dash. For example `file.\*,-\*.wav` will match all files in the current directory that start with `file`, except those with the `.wav` extension.

#### Subcommands (effects)
Next, you must specify what subcommand to run. A subcommand is the effect that should be applied to the file(s).

Each subcommand has its own set of arguments; these are shown by adding `--help` after the subcommand.

You can use multiple subcommands in the same call by simply specifying them one after the other. The effects of each subcommand will be applied to the file(s) in the order that they appear.

#### Fader
`fade`

Fader: adds a fade-in to the start and/or a fade-out to the end of the file(s). This subcommand has itself 2 subcommands, 'in' and 'out'; one of which must be specified. For each, you must specify first the fade length. You can then optionally specify the shape of the fade curve.

#### Normaliser
`norm`

Normaliser: sets the peak amplitude to a certain level. When this is used on multiple files, each file is attenuated by the same amount. You can disable this by specifying the flag --independently.

#### Zero-crossing offsetter
`zcross-offset`

Zero-crossing Offsetter: offsets the start of an audio file to the nearest zero-crossing (or the closest thing to a zero crossing). You can use the option --append to cause the samples that were offsetted to be appended to the end of the file. This is useful for when the file is a seamless loop.

#### Format converter
`convert`

Resampler and bit-depth converter: converts the bit-depth and sample rate using high quality algorithms.

#### Silence remover
`silence-remove`

Silence-remover: removes silence from the start or end of the file(s). Silence is considered anything under -90dB, however this threshold can be changed with the --threshold option.

#### Start/end Trimmer
`trim`

Trimmer: removes the start or end of the file(s). This subcommand has itself 2 subcommands, 'start' and 'end'; one of which must be specified. For each, the amount to remove must be specified.

#### Pitch detector
`detect-pitch`

Pitch-detector: prints out the detected pitch of the file(s).
`tune` Tuner: changes the tune the file(s) by stretching or shrinking them. Uses a high quality resampling algorithm.

#### Auto-tuner
`auto-tune`

Auto-tune: tunes the file(s) to their nearest detected musical pitch. For example, a file with a detected pitch of 450Hz will be tuned to 440Hz (A4).

#### File renamer
`rename`

Renamer: Renames the file. All options for this subcommand relate to just the name of the file - not the folder or the file extension. Text added via this command can contain special substitution variables; these will be replaced by values specified in this list:
`<counter>`: A unique number starting from zero. The ordering of these numbers is not specified.
`<detected-pitch>`: The detected pitch of audio file in Hz. If no pitch is found this variable will be empty.
`<detected-midi-note>`: The MIDI note number that is closest to the detected pitch of the audio file. If no pitch is found this variable will be empty.
`<detected-note>`: The musical note-name that is closest to the detected pitch of the audio file. The note is capitalised, and the octave number is specified. For example 'C3'. If no pitch is found this variable will be empty.
`<parent-folder>`: The name of the folder that contains the audio file.

#### File organiser
`folderise`

Folderiser: moves files into folders based on their names. This is done by specifying a regex pattern to match the name against. The folder in which the matched file should be moved to can be based off of the name. These folders are created if they do not already exist.

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

`signet one-shots/**/.* rename (.*)-a <1>-b`

`signet my_folder convert 44100 24`
