// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include <aliceVision/matching/IndMatch.hpp>
#include <aliceVision/matching/io.hpp>
#include <aliceVision/feature/svgVisualization.hpp>
#include <aliceVision/image/all.hpp>
#include <aliceVision/sfm/sfm.hpp>
#include <aliceVision/sfm/pipeline/regionsIO.hpp>
#include <aliceVision/system/Logger.hpp>
#include <aliceVision/system/cmdline.hpp>

#include <dependencies/vectorGraphics/svgDrawer.hpp>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/progress.hpp>

#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <map>

using namespace aliceVision;
using namespace aliceVision::feature;
using namespace aliceVision::matching;
using namespace aliceVision::sfm;
using namespace svg;

namespace po = boost::program_options;
namespace fs = boost::filesystem;

int main(int argc, char ** argv)
{
  // command-line parameters

  std::string verboseLevel = system::EVerboseLevel_enumToString(system::Logger::getDefaultVerboseLevel());
  std::string sfmDataFilename;
  std::string outputFolder;
  std::string featuresFolder;
  std::string describerTypesName = feature::EImageDescriberType_enumToString(feature::EImageDescriberType::SIFT);

  po::options_description allParams("AliceVision exportKeypoints");

  po::options_description requiredParams("Required parameters");
  requiredParams.add_options()
    ("input,i", po::value<std::string>(&sfmDataFilename)->required(),
      "SfMData file.")
    ("output,o", po::value<std::string>(&outputFolder)->required(),
      "Output path for keypoints.")
    ("featuresFolder,f", po::value<std::string>(&featuresFolder)->required(),
      "Path to a folder containing the extracted features.");

  po::options_description optionalParams("Optional parameters");
  optionalParams.add_options()
    ("describerTypes,d", po::value<std::string>(&describerTypesName)->default_value(describerTypesName),
      feature::EImageDescriberType_informations().c_str());

  po::options_description logParams("Log parameters");
  logParams.add_options()
    ("verboseLevel,v", po::value<std::string>(&verboseLevel)->default_value(verboseLevel),
      "verbosity level (fatal,  error, warning, info, debug, trace).");

  allParams.add(requiredParams).add(optionalParams).add(logParams);

  po::variables_map vm;
  try
  {
    po::store(po::parse_command_line(argc, argv, allParams), vm);

    if(vm.count("help") || (argc == 1))
    {
      ALICEVISION_COUT(allParams);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch(boost::program_options::required_option& e)
  {
    ALICEVISION_CERR("ERROR: " << e.what());
    ALICEVISION_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }
  catch(boost::program_options::error& e)
  {
    ALICEVISION_CERR("ERROR: " << e.what());
    ALICEVISION_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }

  ALICEVISION_COUT("Program called with the following parameters:");
  ALICEVISION_COUT(vm);

  // set verbose level
  system::Logger::get()->setLogLevel(verboseLevel);

  if (outputFolder.empty())
  {
    ALICEVISION_LOG_ERROR("Invalid output folder");
    return EXIT_FAILURE;
  }

  // read SfM Scene (image view names)

  SfMData sfmData;
  if (!sfm::Load(sfmData, sfmDataFilename, sfm::ESfMData(VIEWS|INTRINSICS))) {
    ALICEVISION_LOG_ERROR("The input SfMData file '"<< sfmDataFilename << "' cannot be read.");
    return EXIT_FAILURE;
  }

  // load SfM Scene regions
  
  // get imageDescriberMethodType
  std::vector<EImageDescriberType> describerMethodTypes = EImageDescriberType_stringToEnums(describerTypesName);

  // read the features
  std::vector<std::string> featuresFolders = sfmData.getFeaturesFolders();
  featuresFolders.emplace_back(featuresFolder);

  feature::FeaturesPerView featuresPerView;
  if (!sfm::loadFeaturesPerView(featuresPerView, sfmData, featuresFolders, describerMethodTypes)) {
    ALICEVISION_LOG_ERROR("Invalid features");
    return EXIT_FAILURE;
  }

  // for each image, export visually the keypoints

  fs::create_directory(outputFolder);
  ALICEVISION_LOG_INFO("Export extracted keypoints for all images");
  boost::progress_display myProgressBar(sfmData.views.size());
  for(const auto &iterViews : sfmData.views)
  {
    const View * view = iterViews.second.get();
    const std::string viewImagePath = view->getImagePath();

    const std::pair<size_t, size_t>
      dimImage = std::make_pair(view->getWidth(), view->getHeight());

    const MapFeaturesPerDesc& features = featuresPerView.getData().at(view->getViewId());

    // output filename
    fs::path outputFilename = fs::path(outputFolder) / std::string(std::to_string(view->getViewId()) + "_" + std::to_string(features.size()) + ".svg");

    feature::saveFeatures2SVG(viewImagePath,
                               dimImage,
                               featuresPerView.getData().at(view->getViewId()),
                               outputFilename.string());

    ++myProgressBar;
  }
  
  return EXIT_SUCCESS;
}