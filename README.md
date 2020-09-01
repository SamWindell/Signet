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
### Display help text
Care has been taken to ensure the help text is comprehensive and understandable. Run signet with the argument `--help` to see information about the available options. Run with `--help-all` to see all the available subcommands. You can also add `--help` after a subcommand to see the options of that subcommand specifically. For example:

`signet --help`
`signet --help-all`
`signet file.wav fade --help`
`signet file.wav fade in --help`

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

## Subcommands (effects)
Next, you must specify what subcommand to run. A subcommand is the effect that should be applied to the file(s).

Each subcommand has its own set of arguments; these are shown by adding `--help` after the subcommand.

You can use multiple subcommands in the same call by simply specifying them one after the other. The effects of each subcommand will be applied to the file(s) in the order that they appear.

```
Signet
==========================================================================

Description:
  Signet is a command-line program designed for bulk editing audio files.
  It has commands for converting, editing, renaming and moving WAV and
  FLAC files. It also features commands that generate audio files. Signet
  was primarily designed for people who make sample libraries, but its
  features can be useful for any type of bulk audio processing.

Usage:
  signet [OPTIONS] input-files SUBCOMMAND

Positionals:
  input-files TEXT REQUIRED
    The audio files to process. This is a file, directory or glob pattern.
    To use multiple, separate each one with a comma. You can exclude a
    pattern by beginning it with a dash. e.g. "-*.wav" would exclude all
    .wav files from the current directory.


Options:
  -h,--help
    Print this help message and exit

  --help-all
    Print help message for all subcommands

  --undo
    Undoes any changes made by the last run of Signet; files that were
    overwritten are restored, new files that were created are destroyed,
    and files that were renamed are un-renamed. You can only undo once -
    you cannot keep going back in history.

  --clear-backup
    Deletes all temporary files created by Signet. These files are needed
    for the undo system and are saved to your OS's temporary folder. These
    files are cleared and new ones created every time you run Signet. This
    option is only really useful if you have just processed lots of files
    and you won't be using Signet for a long time afterwards. You cannot
    use --undo directly after clearing the backup.

  --silent
    Disable all messages

  --recursive
    When the input is a directory, scan for files in it recursively


Subcommands:
  auto-tune
    Auto-tune: tunes the file(s) to their nearest detected musical pitch.
    For example, a file with a detected pitch of 450Hz will be tuned to
    440Hz (A4).

  convert
    Converter: converts the file format, bit-depth or sample rate.
    Features a high quality resampling algorithm. This subcommand has
    subcommands; it requires at least one of sample-rate, bit-depth or
    file-format to be specified.

  fade
    Fader: adds a fade-in to the start and/or a fade-out to the end of the
    file(s). This subcommand has itself 2 subcommands, 'in' and 'out'; one
    of which must be specified. For each, you must specify first the fade
    length. You can then optionally specify the shape of the fade curve.

  folderise
    Folderiser: moves files into folders based on their names. This is
    done by specifying a regex pattern to match the name against. The
    folder in which the matched file should be moved to can be based off
    of the name. These folders are created if they do not already exist.

  gain
    Gainer: changes the volume of the file(s).

  highpass
    Highpass: removes frequencies below the given cutoff.

  lowpass
    Lowpass: removes frequencies above the given cutoff.

  norm
    Normaliser: sets the peak amplitude to a certain level. When this is
    used on multiple files, each file is attenuated by the same amount.
    You can disable this by specifying the flag --independently.

  detect-pitch
    Pitch-detector: prints out the detected pitch of the file(s).

  rename
    File Renamer: various commands for renaming files.

    This command can be used to bulk rename a set of files. It also has
    the ability to insert special variables into the file name, such as
    the detected pitch. As well as this, there is a special auto-mapper
    command that is useful to sample library developers.

    All options for this subcommand relate to just the name of the file -
    not the folder or the file extension.

    Any text added via this command can contain special substitution
    variables; these will be replaced by values specified in this list:

    <counter>
    A unique number starting from zero. The ordering of these numbers is
    not specified.

    <alpha-counter>
    A unique 3 character counter starting from aaa and ending with zzz.
    Beyond zzz, <alpha-counter> will be replaced with a number instead.
    The ordering of these numbers is not specified.

    <detected-pitch>
    The detected pitch of audio file in Hz. If no pitch is found this
    variable will be empty.

    <detected-midi-note>
    The MIDI note number that is closest to the detected pitch of the
    audio file. If no pitch is found this variable will be empty.

    <detected-midi-note-octave-plus-1>
    The MIDI note number (+12 semitones) that is closest to the detected
    pitch of the audio file. If no pitch is found this variable will be
    empty.

    <detected-midi-note-octave-plus-2>
    The MIDI note number (+24 semitones) that is closest to the detected
    pitch of the audio file. If no pitch is found this variable will be
    empty.

    <detected-midi-note-octave-minus-1>
    The MIDI note number (-12 semitones) that is closest to the detected
    pitch of the audio file. If no pitch is found this variable will be
    empty.

    <detected-midi-note-octave-minus-2>
    The MIDI note number (-24 semitones) that is closest to the detected
    pitch of the audio file. If no pitch is found this variable will be
    empty.

    <detected-note>
    The musical note-name that is closest to the detected pitch of the
    audio file. The note is capitalised, and the octave number is
    specified. For example 'C3'. If no pitch is found this variable will
    be empty.

    <parent-folder>
    The name of the folder that contains the audio file.

    <parent-folder-snake>
    The snake-case name of the folder that contains the audio file.

    <parent-folder-camel>
    The camel-case name of the folder that contains the audio file.

  sample-blender
    Multi-sample Sample Blender: creates samples in between other samples
    that are different pitches. It takes 2 samples and generates a set of
    samples in between them at a given semitone interval. Each generated
    sample is a different blend of the 2 base samples, tuned to match each
    other. This tool is useful when you have a multi-sampled instrument
    that was sampled only at large intervals; such as every octave. This
    tool can be used to create an instrument that sounds like it was
    sampled at smaller intervals.

  silence-remove
    Silence-remover: removes silence from the start or end of the file(s).
    Silence is considered anything under -90dB, however this threshold can
    be changed with the --threshold option.

  trim
    Trimmer: removes the start or end of the file(s). This subcommand has
    itself 2 subcommands, 'start' and 'end'; one of which must be
    specified. For each, the amount to remove must be specified.

  tune
    Tuner: changes the tune the file(s) by stretching or shrinking them.
    Uses a high quality resampling algorithm.

  zcross-offset
    Zero-crossing Offsetter: offsets the start of an audio file to the
    nearest zero-crossing (or the closest thing to a zero crossing). You
    can use the option --append to cause the samples that were offsetted
    to be appended to the end of the file. This is useful for when the
    file is a seamless loop.
```

## Examples
Adds a fade-in of 1 second to filename.wav

`signet filename.wav fade in 1s`

Normalises (to a common gain) all .wav files in the current directory to -3dB

`signet *.wav norm -3`

Normalises (to a common gain) filename1.wav and filename2.flac to -1dB

`signet filename1.wav,filename2.flac norm -1`

Offsets the start of each file to the nearest zero-crossing within the first 100 milliseconds. Performs this for all .wav files in any subfolder (recursively) of sampler that starts with "session", excluding files in "session 2" that end with -unprocessed.wav.

`signet "sampler/session*/**.wav,-sampler/session 2/*-unprocessed.wav" zcross-offset 100ms`

Rename any file in any of the folders of "one-shots" that match the regex "(.\*)-a". They shall renamed to the whatever group index 1 of the match was, with a -b suffix.

`signet one-shots/**/.* rename (.*)-a <1>-b`

Convert all audio files in the folder "my_folder" (not recursively) to a sample rate of 44100Hz and a bit-depth of 24.

`signet my_folder convert sample-rate 44100 bit-depth 24`
