## Synopsis

A very simple wrapper library for the ffmpeg executable for Linux. It supports feeding a video stream and optionnally an audio stream to ffmpeg and will grab the output stream. The streams are handled through user supplied functors. 

## Code Example

The following excerp for the supplied [test](test_wrapper.cpp) show the basic flow for using the wrapper:
```c++
  ffmpeg_wrapper::FFMpegWrapper muxer(
      std::make_unique<InputFileReader>(
          opt_map[OPT_INPUT_VIDEO_FILE].as<std::string>()),
      {"-f", "h264", "-r", "25", "-probesize", "1024"},
      std::make_unique<InputFileReader>(
          opt_map[OPT_INPUT_AUDIO_FILE].as<std::string>()),
      {"-f", "flac"},
      std::make_unique<OutputFileWriter>(
          opt_map[OPT_OUTPUT_FILE].as<std::string>()),
      {"-vcodec", "copy", "-f", "mp4", "-reset_timestamps", "1",
          "-movflags", "empty_moov+default_base_moof+frag_keyframe"});
```
The InputFileReader and OutputFileWriter classes are functor that you need to supply. The library takes care of managing the process and the pipe. Take care with locks in the functors because you can easily encounter deadlocks with the ffmpeg process.

## Motivation

Many such wrappers exist for scripting languages but I could not find one for C or C++. This is probably due to the fact that one can use the ffmpeg libraries instead of having to create of process. But, the ffmpeg executable is very versatile and in many cases creating a process is not a performance consideration.

## Installation

Just clone the project and build it using cmake.

## API Reference

There is only one API class for now: [FFMpegWrapper](ffmpeg_wrapper.h).


## Contributors

Contributions and suggestions are welcome as I do not claim to be have extensive experience integrating ffmpeg in applications.

## License

This project is licensed under the terms of the MIT license.