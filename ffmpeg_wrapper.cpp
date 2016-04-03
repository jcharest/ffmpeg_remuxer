#include "ffmpeg_wrapper.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <thread>

#include <boost/filesystem.hpp>

namespace bf = boost::filesystem;

namespace {

const int VIDEO_BUFFER_SIZE = 1024;
const int AUDIO_BUFFER_SIZE = 512;
const int OUTPUT_BUFFER_SIZE = 1024;

class FFMpegProcess {
 public:
  FFMpegProcess(bool has_audio_input);
  ~FFMpegProcess();

  void Start(const std::vector<std::string> & video_args,
             const std::vector<std::string> & audio_args,
             const std::vector<std::string> & output_args);
  template<size_t size>
  bool WriteVideo(const std::array<uint8_t, size> & buffer, size_t write_size);
  template<size_t size>
  bool WriteAudio(const std::array<uint8_t, size> & buffer, size_t write_size);
  template<size_t size>
  size_t ReadOutput(std::array<uint8_t, size> & buffer, int timeout);

 private:
  void RemovePipes();

  const bf::path video_pipe_path_;
  const bf::path audio_pipe_path_;
  const bf::path output_pipe_path_;
  int video_pipe_;
  int audio_pipe_;
  int output_pipe_;
  int pid_;
};

}  // namespace

namespace ffmpeg_wrapper {

FFMpegWrapperException::FFMpegWrapperException(const std::string & what)
    : what_("FFMpegRemuxerException: " + what) {
}

const char* FFMpegWrapperException::what() const noexcept {
  return what_.c_str();
}

class FFMpegWrapper::Impl {
 public:
  Impl(std::unique_ptr<InDataFunctor> && video_func,
       const std::vector<std::string> & video_args,
       std::unique_ptr<InDataFunctor> && audio_func,
       const std::vector<std::string> & audio_args,
       std::unique_ptr<OutStreamFunctor> && output_func,
       const std::vector<std::string> & output_args)
     : process_(audio_func != nullptr),
       video_func_(move(video_func)), audio_func_(move(audio_func)),
       output_func_(move(output_func)), start_(false), stop_(false),
       video_input_thread_(&Impl::VideoInputThreadRun, this),
       audio_input_thread_(&Impl::AudioInputThreadRun, this),
       output_thread_(&Impl::OutputThreadRun, this) {

    process_.Start(video_args, audio_args, output_args);
    start_ = true;
  }

  ~Impl() {
    stop_ = true;
    video_input_thread_.join();
    audio_input_thread_.join();
    output_thread_.join();
  }

 private:
  void VideoInputThreadRun() {
    while(!start_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    bool read_pending = true;
    size_t current_size = 0;
    std::array<uint8_t, VIDEO_BUFFER_SIZE> buffer;
    while(!stop_) {
      if(read_pending) {
        current_size = (*video_func_)(buffer.data(), buffer.size());
      }
      read_pending = process_.WriteVideo(buffer, current_size);
    }
  }

  void AudioInputThreadRun() {
    if(audio_func_ == nullptr) {
      return;
    }

    while(!start_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    bool read_pending = true;
    size_t current_size = 0;
    std::array<uint8_t, AUDIO_BUFFER_SIZE> buffer;
    while(!stop_) {
      if(read_pending) {
        current_size = (*audio_func_)(buffer.data(), buffer.size());
      }
      read_pending = process_.WriteAudio(buffer, current_size);
    }
  }

  void OutputThreadRun() {
    while(!start_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::array<uint8_t, OUTPUT_BUFFER_SIZE> buffer;
    while(!stop_) {
      auto size = process_.ReadOutput(buffer, 100);
      (*output_func_)(buffer.data(), size);
    }
  }

  FFMpegProcess process_;
  std::unique_ptr<InDataFunctor> video_func_;
  std::unique_ptr<InDataFunctor> audio_func_;
  std::unique_ptr<OutStreamFunctor> output_func_;

  std::atomic_bool start_;
  std::atomic_bool stop_;
  std::thread video_input_thread_;
  std::thread audio_input_thread_;
  std::thread output_thread_;
};

FFMpegWrapper::FFMpegWrapper(
    std::unique_ptr<InDataFunctor> && video_func,
    const std::vector<std::string> & video_args,
    std::unique_ptr<InDataFunctor> && audio_func,
    const std::vector<std::string> & audio_args,
    std::unique_ptr<OutStreamFunctor> && output_func,
    const std::vector<std::string> & output_args)
    : impl_(std::make_unique<Impl>(move(video_func), video_args,
                                   move(audio_func), audio_args,
                                   move(output_func), output_args)) {
}

FFMpegWrapper::~FFMpegWrapper() {

}

}  // namespace ffmpeg_wrapper

namespace {

FFMpegProcess::FFMpegProcess(bool has_audio_input)
    : video_pipe_path_(bf::temp_directory_path() / bf::unique_path()),
      audio_pipe_path_(has_audio_input ?
          bf::temp_directory_path() / bf::unique_path() : ""),
      output_pipe_path_(bf::temp_directory_path() / bf::unique_path()),
      video_pipe_(-1), audio_pipe_(-1), output_pipe_(-1), pid_(0) {

  try {
    // Create pipes
    auto ret = mkfifo(video_pipe_path_.c_str(),
                      S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if(ret != 0) {
      throw ffmpeg_wrapper::FFMpegWrapperException(
          std::string("Failed to create video pipe: ") + strerror(errno));
    }
    if(!audio_pipe_path_.empty()) {
      auto ret = mkfifo(audio_pipe_path_.c_str(),
                        S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
      if(ret != 0) {
        throw ffmpeg_wrapper::FFMpegWrapperException(
            std::string("Failed to create audio pipe: ") + strerror(errno));
      }
    }
    ret = mkfifo(output_pipe_path_.c_str(),
                      S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if(ret != 0) {
      throw ffmpeg_wrapper::FFMpegWrapperException(
          std::string("Failed to create output pipe: ") + strerror(errno));
    }
  }
  catch(ffmpeg_wrapper::FFMpegWrapperException & ex) {
    RemovePipes();
  }
}

FFMpegProcess::~FFMpegProcess() {
  if(pid_) {
    kill(pid_, SIGTERM);
    waitpid(pid_, nullptr, WNOHANG);
  }

  if(video_pipe_ >= 0) {
    close(video_pipe_);
  }
  if(audio_pipe_ >= 0) {
    close(audio_pipe_);
  }
  if(output_pipe_  >= 0) {
    close(output_pipe_);
  }

  RemovePipes();
}

void FFMpegProcess::Start(const std::vector<std::string> & video_args,
                          const std::vector<std::string> & audio_args,
                          const std::vector<std::string> & output_args) {
  int pid = fork();
  if(!pid) {
    std::vector<std::string> args = {"ffmpeg", "-y"};
    args.insert(args.end(), video_args.begin(), video_args.end());
    args.push_back("-i");
    args.push_back(video_pipe_path_.native());
    if(!audio_pipe_path_.empty()) {
      args.insert(args.end(), audio_args.begin(), audio_args.end());
      args.push_back("-i");
      args.push_back(audio_pipe_path_.native());
    }
    args.insert(args.end(), output_args.begin(), output_args.end());
    args.push_back(output_pipe_path_.native());

    std::unique_ptr<const char * []> argv(new const char *[args.size() + 1]);
    std::transform(args.begin(), args.end(), argv.get(),
                   [](const std::string & arg){return arg.c_str();});
    argv[args.size()] = nullptr;

    // Child process
    execvp(argv[0], const_cast<char * const *>(argv.get()));
  }
  else {
    // Parent process
    pid_ = pid;
  }
}

template<size_t size>
bool FFMpegProcess::WriteVideo(const std::array<uint8_t, size> & buffer,
                               size_t write_size) {
  // Try to open pipe
  if(video_pipe_ < 0) {
    video_pipe_ = open(video_pipe_path_.c_str(), O_WRONLY | O_NONBLOCK);
    if(video_pipe_ < 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      return false;
    }

    // Set pipe back to blocking
    auto flags = fcntl(video_pipe_, F_GETFL, 0);
    fcntl(video_pipe_, F_SETFL, flags & ~O_NONBLOCK);
  }

  auto ret = write(video_pipe_, buffer.data(), write_size);
  if(ret < 0) {
    ffmpeg_wrapper::FFMpegWrapperException(
        std::string("Failed to write to video pipe: ") + strerror(errno));
  }

  return true;
}

template<size_t size>
bool FFMpegProcess::WriteAudio(const std::array<uint8_t, size> & buffer,
                               size_t write_size) {
  // Try to open pipe
  if(audio_pipe_ < 0) {
    audio_pipe_ = open(audio_pipe_path_.c_str(), O_WRONLY | O_NONBLOCK);
    if(audio_pipe_ < 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      return false;
    }

    // Set pipe back to blocking
    auto flags = fcntl(audio_pipe_, F_GETFL, 0);
    fcntl(audio_pipe_, F_SETFL, flags & ~O_NONBLOCK);
  }

  auto ret = write(audio_pipe_, buffer.data(), write_size);
  if(ret < 0) {
    ffmpeg_wrapper::FFMpegWrapperException(
        std::string("Failed to write to audio pipe: ") + strerror(errno));
  }

  return true;
}

template<size_t size>
size_t FFMpegProcess::ReadOutput(std::array<uint8_t, size> & buffer,
                                  int timeout) {
  // Try to open pipe
  if(output_pipe_ < 0) {
    output_pipe_ = open(output_pipe_path_.c_str(), O_RDONLY | O_NONBLOCK);
    if(output_pipe_ < 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      return 0;
    }

    // Set pipe back to blocking
    auto flags = fcntl(output_pipe_, F_GETFL, 0);
    fcntl(output_pipe_, F_SETFL, flags & ~O_NONBLOCK);
  }

  struct pollfd output_pipe_poll = {output_pipe_, POLLIN, 0};
  auto ret = poll(&output_pipe_poll, 1, timeout);
  if(ret < 0) {
    throw ffmpeg_wrapper::FFMpegWrapperException(
              std::string("Failed to to poll output pipe: ") + strerror(errno));
  } else if (ret == 0) {
    return 0;
  }

  auto read_size = read(output_pipe_, buffer.data(), size);
  if(read_size < 0) {
    ffmpeg_wrapper::FFMpegWrapperException(
        std::string("Failed to read from output pipe: ") + strerror(errno));
  }

  return read_size;
}

void FFMpegProcess::RemovePipes() {
  boost::system::error_code ec;
  if(bf::exists(video_pipe_path_, ec)) {
    bf::remove(video_pipe_path_, ec);
  }
  if(bf::exists(audio_pipe_path_, ec)) {
    bf::remove(audio_pipe_path_, ec);
  }
  if(bf::exists(output_pipe_path_, ec)) {
    bf::remove(output_pipe_path_, ec);
  }
}

}  // namespace
