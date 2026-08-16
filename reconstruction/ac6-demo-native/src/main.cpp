#include "ac6demo_native/content_store.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

void usage(const char* executable) {
    std::cerr << "usage: " << executable
              << " import <source-directory> [--store <store-directory>]\n"
                 "       " << executable << " verify [--store <store-directory>]\n";
}

bool parse_store_option(int argc, char** argv, int first, std::filesystem::path* store,
                       std::vector<std::string>* positional) {
    for (int index = first; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--store" && index + 1 < argc) {
            if (!store->empty()) {
                return false;
            }
            *store = argv[++index];
            if (store->empty()) {
                return false;
            }
        } else if (argument.rfind("--", 0U) == 0U) {
            return false;
        } else {
            positional->push_back(argument);
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    const std::string command(argv[1]);
    std::filesystem::path store;
    std::vector<std::string> positional;
    if (!parse_store_option(argc, argv, 2, &store, &positional)) {
        usage(argv[0]);
        return 2;
    }
    if (store.empty()) {
        store = ac6demo_native::ContentStore::default_root();
    }

    ac6demo_native::ContentStore content_store(store);
    std::string error;
    if (command == "import") {
        if (positional.size() != 1U ||
            !content_store.import_directory(positional.front(), &error)) {
            if (positional.size() != 1U) {
                usage(argv[0]);
            } else {
                std::cerr << "import rejected: " << error << '\n';
            }
            return 1;
        }
        std::cout << "import verified and published\n";
        return 0;
    }
    if (command == "verify") {
        if (!positional.empty() || !content_store.verify(&error)) {
            if (!positional.empty()) {
                usage(argv[0]);
            } else {
                std::cerr << "verify failed: " << error << '\n';
            }
            return 1;
        }
        std::cout << "verified: ac6-demo-xbox360-pal\n";
        return 0;
    }

    usage(argv[0]);
    return 2;
}
