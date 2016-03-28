#include <atomic>
#include <iostream>
#include <boost/timer/timer.hpp>
#include <boost/program_options.hpp>
#include <opencv2/opencv.hpp>
#include "picpac.h"

using namespace std;
using namespace boost;
namespace fs = boost::filesystem;

// Configurations
// 0. All categories into a single database
// 1. All categories into separate databases
// 2. Category 0 into a database,
//    all other categories into a single database

// paths under a directory, following symlinks
class Paths: public vector<fs::path> {
public:
    Paths () {}
    Paths (fs::path const &path) {
        fs::recursive_directory_iterator it(fs::path(path), fs::symlink_option::recurse), end;
        for (; it != end; ++it) {
            if (it->status().type() == fs::regular_file) {
                push_back(it->path());
            }
        }
        CHECK(size() >= 10) << "Need at least 10 files to train: " << path;
        random_shuffle(this->begin(), this->end());
    }
};

class Samples: public vector<Paths> {
public:
    Samples () {}
    Samples (fs::path const &root) {
        fs::directory_iterator it(root), end;
        vector<unsigned> cats;
        for (; it != end; ++it) {
            if (!fs::is_directory(it->path())) {
                LOG(ERROR) << "Not a directory: " << it->path();
                continue;
            }
            fs::path name = it->path().filename();
            try {
                unsigned c = lexical_cast<unsigned>(name.native());
                cats.push_back(c);
            }
            catch (...) {
                LOG(ERROR) << "Category directory not properly named: " << it->path();
            }
        }
        sort(cats.begin(), cats.end());
        cats.resize(unique(cats.begin(), cats.end()) - cats.begin());
        CHECK(cats.size() >= 2) << "Need at least 2 categories to train.";
        CHECK((cats.front() == 0)
                && (cats.back() == cats.size() -1 ))
            << "Subdirectories must be consecutively named from 0 to N-1.";
        for (unsigned c = 0; c < cats.size(); ++c) {
            emplace_back(root / fs::path(lexical_cast<string>(c)));
            LOG(INFO) << "Loaded " << back().size() << " paths for category " << c << ".";
        }
    }
};

int main (int argc, char *argv[]) {
    FLAGS_minloglevel=1;
    google::InitGoogleLogging(argv[0]);
    namespace po = boost::program_options; 
    fs::path in_dir;
    fs::path out_path;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("input", po::value(&in_dir), "input directory")
    ("output", po::value(&out_path), "output directory")
    ;   
    
    po::positional_options_description p;
    p.add("input", 1);
    p.add("output", 1); 

    po::variables_map vm; 
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help") 
            || vm.count("input") == 0
            || vm.count("output") == 0) {
        cerr << desc;
        return 1;
    }

    Samples all(in_dir);

    picpac::FileWriter dataset(out_path);

    for (unsigned i = 0; i < all.size(); ++i) {
        for (auto const &path: all[i]) {
            cv::Mat image = cv::imread(path.native());
            if (!image.data) {
                LOG(ERROR) << "not a image: " << path;
                continue;
            }
            picpac::Record rec;
            rec.pack(i, path);
            dataset.append(rec);
        }
    }
}
