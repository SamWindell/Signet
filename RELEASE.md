Changes:
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

There are experimental binaries available for the Windows, Linux and Mac versions of Signet. These can be found in the assets section of this Github release. Just download and extract the file to start using it.

In all likelihood, **the Mac version will not run** due to it not being codesigned or notarized. Deploying software on Mac often requires these extra steps.