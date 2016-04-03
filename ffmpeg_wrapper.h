#ifndef FFMPEG_WRAPPER_H_
#define FFMPEG_WRAPPER_H_

#include <cstdint>

#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace ffmpeg_wrapper {

class FFMpegWrapperException : public std::exception {
 public:
  explicit FFMpegWrapperException(const std::string & what);

  const char* what() const noexcept override;

 private:
  std::string what_;
};

class InDataFunctor {
 public:
  InDataFunctor() = default;
  virtual ~InDataFunctor() = default;

  virtual int operator()(uint8_t * buffer, int buffer_size) = 0;
  virtual size_t GetAvailableData() const = 0;
};

class OutStreamFunctor {
 public:
  OutStreamFunctor() = default;
  virtual ~OutStreamFunctor() = default;

  virtual void operator()(const uint8_t * buffer, int buffer_size) = 0;
};

class FFMpegWrapper {
 public:
  FFMpegWrapper(std::unique_ptr<InDataFunctor> && video_func,
                const std::vector<std::string> & video_args,
                std::unique_ptr<InDataFunctor> && audio_func,
                const std::vector<std::string> & audio_args,
                std::unique_ptr<OutStreamFunctor> && output_func,
                const std::vector<std::string> & output_args);
  ~FFMpegWrapper();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;

  FFMpegWrapper(const FFMpegWrapper &) = delete;
  FFMpegWrapper(FFMpegWrapper &&) = delete;
  FFMpegWrapper & operator=(const FFMpegWrapper &) = delete;
  FFMpegWrapper & operator=(FFMpegWrapper &&) = delete;
};

}  // namespace ffmpeg_wrapper

#endif  // FFMPEG_WRAPPER_H_
