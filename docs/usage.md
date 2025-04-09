# Signet Usage

This is an auto-generated file based on the output of `signet --help`. It contains information about every feature of Signet.

- [General Usage](#General-Usage)
- [Audio Commands](#Audio-Commands)
  - [add-loop](#sound-add-loop)
  - [auto-tune](#sound-auto-tune)
  - [fade](#sound-fade)
    - [in](#in)
    - [out](#out)
  - [fix-pitch-drift](#sound-fix-pitch-drift)
  - [gain](#sound-gain)
  - [highpass](#sound-highpass)
  - [lowpass](#sound-lowpass)
  - [norm](#sound-norm)
  - [pan](#sound-pan)
  - [reverse](#sound-reverse)
  - [seamless-loop](#sound-seamless-loop)
  - [trim](#sound-trim)
    - [start](#start)
    - [end](#end)
  - [trim-silence](#sound-trim-silence)
  - [tune](#sound-tune)
  - [zcross-offset](#sound-zcross-offset)
- [File Data Commands](#File-Data-Commands)
  - [convert](#sound-convert)
    - [sample-rate](#sample-rate)
    - [bit-depth](#bit-depth)
    - [file-format](#file-format)
  - [embed-sampler-info](#sound-embed-sampler-info)
    - [remove](#remove)
    - [root](#root)
    - [note-range](#note-range)
    - [velocity-range](#velocity-range)
- [Filepath Commands](#Filepath-Commands)
  - [folderise](#sound-folderise)
  - [move](#sound-move)
  - [rename](#sound-rename)
    - [prefix](#prefix)
    - [suffix](#suffix)
    - [regex-replace](#regex-replace)
    - [note-to-midi](#note-to-midi)
    - [auto-map](#auto-map)
- [Generate Commands](#Generate-Commands)
  - [sample-blend](#sound-sample-blend)
- [Info Commands](#Info-Commands)
  - [detect-pitch](#sound-detect-pitch)
  - [print-info](#sound-print-info)
- [Signet Utility Commands](#Signet-Utility-Commands)
  - [clear-backup](#sound-clear-backup)
  - [make-docs](#sound-make-docs)
  - [script](#sound-script)
  - [undo](#sound-undo)

# General Usage
## Description:
Signet is a command-line program designed for bulk editing audio files. It has commands for converting, editing, renaming and moving WAV and FLAC files. It also features commands that generate audio files. Signet was primarily designed for people who make sample libraries, but its features can be useful for any type of bulk audio processing.

## Usage:
  `signet` `[OPTIONS]` `[input-files...]` `COMMAND`

## POSITIONALS:
`input-files TEXT ...`
The audio files to process. You can specify more than one of these. Each input-file you specify has to be a file, directory or a glob pattern. You can exclude a pattern by beginning it with a dash. e.g. "-\*.wav" would exclude all .wav files that are in the current directory. If you specify a directory, all files within it will be considered input-files, but subdirectories will not be searched. You can use the --recursive flag to make signet search all subdirectories too.

## OPTIONS:
`--version`
Display the version of Signet

`--silent`
Disable all messages

`--warnings-are-errors`
Attempt to exit Signet and return a non-zero value as soon as possible if a warning occurs.

`--recursive`
When the input is a directory, scan for files in it recursively.

`--output-folder TEXT Excludes: --output-file`
Instead of overwriting the input files, processed audio files are put into the given output folder. If your input files are within the current working directory they will be placed inside the output folder with the same structure of subfolders. If not, then the files are put at the top level of the output folder. This option takes 1 argument - the path of the folder where the files should be moved to. You can specify this folder to be the same as any of the input folders, however, you will need to use the rename command to avoid overwriting the files. If the output folder does not already exist it will be created. Some commands do not allow this option - such as move.

`--output-file TEXT Excludes: --output-folder`
Write to a single output file rather than overwrite the original. Only valid if there's only 1 input file. If the output file already exists it is overwritten. Directories are created. Some commands do not allow this option - such as move.

# Audio Commands
## :sound: add-loop
### Description:
Adds a loop to the audio file(s). The loop is defined by a start point and either an end point or number of frames. These points can be negative, meaning they are relative to the end of the file.

### Usage:
  `add-loop` `[OPTIONS]` `start-point [end-point]`

### POSITIONALS:
`start-point TEXT REQUIRED`
The start point of the loop. This value is a number in samples, or a number directly followed by a unit: the unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp. If negative, it's measured from the end of the file.

`end-point TEXT Excludes: --num-frames`
The end point of the loop. This value is a number in samples, or a number directly followed by a unit: the unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp. If negative, it's measured from the end of the file. 0 means the end sample.

### OPTIONS:
`--num-frames TEXT Excludes: end-point`
Number of frames in the loop.  Can be used instead of specifying an end-point. This value is a number in samples, or a number directly followed by a unit: the unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.

`--name TEXT`
Optional name for the loop.

`--type ENUM:value in {Backward->1,Forward->0,PingPong->2} OR {1,0,2} [0] `
Type of loop. Default is Forward.

`--loop-count UINT [0] `
Number of times to loop. 0 means infinite looping (default).

## :sound: auto-tune
### Description:
Tunes the file(s) to their nearest detected musical pitch. For example, a file with a detected pitch of 450Hz will be tuned to 440Hz (A4). The whole audio is analysed, and the most frequent and prominent pitch is determined. The whole audio is then retuned as if by using Signet's tune command (i.e. sped up or slowed down). This command works surprising well for almost any type of sample - transparently shifting it by the smallest amount possible to be more musically in-tune.

### Usage:
  `auto-tune` `[OPTIONS]`

### OPTIONS:
`--sample-sets TEXT x 2`
Rather than process each file individually, identify sets of files and process the files in each set in an identical manner based on a single authority file in that set. 
    
For example, you might have a set of samples of something recorded simultaneously with different microphones; you can use this tool to process all of the samples in the same way based on the close mic.
    
To allow for batch processing (as is the goal of Signet), this option is flexible and therefore requires a little explanation.

This option requires 2 arguments. 

The first argument is a regex pattern that will be used to identify sample sets from all of the file names (not including folders or extension). This must capture a single regex group. The bit that you capture is the bit of text that is different for each name in the set.

For example, take a folder of files like this:
sample-C2-close.flac
sample-C2-room.flac
sample-C2-ambient.flac
sample-D2-close.flac
sample-D2-room.flac
sample-D2-ambient.flac

In this example, close, room and ambient are names of commonly used mic positions. Each mic is capturing the same sound source at the same time; we therefore want any processing done on these files to be identical.

The differentiator (first arg) should be ".\*(close|room|ambient).\*". 

This is because "close|room|ambient" is the ONLY bit that changes between the file names of each set. This option does not work if samples in a set have multiple parts that are different.

The second argument required for this command is used to determine what should be the authority for the processing. This is a string that should match against whatever we have captured in the first argument. In this example case, it would be the word "close", because we want the close mics to be the authority.

Putting it all together, here's what the full command would look like for our example:

signet sample-\* auto-tune --sample-sets ".\*(close|room|ambient).\*" "close"

The entire folder of different mic positions can be processed in a single command. For a simpler version of this option, see --authority-file.

`--authority-file TEXT`
Rather than process each file individually, process all of the files in an identical manner based on a single authority file. This takes 1 argument: the name (without folders or extension) of the file that should be the authority.
    
This is the same as --sample-sets, but just takes a single filename for all of the files (rather than allowing multiple sets to be identified using a regex pattern

`--expected-note TEXT`
Only correct the audio if the detected target pitch matches the one given (or any octave of that note). To do this, specify a regex pattern that has a single capture group. This will be compared against each filename (excluding folder or file extension). The bit that you capture should be the MIDI note number of the audio file. You can also optionally specify an additional argument: the octave number for MIDI note zero (the default is that MIDI note 0 is C-1).

Example: auto-tune --expected-note ".\*-note-(\d+)-.\*" 0
This would find the digits after the text '-note-' in the filename and interpret them as the expected pitch of the track using 0 as the octave number for MIDI note 0.


### Examples:
```
  signet file.wav auto-tune
  signet sample-* auto-tune --sample-sets ".*(close|room|ambient).*" "close"
  signet sample-*.wav auto-tune --authority-file "sample-close"
  signet piano-root-*-*.wav auto-tune --expected-note "piano-root-(\d+)-.*"
```

## :sound: fade
### Description:
Adds a fade-in to the start and/or a fade-out to the end of the file(s). This subcommand has itself 2 subcommands, 'in' and 'out'; one of which must be specified. For each, you must specify first the fade length. You can then optionally specify the shape of the fade curve.

### Usage:
  `fade` `COMMAND`

### Commands:
#### in
##### Description:
Fade in the volume at the start of the file(s).

##### POSITIONALS:
`fade-in-length TEXT REQUIRED`
The length of the fade in. This value is a number in samples, or a number directly followed by a unit: the unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.

`fade-in-shape ENUM:value in {Exp->4,Linear->0,Log->3,SCurve->2,Sine->1,Sqrt->5} OR {4,0,3,2,1,5}`
The shape of the fade-in curve. The default is the 'sine' shape.


#### out
##### Description:
Fade out the volume at the end of the file(s).

##### POSITIONALS:
`fade-out-length TEXT REQUIRED`
The length of the fade out. This value is a number in samples, or a number directly followed by a unit: the unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.

`fade-out-shape ENUM:value in {Exp->4,Linear->0,Log->3,SCurve->2,Sine->1,Sqrt->5} OR {4,0,3,2,1,5}`
The shape of the fade-out curve. The default is the 'sine' shape.

## :sound: fix-pitch-drift
### Description:
Automatically corrects regions of drifting pitch in the file(s). This tool is ideal for samples of single-note instruments that subtly drift out of pitch, such as a human voice or a wind instrument. It analyses the audio for regions of consistent pitch (avoiding noise or silence), and for each of these regions, it smoothly speeds up or slows down the audio to counteract any drift pitch. The result is a file that stays in-tune throughout its duration. Only the drifting pitch is corrected by this tool; it does not tune the audio to be a standard musical pitch. See Signet's other auto-tune command for that. As well as this, fix-pitch-drift is a bit more specialised and does not always work as ubiquitously as Signet's other auto-tune command.

### Usage:
  `fix-pitch-drift` `[OPTIONS]`

### OPTIONS:
`--sample-sets TEXT x 2`
Rather than process each file individually, identify sets of files and process the files in each set in an identical manner based on a single authority file in that set. 
    
For example, you might have a set of samples of something recorded simultaneously with different microphones; you can use this tool to process all of the samples in the same way based on the close mic.
    
To allow for batch processing (as is the goal of Signet), this option is flexible and therefore requires a little explanation.

This option requires 2 arguments. 

The first argument is a regex pattern that will be used to identify sample sets from all of the file names (not including folders or extension). This must capture a single regex group. The bit that you capture is the bit of text that is different for each name in the set.

For example, take a folder of files like this:
sample-C2-close.flac
sample-C2-room.flac
sample-C2-ambient.flac
sample-D2-close.flac
sample-D2-room.flac
sample-D2-ambient.flac

In this example, close, room and ambient are names of commonly used mic positions. Each mic is capturing the same sound source at the same time; we therefore want any processing done on these files to be identical.

The differentiator (first arg) should be ".\*(close|room|ambient).\*". 

This is because "close|room|ambient" is the ONLY bit that changes between the file names of each set. This option does not work if samples in a set have multiple parts that are different.

The second argument required for this command is used to determine what should be the authority for the processing. This is a string that should match against whatever we have captured in the first argument. In this example case, it would be the word "close", because we want the close mics to be the authority.

Putting it all together, here's what the full command would look like for our example:

signet sample-\* fix-pitch-drift --sample-sets ".\*(close|room|ambient).\*" "close"

The entire folder of different mic positions can be processed in a single command. For a simpler version of this option, see --authority-file.

`--authority-file TEXT`
Rather than process each file individually, process all of the files in an identical manner based on a single authority file. This takes 1 argument: the name (without folders or extension) of the file that should be the authority.
    
This is the same as --sample-sets, but just takes a single filename for all of the files (rather than allowing multiple sets to be identified using a regex pattern

`--chunk-ms FLOAT:INT in [20 - 200]`
fix-pitch-drift evaluates the audio in small chunks. The pitch of each chunk is determined in order to get a picture of the audio's pitch over time. You can set the chunk size with this option. The default is 60 milliseconds. If you are finding this tool is incorrectly changing the pitch, you might try increasing the chunk size by 10 ms or so.

`--expected-note TEXT`
Only correct the audio if the detected target pitch matches the one given. To do this, specify a regex pattern that has a single capture group. This will be compared against each filename (excluding folder or file extension). The bit that you capture should be the MIDI note number of the audio file. You can also optionally specify an additional argument: the octave number for MIDI note zero (the default is that MIDI note 0 is C-1).

Example: fix-pitch-drift --expected-note ".\*-note-(\d+)-.\*" 0
This would find the digits after the text '-note-' in the filename and interpret them as the expected pitch of the track using 0 as the octave number for MIDI note 0.

`--print-csv`
Print a block of CSV data that can be loaded into a spreadsheet in order to determine what fix-pitch-drift is doing to the audio over time.

## :sound: gain
### Description:
Changes the volume of the file(s).

### Usage:
  `gain` `gain-amount`

### POSITIONALS:
`gain-amount TEXT REQUIRED`
The gain amount. This is a number followed by a unit. The unit can be % or db. For example 10% or -3.5db. A gain of 50% makes the signal half as loud. A gain of 200% makes it twice as loud.

## :sound: highpass
### Description:
Removes frequencies below the given cutoff.

### Usage:
  `highpass` `cutoff-freq-hz`

### POSITIONALS:
`cutoff-freq-hz FLOAT REQUIRED`
The cutoff point where frequencies below this should be removed.

## :sound: lowpass
### Description:
Lowpass: removes frequencies above the given cutoff.

### Usage:
  `lowpass` `cutoff-freq-hz`

### POSITIONALS:
`cutoff-freq-hz FLOAT REQUIRED`
The cutoff point where frequencies above this should be removed.

## :sound: norm
### Description:
Sets the peak amplitude to a given level (normalisation). When this is used on multiple files, each file is altered by the same amount; preserving their volume levels relative to each other (sometimes known as common-gain normalisation). Alternatively, you can make each file always normalise to the target by specifying the flag --independently.

### Usage:
  `norm` `[OPTIONS]` `target-decibels`

### POSITIONALS:
`target-decibels FLOAT:INT in [-200 - 0] REQUIRED`
The target level in decibels, where 0dB is the max volume.

### OPTIONS:
`--independently`
When there are multiple files, normalise each one individually rather than by a common gain.

`--independent-channels`
Normalise each channel independently rather than scale them together.

`--mode ENUM:value in {Energy->2,Peak->0,Rms->1} OR {2,0,1}`
The mode for normalisation calculations. The default is peak. Use peak calculations to work out the required gain amount. This is the default.Use average RMS (root mean squared) calculations to work out the required gain amount. In other words, the whole file's loudness is analysed, rather than just the peak. This does not work well with audio that has large fluctuations in volume level.Use energy (power) normalization to calculate the required gain amount. This sums the squares of all samples and normalizes based on total energy content. Particularly effective for impulse responses and convolution reverb sources, as it helps consistent perceived loudness when different IRs are applied to audio.

`--mix FLOAT:INT in [0 - 100]`
The mix of the normalised signal, where 100% means normalise to exactly to the target, and 50% means apply a gain to get halfway from the current level to the target. The default is 100%.

`--mix-channels FLOAT:INT in [0 - 100] Needs: --independent-channels`
When --independent-channels is also given, this option controls the mix of each channels normalised signal, where 100% means normalise to exactly to the target, and 50% means apply a gain to get halfway from the current level to the target. The default is 100%.

`--crest-factor-scaling FLOAT:INT in [0 - 100]`
Add an additional volume reduction for audio that has very low crest factors; in other words, audio that is consistently loud. This is useful when trying to achieve a consistent perceived loudness. A value of 0 means no reduction, and 100 means reduce the volume of non-peaky audio by 12dB. The default is 0.

## :sound: pan
### Description:
Changes the pan of stereo file(s). Does not work on non-stereo files.

### Usage:
  `pan` `pan-amount`

### POSITIONALS:
`pan-amount TEXT REQUIRED`
The pan amount. This is a number from 0 to 100 followed by either L or R (representing left or right). For example: 100R (full right pan), 100L (full left pan), 10R (pan right with 10% intensity).

## :sound: reverse
### Description:
Reverses the audio in the file(s).

### Usage:
  `reverse`

## :sound: seamless-loop
### Description:
Turns the file(s) into seamless loops. If you specify a crossfade-percent of 0, the algorithm will trim the file down to the smallest possible seamless-sounding loop, which starts and ends on a zero crossings. Useful if you have samples that you know are regularly repeating (for example a synth sawtooth). If you specify a non-zero crossfade-percent, the given percentage of audio from the start of the file will be faded onto the end of the file. Due to this overlap, the resulting file is shorter.

### Usage:
  `seamless-loop` `crossfade-percent [strictness-percent]`

### POSITIONALS:
`crossfade-percent FLOAT:INT in [0 - 100] REQUIRED`
The size of the crossfade region as a percent of the whole file. If this is zero then the algorithm will scan the file for the smallest possible loop, starting and ending on zero-crossings, and trim the file to that be that loop.

`strictness-percent FLOAT:INT in [1 - 100]`
How strict should the algorithm be when detecting loops when you specify 0 for crossfade-percent. Has no use if crossfade-percent is non-zero. Default is 50.

## :sound: trim
### Description:
Removes the start or end of the file(s). This command has 2 subcommands, 'start' and 'end'; one of which must be specified. For each, the amount to remove must be specified.

### Usage:
  `trim` `COMMAND`

### Commands:
#### start
##### Description:
Removes the start of the file.

##### POSITIONALS:
`trim-start-length TEXT REQUIRED`
The amount to remove from the start. This value is a number in samples, or a number directly followed by a unit: the unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.


#### end
##### Description:
Removes the end of the file.

##### POSITIONALS:
`trim-end-length TEXT REQUIRED`
The amount to remove from the end. This value is a number in samples, or a number directly followed by a unit: the unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.

## :sound: trim-silence
### Description:
Trims silence from the start or end of the file(s). Silence is considered anything under -90dB, however this threshold can be changed with the --threshold option.

### Usage:
  `trim-silence` `[OPTIONS]` `[start-or-end]`

### POSITIONALS:
`start-or-end ENUM:value in {Both->2,End->1,Start->0} OR {2,1,0}`
Specify whether the removal should be at the start, the end or both.

### OPTIONS:
`--sample-sets TEXT x 2`
Rather than process each file individually, identify sets of files and process the files in each set in an identical manner based on a single authority file in that set. 
    
For example, you might have a set of samples of something recorded simultaneously with different microphones; you can use this tool to process all of the samples in the same way based on the close mic.
    
To allow for batch processing (as is the goal of Signet), this option is flexible and therefore requires a little explanation.

This option requires 2 arguments. 

The first argument is a regex pattern that will be used to identify sample sets from all of the file names (not including folders or extension). This must capture a single regex group. The bit that you capture is the bit of text that is different for each name in the set.

For example, take a folder of files like this:
sample-C2-close.flac
sample-C2-room.flac
sample-C2-ambient.flac
sample-D2-close.flac
sample-D2-room.flac
sample-D2-ambient.flac

In this example, close, room and ambient are names of commonly used mic positions. Each mic is capturing the same sound source at the same time; we therefore want any processing done on these files to be identical.

The differentiator (first arg) should be ".\*(close|room|ambient).\*". 

This is because "close|room|ambient" is the ONLY bit that changes between the file names of each set. This option does not work if samples in a set have multiple parts that are different.

The second argument required for this command is used to determine what should be the authority for the processing. This is a string that should match against whatever we have captured in the first argument. In this example case, it would be the word "close", because we want the close mics to be the authority.

Putting it all together, here's what the full command would look like for our example:

signet sample-\* trim-silence --sample-sets ".\*(close|room|ambient).\*" "close"

The entire folder of different mic positions can be processed in a single command. For a simpler version of this option, see --authority-file.

`--authority-file TEXT`
Rather than process each file individually, process all of the files in an identical manner based on a single authority file. This takes 1 argument: the name (without folders or extension) of the file that should be the authority.
    
This is the same as --sample-sets, but just takes a single filename for all of the files (rather than allowing multiple sets to be identified using a regex pattern

`--threshold FLOAT:INT in [-200 - 0]`
The threshold in decibels to which anything under it should be considered silence.

## :sound: tune
### Description:
Changes the tune of the file(s) by stretching or shrinking them. Uses a high-quality resampling algorithm. Tuning up will result in audio that is shorter in duration, and tuning down will result in longer audio.

### Usage:
  `tune` `cents`

### POSITIONALS:
`cents FLOAT REQUIRED`
The cents to change the pitch by. For example, a value of 1200 would tune the audio up an octave. A value of -80 would tune the audio down by nearly an octave.


### Examples:
```
  signet file.wav tune -100
  signet folder-name tune 1200
```

## :sound: zcross-offset
### Description:
Offsets the start of an audio file to the nearest zero-crossing (or the closest thing to a zero crossing). You can use the option --append to cause the samples that were offsetted to be appended to the end of the file. This is useful for when the file is a seamless loop.

### Usage:
  `zcross-offset` `[OPTIONS]` `search_size`

### POSITIONALS:
`search_size TEXT REQUIRED`
The maximum length that it is allowed to offset to. This value is a number in samples, or a number directly followed by a unit: the unit can be one of {s, ms, %, smp}. These represent {Seconds, Milliseconds, Percent, Samples} respectively. The percent option specifies the duration relative to the whole length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.

### OPTIONS:
`--append`
Append the frames offsetted to the end of the file - useful when the sample is a seamless loop.

# File Data Commands
## :sound: convert
### Description:
Converts the file format, bit-depth or sample rate. Features a high quality resampling algorithm. This command has subcommands; it requires at least one of sample-rate, bit-depth or file-format to be specified.

### Usage:
  `convert` `COMMAND`

### Commands:
#### sample-rate
##### Description:
Change the sample rate using a high quality resampler.

##### POSITIONALS:
`sample-rate UINT:UINT in [1 - 4300000000] REQUIRED`
The target sample rate in Hz. For example 44100


#### bit-depth
##### Description:
Change the bit depth of the file(s).

##### POSITIONALS:
`bit-depth UINT:{8,16,20,24,32,64} REQUIRED`
The target bit depth.


#### file-format
##### Description:
Change the file format.

##### POSITIONALS:
`file-format ENUM:value in {Flac->1,Wav->0} OR {1,0} REQUIRED`
The output file format.


### Examples:
```
  signet . convert file-format flac sample-rate 44100 bit-depth 16
  signet *.wav convert file-format wav bit-depth 24
```

## :sound: embed-sampler-info
### Description:
Embeds sampler metadata into the audio file(s), such as the root note, the velocity mapping range and the note mapping range.

### Usage:
  `embed-sampler-info` `COMMAND`

### Commands:
#### remove
##### Description:
Remove all sampler metadata from the file(s)


#### root
##### Description:
Embed the root note of the audio file

##### POSITIONALS:
`root-note-value TEXT REQUIRED`
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

##### POSITIONALS:
`note-range-values TEXT REQUIRED`
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

##### POSITIONALS:
`low-and-high-velocity-values TEXT x 2 REQUIRED`
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

# Filepath Commands
## :sound: folderise
### Description:
Moves files into folders based on their names. This is done by specifying a regex pattern to match the name against. The folder in which the matched file should be moved to can be based off of the name. These folders are created if they do not already exist.

### Usage:
  `folderise` `filename-regex out-folder`

### POSITIONALS:
`filename-regex TEXT REQUIRED`
The ECMAScript-style regex pattern used to match filenames against. The file extension is not part of this comparison.

`out-folder TEXT REQUIRED`
The output folder that the matching files should be put into. This will be created if it does not exist. It can contain numbers in angle brackets to signify where groups from the matching regex should be inserted. These means files can end up in multiple folders. For example, 'folderise file(\d+).wav C:/folder<1>' would create folders 'C:/folder1' and 'C:/folder2' if there were files 'file1.wav' and 'file2.wav'.

## :sound: move
### Description:
Moves all input files to a given folder.

### Usage:
  `move` `[destination-folder]`

### POSITIONALS:
`destination-folder TEXT`
The folder to put all of the input files in.

## :sound: rename
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

`<detected-midi-note-octave-nearest-to-middle-c>`
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

##### POSITIONALS:
`prefix-text TEXT REQUIRED`
The text to add, may contain substitution variables.


#### suffix
##### Description:
Add text to the end of the filename (before the extension).

##### POSITIONALS:
`suffix-text TEXT REQUIRED`
The text to add, may contain substitution variables.


#### regex-replace
##### Description:
Replace names that match the given regex pattern. The replacement can contain regex-groups from the matched filename.

##### POSITIONALS:
`regex-pattern TEXT REQUIRED`
The ECMAScript-style regex pattern to match filenames against - folder names or file extensions are ignored.

`regex-replacement TEXT REQUIRED`
The new filename for files that matched the regex. This may contain substitution variables. Matching groups from the regex can also be substituted into this new name. You achieve this similarly to the special variable substitution. However, this time you are put the regex group index in the angle-brackets (such as <1>). Remember that with regex, group index 0 is always the whole match.


#### note-to-midi
##### Description:
Replace all occurrences of note names with the corresponding MIDI note number. For example replace C3 with 60.

##### OPTIONS:
`--midi-zero-note TEXT`
The note that should represent MIDI note number 0. Default is C-1.


#### auto-map
##### Description:
Samplers can sometimes read the root, low and high MIDI note numbers from within a filename. This command makes inserting the low and high keys into the filename simple.

First you specify a regex pattern that captures a number representing the MIDI root note from the input filenames. This tool collects all of the root notes found in each folder, and works out reasonable values for what the low and high MIDI notes should be.

You control the format of the renaming by specifing a pattern containing substitution variables for `<lo>`, `<root>` and `<high>`. These variables are replaced by the MIDI note numbers in the range 0 to 127.

##### POSITIONALS:
`auto-map-filename-pattern TEXT REQUIRED`
The ECMAScript-style regex the should match against filename (excluding extension). This regex should contain a capture group to represent the root note of the sample.

`root-note-regex-group INT`
The group number that represents the MIDI root note number. Remember regex group 0 is always the full match.

`auto-map-renamed-filename TEXT REQUIRED`
The name of the output file (excluding extension). This should contain substitution variables `<lo>`, `<root>` and `<hi>` which will be replaced by the low MIDI note number, the root MIDI note number and the high MIDI note number. The low and high numbers are generated by the auto-mapper so that all samples in each folder will fill out the entire range 0-127. Matching groups from the regex can also be substituted into this new name. To do this, add the regex group index in the angle-brackets (such as <1>).


### Examples:
```
  signet my-folder rename prefix "clarinet-"
  signet **.wav rename suffix "-root-<detected-midi-note>"
  signet session1 rename regex-replace ""
```

# Generate Commands
## :sound: sample-blend
### Description:
Creates samples in between other samples that are different pitches. It takes 2 samples and generates a set of samples in between them at a given semitone interval. Each generated sample is a different blend of the 2 base samples, tuned to match each other. This tool is useful when you have a multi-sampled instrument that was sampled only at large intervals; such as every octave. This tool can be used to create an instrument that sounds like it was sampled at smaller intervals.

### Usage:
  `sample-blend` `[OPTIONS]` `root_note_regex semitone-interval out-filename`

### POSITIONALS:
`root_note_regex TEXT REQUIRED`
Regex pattern containing 1 group that is to match the root note

`semitone-interval INT REQUIRED`
The semitone interval at which to generate new samples by

`out-filename TEXT REQUIRED`
The filename of the generated files (excluding extension). It should contain either the substitution variable `<root-num>` or `<root-note>` which will be replaced by the root note of the generated file. `<root-num>` is replaced by the MIDI note number, and `<root-name>` is replaced by the note name, such as C3.

### OPTIONS:
`--make-same-length`
For each generated file, if the 2 files that are being combined are not the same length, the longer one will be trimmed to the same length as the shorter before they are blended.


### Examples:
```
  signet Clav_40.wav Clav_52.wav sample-blend "Clav_.*(40|52).*" 2 "Clavout_<root-num>"
  signet Clav_*.wav sample-blend "Clav_.*(\d+).*" 2 "Clavout_<root-num>"
  signet sustain_sample_*.flac sample-blend --make-same-length "sustain_sample_(\d+).flac" 1 "sustain_sample_<root-num>"
```

# Info Commands
## :sound: detect-pitch
### Description:
Prints out the detected pitch of the file(s).

### Usage:
  `detect-pitch`

## :sound: print-info
### Description:
Prints information about the audio file(s), such as the embedded metadata, sample-rate and RMS.

### Usage:
  `print-info`

# Signet Utility Commands
## :sound: clear-backup
### Description:
Deletes all temporary files created by Signet. These files are needed for the undo system and are saved to your OS's temporary folder. These files are cleared and new ones created every time you run Signet. This option is only really useful if you have just processed lots of files and you won't be using Signet for a long time afterwards. You cannot use undo directly after clearing the backup.

### Usage:
  `clear-backup`

## :sound: make-docs
### Description:
Creates a Github flavour markdown file containing the full CLI - based on running signet --help.

### Usage:
  `make-docs` `output-file`

### POSITIONALS:
`output-file TEXT REQUIRED`
The filepath for the generated markdown file.

## :sound: script
### Description:
Run a script file containing a list of commands to run. The script file should be a text file with one command per line. The commands should be in the same format as you would use on the command line. Empty lines or lines starting with # are ignored.

### Usage:
  `script` `[script-file]`

### POSITIONALS:
`script-file TEXT:PATH(existing)`
The filepath for the script file.

## :sound: undo
### Description:
Undo any changes made by the last run of Signet; files that were overwritten are restored, new files that were created are destroyed, and files that were renamed are un-renamed. You can only undo once - you cannot keep going back in history.

### Usage:
  `undo`

