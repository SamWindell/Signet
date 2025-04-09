There are binaries available for the Windows, Linux (Ubuntu) and Mac versions of Signet. These can be found in the assets section of this Github release. Just download and extract the file to start using it.

The macOS version is a universal binary, codesigned and notarized.

Changes:

0.3.0:
- Slightly change behaviour of `--output-folder`: now, if the input files are within the CWD, the output files will be put into the output folder with the same structure of subfolders.
- Add `script` command to run a series of subcommands.

0.2.0:
- Rename subcommand `remove-silence` to `trim-silence`
- Change `norm` subcommand to take a `--mode` argument; one of `peak` or `rms` rather than `--rms` flag. Default mode is `peak`.
- Add `energy` mode to `norm` subcommand allowing for normalisation based on the energy of the audio (similar to RMS).
- Add `reverse` subcommand.
- Add `add-loop` subcommand.

0.1.8:
- Notarize the macOS version

0.1.7:
- Codesign the macOS version

0.1.6:
- Add --output-file option for specifying a single output file instead of overwriting the input file
- Fix filepath case sensitivity issues on Window and macOS
- Fix norm --rms when using --independent-channels

0.1.5:
- Prints a stack trace when a fatal error occurs
- Build universal binaries on macOS

0.1.4:
- print-info now shows peak meter info (dB) and length (seconds)
- Fix crash caused by having a wildcard at the start of the glob as well as a slash
- Add a mode to seamless-loop that can make a loop without using crossfades: this is used when the crossfade-size is 0. See seamless-loop documentation for more info.

0.1.3:
- Add --independent-channels and --mix-channels to norm command
- Improve the wording and usefulness of messages
- Add --output-folder option to Signet instead of always overwriting files
- --sample-sets now searches for sets across all input files rather than just in each folder
- Fix AudioDurations ms being interpreted as s
- Improve backup system
- Add --expected-note to auto-tune
- Improve usage.md style
- Compile with static runtime libraries on Windows

0.1.2:
- Add --make-same-length option to sample-blend
- Tweak fix-pitch drift algorithm and add extra utility options
- Add pan command
- Add --sample-set option to fix-pitch-drift and auto-tune
- Turn --undo and --clear-backup into commands rather than options (they work identically to as before)
- Improve colours and the format of the message output; each message now reports what filename it belongs to
- Improve what counts as an error and what counts as a warning
- Add --warnings-are-errors to require Signet to exit if any warning is issued
- Add option to norm to normalise each channel independently
- Fix-pitch-drift: add --expected-note, so as to only correct drift if the detected pitch is as expected
