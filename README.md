# AudioUtils
### Command line utilities for manipulating audio samples

These are very simple programs that I have written to edit audio samples for use in a sampler. There are no external dependencies. To build from source, run CMake to generate your preferred build tool configuration (Visual Studio Solution, makefile, etc.).

Run with the argument `--help` on each of the tools to see the command line interface.

### Current tools
- Zero crossing offsetter. Offsets the start of a audio file to the nearest zero-crossing. Optionally appends the samples that it skipped to the end of the file. If the sample is a seamless loop, this is neccessary to do to ensure the loop is still seamless.
