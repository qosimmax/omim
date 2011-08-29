#include "api.hpp"

#include "../../indexer/classificator_loader.hpp"

#include "../../platform/platform.hpp"

#include "../../3party/gflags/src/gflags/gflags.h"


DEFINE_string(input, "", "MWM file name in the data directory");
DEFINE_int32(count, 3, "How many times to run benchmark");
DEFINE_int32(lowS, 10, "Low processing scale");
DEFINE_int32(highS, 17, "High processing scale");


int main(int argc, char ** argv)
{
  Platform & pl = GetPlatform();
  classificator::Read(pl.GetReader("drawing_rules.bin"),
                      pl.GetReader("classificator.txt"),
                      pl.GetReader("visibility.txt"),
                      pl.GetReader("types.txt"));

  google::SetUsageMessage("MWM benchmarking tool");
  if (argc < 2)
  {
    google::ShowUsageWithFlagsRestrict(argv[0], "main");
    return 0;
  }

  google::ParseCommandLineFlags(&argc, &argv, false);

  if (!FLAGS_input.empty())
  {
    RunFeaturesLoadingBenchmark(FLAGS_input, FLAGS_count,
                                make_pair(FLAGS_lowS, FLAGS_highS));
  }

  return 0;
}
