## Synopsis

A very simple wrapper library for the ffmpeg executable for Linux. It supports feeding a video stream and optionnally an audio stream to ffmpeg and will grab the output stream. The streams are handled through user supplied functors. 

## Code Example

The following excerpt for the supplied [test](test_wrapper.cpp) show the basic flow for using the wrapper:
```c++
  ffmpeg_wrapper::FFMpegWrapper muxer(
      std::make_unique<InputFileReader>(
          input_video_file_path),
      {"-f", "h264", "-r", "25", "-probesize", "1024"},
      std::make_unique<InputFileReader>(
          input_audio_file_path),
      {"-f", "flac"},
      std::make_unique<OutputFileWriter>(
          output_file_path),
      {"-vcodec", "copy", "-f", "mp4", "-reset_timestamps", "1",
          "-movflags", "empty_moov+default_base_moof+frag_keyframe"});
```
The InputFileReader and OutputFileWriter classes are functor that you need to supply. In this example, the wrapper takes a h264 video file with a flac audio file, muxes the video and transcodes the audio to a framented mp4 file using ffmpeg. The library takes care of managing the process and the pipe. Take care with locks in the functors because you can easily encounter deadlocks with the ffmpeg process.

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
