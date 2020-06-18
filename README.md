# Signet
## Command-line program for editing audio files, and assisting sample library development (still a work-in-progress)

Signet makes sample library development (multi-sampling) easier and more effective by offering a suite of tools covering these areas:
- Bulk editing audio files
- Bulk renaming audio files
- Bulk organising files (moving them into folders)
- Generating files based off of existing files

Signet is not exclusively useful for sample library development though. The editing features in particular could be useful to anyone working with large sets of audio files.

## Limitations
- Currently only supports reading and writing WAV and FLAC files.
- Any metadata in the file is discarded - such as loop markers in a WAV file.

## Building
To get Signet, you currently have to build it from the source code. However, this process is designed to be simple for those familiar with building C++ programs. There are no library dependencies external to this repo. Just run CMake to generate a configuration for your preferred build tool (Visual Studio Solution, makefile, etc.), and then build using that.

A C++17 compiler is required. Tested with MSVC 16.5.1 and Apple Clang 11.0.0.

## Usage Overview
### Edit or generate
Signet has 2 modes:
- Edit: Editing already existing audio files.
- Generate: Generating audio files based off of others.

To use the commands for editing audio files, you must specify `edit` as the first argument. And to the commands for generating you specify `gen` instead.

Synopsis:
```
signet edit in-files subcommand subcommand-options
or
signet gen subcommand subcommand-options
```

### Display help text
Care has been taken to ensure the help text is comprehensive and understandable. Run signet with the argument `--help` to see information about the available options. Run with `--help-all` to see all the available subcommands. You can also add `--help` after a subcommand to see the options of that subcommand specifically. For example:

`signet --help`
`signet --help-all`
`signet edit --help`
`signet gen --help`
`signet edit file.wav fade --help`
`signet edit file.wav fade in --help`

## Usage: Edit
This sections covers how to use Signet for editing audio files. To do this you must first specify edit as the first argument. `signet edit`.

### Input files
You must first specify the input file(s). This is a single argument, but you can pass in multiple inputs by comma-separating them. Each comma separated section can be one of 3 types:

- A single file such as `file.wav`.
- A directory such as `sounds/unprocessed`. In this case Signet will search for all audio files in that directory and process them all. You can specify the option `--recursive` to make this also search subfolders.
- A glob-style filename pattern. You can use `*` to match any non-slash character and use `**` to match any character. So essentially use `**` to signify recursively searching folders. For example `*.wav` will match any file that has a `.wav` extension in the current folder. `unprocessed/\*\*/\*.wav` will match any file with the `.wav` extension in the `unprocessed` folder and any subfolder of it.

Input files are processed and then saved back to file (overwritten). Signet features a simple undo option that will restore any files that you overwrote in the last call.

### Exclude files
You can exclude certain files from being processed by prefixing them with a dash. For example `file.\*,-\*.wav` will match all files in the current directory that start with `file`, except those with the `.wav` extension.

### Undo
Signet overwrites the files that it processes. It is therefore advisable to make a copy your audio files before processing them with Signet.

However, Signet can help with safety too. It features a simple undo system. You can undo any changes made in the previous run of Signet by running it again with the `--undo` option. For example `signet --undo`.

Files that were overwritten are restored, new files that were created are destroyed, and files that were renamed are un-renamed. You can only undo once - you cannot keep going back in history.

## Editing Subcommands (effects)
Next, you must specify what subcommand to run. A subcommand is the effect that should be applied to the file(s).

Each subcommand has its own set of arguments; these are shown by adding `--help` after the subcommand.

You can use multiple subcommands in the same call by simply specifying them one after the other. The effects of each subcommand will be applied to the file(s) in the order that they appear.

### Fader
`fade`

Fader: adds a fade-in to the start and/or a fade-out to the end of the file(s). This subcommand has itself 2 subcommands, 'in' and 'out'; one of which must be specified. For each, you must specify first the fade length. You can then optionally specify the shape of the fade curve.

### Normaliser
`norm`

Normaliser: sets the peak amplitude to a certain level. When this is used on multiple files, each file is attenuated by the same amount. You can disable this by specifying the flag --independently.

### Zero-crossing offsetter
`zcross-offset`

Zero-crossing Offsetter: offsets the start of an audio file to the nearest zero-crossing (or the closest thing to a zero crossing). You can use the option --append to cause the samples that were offsetted to be appended to the end of the file. This is useful for when the file is a seamless loop.

### Convert sample rate, bit depth and file format
`convert`

Converter: converts the file format, bit-depth or sample rate. Features a high quality resampling algorithm. This subcommand has subcommands, it requires at least one of sample-rate, bit-depth or file-format to be specified.

### Silence remover
`silence-remove`

Silence-remover: removes silence from the start or end of the file(s). Silence is considered anything under -90dB, however this threshold can be changed with the --threshold option.

### Start/end Trimmer
`trim`

Trimmer: removes the start or end of the file(s). This subcommand has itself 2 subcommands, 'start' and 'end'; one of which must be specified. For each, the amount to remove must be specified.

### Pitch detector
`detect-pitch`

Pitch-detector: prints out the detected pitch of the file(s).

### Tuner
`tune`

Tuner: changes the tune the file(s) by stretching or shrinking them. Uses a high quality resampling algorithm.

### Auto-tuner
`auto-tune`

Auto-tune: tunes the file(s) to their nearest detected musical pitch. For example, a file with a detected pitch of 450Hz will be tuned to 440Hz (A4).

### File renamer
`rename`

Renamer: Renames the file. All options for this subcommand relate to just the name of the file - not the folder or the file extension. Text added via this command can contain special substitution variables; these will be replaced by values specified in this list:
- `<counter>`: A unique number starting from zero. The ordering of these numbers is not specified.
- `<detected-pitch>`: The detected pitch of audio file in Hz. If no pitch is found this variable will be empty.
- `<detected-midi-note>`: The MIDI note number that is closest to the detected pitch of the audio file. If no pitch is found this variable will be empty.
- `<detected-note>`: The musical note-name that is closest to the detected pitch of the audio file. The note is capitalised, and the octave number is specified. For example 'C3'. If no pitch is found this variable will be empty.
- `<parent-folder>`: The name of the folder that contains the audio file.
- `<parent-folder-snake>`: The snake-case name of the folder that contains the audio file.
- `<parent-folder-camel>`: The camel-case name of the folder that contains the audio file.

### File organiser
`folderise`

Folderiser: moves files into folders based on their names. This is done by specifying a regex pattern to match the name against. The folder in which the matched file should be moved to can be based off of the name. These folders are created if they do not already exist.

## Usage: Generate
This section covers how to use the audio file generating features of Signet. To use these features you specify `gen` as the first argument: `signet gen`.

## Generating Subcommands
### Multi-sample Sample Blender (WIP)
Creates samples in between other samples that are different pitches. It takes 2 samples and generates a set of samples in between them at a given semitone interval. Each generated sample is a different blend of the 2 base samples, tuned to match each other. This tool is useful when you have a multisampled instrument, but it was sampled only at large intervals; such as every octave. This tool will generate samples to create an instrument that sounds like it was sampled with close intervals.

## Examples
Adds a fade-in of 1 second to filename.wav

`signet edit filename.wav fade in 1s`

Normalises (to a common gain) all .wav files in the current directory to -3dB

`signet edit *.wav norm -3`

Normalises (to a common gain) filename1.wav and filename2.flac to -1dB

`signet edit filename1.wav,filename2.flac norm -1`

Offsets the start of each file to the nearest zero-crossing within the first 100 milliseconds. Performs this for all .wav files in any subfolder (recursively) of sampler that starts with "session", excluding files in "session 2" that end with -unprocessed.wav.

`signet edit "sampler/session*/**.wav,-sampler/session 2/*-unprocessed.wav" zcross-offset 100ms`

Rename any file in any of the folders of "one-shots" that match the regex "(.\*)-a". They shall renamed to the whatever group index 1 of the match was, with a -b suffix.

`signet edit one-shots/**/.* rename (.*)-a <1>-b`

Convert all audio files in the folder "my_folder" (not recursively) to a sample rate of 44100Hz and a bit-depth of 24.

`signet edit my_folder convert sample-rate 44100 bit-depth 24`
