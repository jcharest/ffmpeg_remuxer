#include "ffmpeg_wrapper.h"

#include <cstdlib>

#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

namespace bf = boost::filesystem;
namespace bpo = boost::program_options;

namespace {

const std::string OPT_HELP = "help";
const std::string OPT_INPUT_VIDEO_FILE = "input-video-file";
const std::string OPT_INPUT_AUDIO_FILE = "input-audio-file";
const std::string OPT_OUTPUT_FILE = "output-file";

class InputFileReader : public ffmpeg_wrapper::InDataFunctor {
 public:
  InputFileReader(const bf::path & input_file)
      : input_file_(input_file.c_str()) {
  }

  int operator()(uint8_t * buffer, int buffer_size) override {
    input_file_.read(reinterpret_cast<char *>(buffer), buffer_size);

    return input_file_.gcount();
  }

  size_t GetAvailableData() const override {
    auto current_pos = input_file_.tellg();
    input_file_.seekg(std::ios::end);
    auto available_data = input_file_.tellg() - current_pos;
    input_file_.seekg(current_pos, std::ios::beg);

    return available_data;
  }

 private:
  mutable std::ifstream input_file_;
};

class OutputFileWriter : public ffmpeg_wrapper::OutStreamFunctor {
 public:
  OutputFileWriter(const bf::path & output_file)
      : output_file_(output_file.c_str()) {
  }

  void operator()(const uint8_t * buffer, int buffer_size) override {
    output_file_.write(reinterpret_cast<const char *>(buffer), buffer_size);
  }

 private:
  std::ofstream output_file_;
};

}  // namespace

int main(int argc, char * argv[]) {
  bpo::options_description config("Configuration options");
  config.add_options()
      (OPT_HELP.c_str(), "produce help message")
      (OPT_INPUT_VIDEO_FILE.c_str(), bpo::value<std::string>(), "set input video file")
      (OPT_INPUT_AUDIO_FILE.c_str(), bpo::value<std::string>(), "set input audio file")
      (OPT_OUTPUT_FILE.c_str(), bpo::value<std::string>(), "set output video file");

  bpo::variables_map opt_map;
  try {
    bpo::store(bpo::parse_command_line(argc, argv, config), opt_map);
  }
  catch(bpo::unknown_option & e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  bpo::notify(opt_map);

  // Handle help
  if (opt_map.count(OPT_HELP)) {
    std::cerr << config << std::endl;
    return EXIT_SUCCESS;
  }

  // Mandatory options
  std::vector<std::string> mandatory_options = {
      OPT_INPUT_VIDEO_FILE, OPT_INPUT_AUDIO_FILE, OPT_OUTPUT_FILE};
  for(auto & option : mandatory_options) {
    if(!opt_map.count(option)) {
      std::cerr << "Missing mandatory option: " << option << std::endl;
      return EXIT_FAILURE;
    }
  }

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
          "-movflags", "empty_moov+default_base_moof+frag_keyframe"}); // "-loglevel", "quiet"


  std::this_thread::sleep_for(std::chrono::seconds(10000));

  return EXIT_SUCCESS;
}
