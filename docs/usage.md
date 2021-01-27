# Signet Usage

This is an auto-generated file based on the output of `signet --help`. It contains information about every feature of Signet.

- [Signet](#Signet)
- [Commands](#Signet%20Commands)
  - [auto-tune](#auto-tune)
  - [convert](#convert)
    - [sample-rate](#sample-rate)
    - [bit-depth](#bit-depth)
    - [file-format](#file-format)
  - [detect-pitch](#detect-pitch)
  - [embed-sampler-info](#embed-sampler-info)
    - [root](#root)
    - [note-range](#note-range)
    - [velocity-range](#velocity-range)
  - [fade](#fade)
    - [in](#in)
    - [out](#out)
  - [folderise](#folderise)
  - [gain](#gain)
  - [highpass](#highpass)
  - [lowpass](#lowpass)
  - [make-docs](#make-docs)
  - [move](#move)
  - [norm](#norm)
  - [print-info](#print-info)
  - [remove-silence](#remove-silence)
  - [rename](#rename)
    - [prefix](#prefix)
    - [suffix](#suffix)
    - [regex-replace](#regex-replace)
    - [note-to-midi](#note-to-midi)
    - [auto-map](#auto-map)
  - [sample-blend](#sample-blend)
  - [seamless-loop](#seamless-loop)
  - [trim](#trim)
    - [start](#start)
    - [end](#end)
  - [tune](#tune)
  - [zcross-offset](#zcross-offset)
# Signet
## Description:
Signet is a command-line program designed for bulk editing audio files. It has commands for converting, editing, renaming and moving WAV and FLAC files. It also features commands that generate audio files. Signet was primarily designed for people who make sample libraries, but its features can be useful for any type of bulk audio processing.

## Usage:
  `signet` `[OPTIONS]` `[input-files...]` `COMMAND`

## Arguments:
`input-files TEXT ...`
The audio files to process. You can specify more than one of these. Each input-file you specify has to be a file, directory or a glob pattern. You can exclude a pattern by beginning it with a dash. e.g. "-\*.wav" would exclude all .wav files that are in the current directory. If you specify a directory, all files within it will be considered input-files, but subdirectories will not be searched. You can use the --recursive flag to make signet search all subdirectories too.


## Options:
`--undo`
Undoes any changes made by the last run of Signet; files that were overwritten are restored, new files that were created are destroyed, and files that were renamed are un-renamed. You can only undo once - you cannot keep going back in history.

`--clear-backup`
Deletes all temporary files created by Signet. These files are needed for the undo system and are saved to your OS's temporary folder. These files are cleared and new ones created every time you run Signet. This option is only really useful if you have just processed lots of files and you won't be using Signet for a long time afterwards. You cannot use --undo directly after clearing the backup.

`--silent`
Disable all messages

`--recursive`
When the input is a directory, scan for files in it recursively


# Signet Commands
## auto-tune
### Description:
Tunes the file(s) to their nearest detected musical pitch. For example, a file with a detected pitch of 450Hz will be tuned to 440Hz (A4).

### Usage:
  `auto-tune`

## convert
### Description:
Converts the file format, bit-depth or sample rate. Features a high quality resampling algorithm. This command has subcommands; it requires at least one of sample-rate, bit-depth or file-format to be specified.

### Usage:
  `convert` `COMMAND`

### Commands:
#### sample-rate
##### Description:
Change the sample rate using a high quality resampler.


##### Arguments:
`sample-rate UINT:UINT in [1 - 4300000000] REQUIRED`
The target sample rate in Hz. For example 44100


#### bit-depth
##### Description:
Change the bit depth of the file(s).


##### Arguments:
`bit-depth UINT:{8,16,20,24,32,64} REQUIRED`
The target bit depth.


#### file-format
##### Description:
Change the file format.


##### Arguments:
`file-format ENUM:value in {Flac->1,Wav->0} OR {1,0} REQUIRED`
The output file format.



## detect-pitch
### Description:
Prints out the detected pitch of the file(s).

### Usage:
  `detect-pitch`

## embed-sampler-info
### Description:
Embeds sampler metadata into the audio file(s), such as the root note, the velocity mapping range and the note mapping range.

### Usage:
  `embed-sampler-info` `COMMAND`

### Commands:
#### root
##### Description:
Embed the root note of the audio file


##### Arguments:
`Root note value TEXT REQUIRED`
This value should be 1 of the following 3 formats:

(1) A MIDI note number.
(2) An ECMAScript-style regex pattern containing 1 capture group which is to be used to get the value from the filename of the audio file (not including the extension). For example if the file was called sample_40.wav, you could use the regex pattern "sample_(\d+)" to get the value 40.
(3) 'auto-detect' or one of the other auto-detect options. This will invoke the pitch-detection algorithm to automatically get the MIDI number from the audio file. There are a few variations for auto-detect. They all use the same algorithm but some also shift the value in octaves. Here is the full list of options: auto-detect, auto-detect-octave-plus-1, auto-detect-octave-plus-2, auto-detect-octave-minus-1, auto-detect-octave-minus-2, auto-detect-nearest-to-middle-c.

EXAMPLE

'root 60'
Sets the root note to MIDI number 60.

'root "sample_(\d+)"'
Sets the root note by looking at the filename and matching the given regex pattern to it.

'root auto-detect'
Sets the root note by running a pitch detection algorithm on the file. If the audio file does not have a pitch, the value will be set to 60.


#### note-range
##### Description:
Embed the low and high notes.


##### Arguments:
`Note range value(s) TEXT REQUIRED`
This value is either 'auto-map' or 2 separate values to set the low and high note range.

EXAMPLES

'note-range auto-map'
Auto maps the file based on the files in its same folder.

'note-range 40 62'
Sets the low note to MIDI note 40 and the high note to 62.

'note-range unchanged 127'
Leaves the low note unchanged (if no value exists already it will be set to 0), and sets the high note to 127.

'note-range "sample_(\d+)_\d+_\d+" "sample_\d+_\d+_(\d+)"'
Sets the low value from the name of the file by pattern-matching the given regex pattern to it, and taking the value of the given capture group (with is the bit inside the brackets). The high value is set from the filename too, but a different regex pattern is matched against.


DESCRIPTION

If it's auto-map, then the low and high note will be set by assessing the root note of every file in the same folder and calculating an appropriate value.

If not 'auto-map' then each of the 2 arguments can be 1 of 3 different formats. The first of these 2 arguments represents the low note, and the second represents the high note. The 3 possible formats are as follows: (1) a MIDI note number, (2) a regex pattern containing 1 capture group which is to be used to capture the value from the filename of the audio file (not including the extension). Or (3), the word 'unchanged' which means the value is not changed if it is already embedded in the file; if there is no value already present, it's set to 0 for the low note or 127 for the high note.


#### velocity-range
##### Description:
Embeds the velocity mapping info. Samplers can often play different samples depending on the MIDI velocity. In order to do this, the sampler needs to know what the minimum and maximum MIDI velocity values that a sample should play in. The whole MIDI velocity range is between 1 and 127.


##### Arguments:
`Low and high velocity values TEXT x 2 REQUIRED`
2 values to represent the low and high velocity mapping.

EXAMPLES

'velocity-range 1 64'
Sets the low velocity to 1 and the high velocity to 64.

'velocity-range "sample_(\d+)_\d+_\d+" "sample_\d+_\d+_(\d+)"'
Sets the low velocity from the name of the file, by matching the given regex pattern to it, and get the number from the captured region. Same is done for the high velocity but a different capture region is specified.

'velocity-range 1 unchanged'
Sets the low velocity to 1 and leaves the high velocity unchanged. If the high velocity is not already embedding the file, it will be set to 127

DESCRIPTION

2 values must be given. The first one represents the low velocity and the second one represents the high velocity. Each value can be 1 of 3 formats. (1) A number from 1 to 127, (2) a regex pattern containing 1 capture group which is to be used to capture the value from the filename of the audio file (not including the extension). Or (3), the word 'unchanged' which means the value is not changed if it is already embedded in the file; if there is no value already present, it's set to 1 for the low velocity or 127 for the high velocity.



## fade
### Description:
Adds a fade-in to the start and/or a fade-out to the end of the file(s). This subcommand has itself 2 subcommands, 'in' and 'out'; one of which must be specified. For each, you must specify first the fade length. You can then optionally specify the shape of the fade curve.

### Usage:
  `fade` `COMMAND`

### Commands:
#### in
##### Description:
Fade in the volume at the start of the file(s).


##### Arguments:
`fade-in length TEXT REQUIRED`
The length of the fade in. This value is a number directly followed by a unit. The unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.

`fade-in shape ENUM:value in {Exp->4,Linear->0,Log->3,SCurve->2,Sine->1,Sqrt->5} OR {4,0,3,2,1,5}`
The shape of the fade-in curve. The default is the 'sine' shape.


#### out
##### Description:
Fade out the volume at the end of the file(s).


##### Arguments:
`fade-out length TEXT REQUIRED`
The length of the fade out. This value is a number directly followed by a unit. The unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.

`fade-out shape ENUM:value in {Exp->4,Linear->0,Log->3,SCurve->2,Sine->1,Sqrt->5} OR {4,0,3,2,1,5}`
The shape of the fade-out curve. The default is the 'sine' shape.



## folderise
### Description:
Moves files into folders based on their names. This is done by specifying a regex pattern to match the name against. The folder in which the matched file should be moved to can be based off of the name. These folders are created if they do not already exist.

### Usage:
  `folderise` `filename-regex out-folder`

### Arguments:
`filename-regex TEXT REQUIRED`
The ECMAScript-style regex pattern used to match filenames against. The file extension is not part of this comparison.

`out-folder TEXT REQUIRED`
The output folder that the matching files should be put into. This will be created if it does not exist. It can contain numbers in angle brackets to signify where groups from the matching regex should be inserted. These means files can end up in multiple folders. For example, 'folderise file(\d+).wav C:/folder`<1>`' would create folders 'C:/folder1' and 'C:/folder2' if there were files 'file1.wav' and 'file2.wav'.


## gain
### Description:
Changes the volume of the file(s).

### Usage:
  `gain` `gain`

### Arguments:
`gain TEXT REQUIRED`
The gain amount. This is a number followed by a unit. The unit can be % or db. For example 10% or -3.5db. A gain of 50% makes the signal half as loud. A gain of 200% makes it twice as loud.


## highpass
### Description:
Removes frequencies below the given cutoff.

### Usage:
  `highpass` `cutoff-freq-hz`

### Arguments:
`cutoff-freq-hz FLOAT REQUIRED`
The cutoff point where frequencies below this should be removed.


## lowpass
### Description:
Lowpass: removes frequencies above the given cutoff.

### Usage:
  `lowpass` `cutoff-freq-hz`

### Arguments:
`cutoff-freq-hz FLOAT REQUIRED`
The cutoff point where frequencies above this should be removed.


## make-docs
### Description:
Creates a markdown file containing the full CLI - based on running signet --help.

### Usage:
  `make-docs` `output-file`

### Arguments:
`output-file TEXT REQUIRED`
The filepath for the generated markdown file.


## move
### Description:
Moves all input files to a given folder.

### Usage:
  `move` `[destination-folder]`

### Arguments:
`destination-folder TEXT`
The folder to put all of the input files in.


## norm
### Description:
Sets the peak amplitude to a certain level. When this is used on multiple files, each file is attenuated by the same amount. You can disable this by specifying the flag --independently.

### Usage:
  `norm` `[OPTIONS]` `target-decibels`

### Arguments:
`target-decibels FLOAT:INT in [-200 - 0] REQUIRED`
The target level in decibels, where 0dB is the max volume.


### Options:
`--independently`
When there are multiple files, normalise each one individually rather than by a common gain.

`--rms`
Use RMS (root mean squared) calculations to work out the required gain amount.

`--mix FLOAT:INT in [0 - 100]`
The mix of the normalised signal, where 100% means normalised exactly to the target, and 0% means no change.


## print-info
### Description:
Prints information about the audio file(s), such as the embedded metadata, sample-rate and RMS.

### Usage:
  `print-info`

## remove-silence
### Description:
Removes silence from the start or end of the file(s). Silence is considered anything under -90dB, however this threshold can be changed with the --threshold option.

### Usage:
  `remove-silence` `[OPTIONS]` `[start-or-end]`

### Arguments:
`start-or-end ENUM:value in {Both->2,End->1,Start->0} OR {2,1,0}`
Specify whether the removal should be at the start, the end or both.


### Options:
`--threshold FLOAT:INT in [-200 - 0]`
The threshold in decibels to which anything under it should be considered silence.


## rename
### Description:
Various commands for renaming files.

This command can be used to bulk rename a set of files. It also has the ability to insert special variables into the file name, such as the detected pitch. As well as this, there is a special auto-mapper command that is useful to sample library developers.

All options for this subcommand relate to just the name of the file - not the folder or the file extension.

Any text added via this command can contain special substitution variables; these will be replaced by values specified in this list:

`<counter>`
A unique number starting from zero. The ordering of these numbers is not specified.

`<alpha-counter>`
A unique 3 character counter starting from aaa and ending with zzz. Beyond zzz, `<alpha-counter>` will be replaced with a number instead. The ordering of these numbers is not specified.

`<detected-pitch>`
The detected pitch of audio file in Hz. If no pitch is found this variable will be empty.

`<detected-midi-note>`
The MIDI note number that is closest to the detected pitch of the audio file. If no pitch is found this variable will be empty.

`<detected-midi-note-octave-plus-1>`
The MIDI note number (+12 semitones) that is closest to the detected pitch of the audio file. If no pitch is found this variable will be empty.

`<detected-midi-note-octave-plus-2>`
The MIDI note number (+24 semitones) that is closest to the detected pitch of the audio file. If no pitch is found this variable will be empty.

`<detected-midi-note-octave-minus-1>`
The MIDI note number (-12 semitones) that is closest to the detected pitch of the audio file. If no pitch is found this variable will be empty.

`<detected-midi-note-octave-minus-2>`
The MIDI note number (-24 semitones) that is closest to the detected pitch of the audio file. If no pitch is found this variable will be empty.

`<detected-midi-note-octaved-to-be-nearest-to-middle-c>`
The MIDI note number that is closest to the detected pitch of the audio file, but moved by octaves to be nearest as possible to middle C. If no pitch is found this variable will be empty.

`<detected-note>`
The musical note-name that is closest to the detected pitch of the audio file. The note is capitalised, and the octave number is specified. For example 'C3'. If no pitch is found this variable will be empty.

`<parent-folder>`
The name of the folder that contains the audio file.

`<parent-folder-snake>`
The snake-case name of the folder that contains the audio file.

`<parent-folder-camel>`
The camel-case name of the folder that contains the audio file.

### Usage:
  `rename` `COMMAND`

### Commands:
#### prefix
##### Description:
Add text to the start of the filename.


##### Arguments:
`prefix-text TEXT REQUIRED`
The text to add, may contain substitution variables.


#### suffix
##### Description:
Add text to the end of the filename (before the extension).


##### Arguments:
`suffix-text TEXT REQUIRED`
The text to add, may contain substitution variables.


#### regex-replace
##### Description:
Replace names that match the given regex pattern. The replacement can contain regex-groups from the matched filename.


##### Arguments:
`regex-pattern TEXT REQUIRED`
The ECMAScript-style regex pattern to match filenames against - folder names or file extensions are ignored.

`regex-replacement TEXT REQUIRED`
The new filename for files that matched the regex. This may contain substitution variables. Matching groups from the regex can also be substituted into this new name. You achieve this similarly to the special variable substitution. However, this time you are put the regex group index in the angle-brackets (such as `<1>`). Remember that with regex, group index 0 is always the whole match.


#### note-to-midi
##### Description:
Replace all occurrences of note names with the corresponding MIDI note number. For example replace C3 with 60.


##### Options:
`--midi-zero-note TEXT`
The note that should represent MIDI note number 0. Default is C-1.


#### auto-map
##### Description:
Samplers can sometimes read the root, low and high MIDI note numbers from within a filename. This command makes inserting the low and high keys into the filename simple.

First you specify a regex pattern that captures a number representing the MIDI root note from the input filenames. This tool collects all of the root notes found in each folder, and works out reasonable values for what the low and high MIDI notes should be.

You control the format of the renaming by specifing a pattern containing substitution variables for `<lo>`, `<root>` and `<high>`. These variables are replaced by the MIDI note numbers in the range 0 to 127.


##### Arguments:
`auto-map-filename-pattern TEXT REQUIRED`
The ECMAScript-style regex the should match against filename (excluding extension). This regex should contain a capture group to represent the root note of the sample.

`root-note-regex-group INT`
The group number that represents the MIDI root note number. Remember regex group 0 is always the full match.

`auto-map-renamed-filename TEXT REQUIRED`
The name of the output file (excluding extension). This should contain substitution variables `<lo>`, `<root>` and `<hi>` which will be replaced by the low MIDI note number, the root MIDI note number and the high MIDI note number. The low and high numbers are generated by the auto-mapper so that all samples in each folder will fill out the entire range 0-127. Matching groups from the regex can also be substituted into this new name. To do this, add the regex group index in the angle-brackets (such as `<1>`).



## sample-blend
### Description:
Creates samples in between other samples that are different pitches. It takes 2 samples and generates a set of samples in between them at a given semitone interval. Each generated sample is a different blend of the 2 base samples, tuned to match each other. This tool is useful when you have a multi-sampled instrument that was sampled only at large intervals; such as every octave. This tool can be used to create an instrument that sounds like it was sampled at smaller intervals.

### Usage:
  `sample-blend` `root_note_regex semitone-interval out-filename`

### Arguments:
`root_note_regex TEXT REQUIRED`
Regex pattern containing 1 group that is to match the root note

`semitone-interval INT REQUIRED`
The semitone interval at which to generate new samples by

`out-filename TEXT REQUIRED`
The filename of the generated files (excluding extension). It should contain either the substitution variable `<root-num>` or `<root-note>` which will be replaced by the root note of the generated file. `<root-num>` is replaced by the MIDI note number, and `<root-name>` is replaced by the note name, such as C3.


## seamless-loop
### Description:
Turns the files(s) into seamless loops by crossfading a given percentage of audio from the start of the file to the end of the file. Due to this overlap, the resulting file is shorter.

### Usage:
  `seamless-loop` `[crossfade-percent]`

### Arguments:
`crossfade-percent FLOAT:INT in [0 - 100]`
The size of the crossfade region as a percent of the whole file.


## trim
### Description:
Removes the start or end of the file(s). This command has 2 subcommands, 'start' and 'end'; one of which must be specified. For each, the amount to remove must be specified.

### Usage:
  `trim` `COMMAND`

### Commands:
#### start
##### Description:
Removes the start of the file.


##### Arguments:
`trim-start-length TEXT REQUIRED`
The amount to remove from the start. This value is a number directly followed by a unit. The unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.


#### end
##### Description:
Removes the end of the file.


##### Arguments:
`trim-end-length TEXT REQUIRED`
The amount to remove from the end. This value is a number directly followed by a unit. The unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.



## tune
### Description:
Changes the tune the file(s) by stretching or shrinking them. Uses a high quality resampling algorithm.

### Usage:
  `tune` `tune cents`

### Arguments:
`tune cents FLOAT REQUIRED`
The cents to change the pitch by.


## zcross-offset
### Description:
Offsets the start of an audio file to the nearest zero-crossing (or the closest thing to a zero crossing). You can use the option --append to cause the samples that were offsetted to be appended to the end of the file. This is useful for when the file is a seamless loop.

### Usage:
  `zcross-offset` `[OPTIONS]` `search_size`

### Arguments:
`search_size TEXT REQUIRED`
The maximum length that it is allowed to offset to. This value is a number directly followed by a unit. The unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.


### Options:
`--append`
Append the frames offsetted to the end of the file - useful when the sample is a seamless loop.


