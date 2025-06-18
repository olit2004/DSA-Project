#include "MiniGit.h"
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm> // For std::trim

// Function to trim whitespace from strings
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

void printUsage() {
    std::cout << "MiniGit - A minimal version control system\n\n";
    std::cout << "Usage: minigit <command> [arguments]\n\n";
    std::cout << "Basic commands:\n";
    std::cout << "  init                     Initialize a new repository\n";
    std::cout << "  add <file>               Add file contents to the index\n";
    std::cout << "  commit -m \"<msg>\"        Record changes to the repository\n";
    std::cout << "  log                      Show commit logs\n\n";
    std::cout << "Branching commands:\n";
    std::cout << "  branch <name>            Create a new branch\n";
    std::cout << "  checkout <branch|commit> Switch branches or restore files\n";
    std::cout << "  merge <branch>           Merge another branch into current\n\n";
    std::cout << "Other commands:\n";
    std::cout << "  diff [commit] [commit]   Show changes between commits\n";
    std::cout << "  help                     Show this help message\n";
    std::cout << "  version                  Show version information\n\n";
    std::cout << "Examples:\n";
    std::cout << "  minigit init\n";
    std::cout << "  minigit add README.md\n";
    std::cout << "  minigit commit -m \"Initial commit\"\n";
    std::cout << "  minigit branch new-feature\n";
    std::cout << "  minigit checkout new-feature\n";
}

void printVersion() {
    std::cout << "MiniGit version 1.0.0\n";
}

bool isValidCommitHash(const std::string& hash) {
    // Basic validation - real Git uses 40-character SHA-1 hashes
    return hash.length() == 40 && 
           hash.find_first_not_of("0123456789abcdef") == std::string::npos;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];
    MiniGit minigit;

    try {
        if (command == "help" || command == "--help") {
            printUsage();
        } 
        else if (command == "version" || command == "--version") {
            printVersion();
        }
        else if (command == "init") {
            minigit.init();
        } 
        else if (command == "add") {
            if (argc < 3) {
                std::cerr << "Error: Missing filename for 'add' command\n";
                printUsage();
                return 1;
            }
            minigit.add(argv[2]);
        } 
        else if (command == "commit") {
            if (argc < 4 || std::string(argv[2]) != "-m") {
                std::cerr << "Error: Commit requires a message (-m \"message\")\n";
                printUsage();
                return 1;
            }
            std::string message = trim(argv[3]);
            if (message.empty()) {
                std::cerr << "Error: Commit message cannot be empty\n";
                return 1;
            }
            minigit.commit(message);
        } 
        else if (command == "log") {
            minigit.log();
        } 
        else if (command == "branch") {
            if (argc < 3) {
                std::cerr << "Error: Missing branch name\n";
                printUsage();
                return 1;
            }
            minigit.branch(argv[2]);
        } 
        else if (command == "checkout") {
            if (argc < 3) {
                std::cerr << "Error: Missing branch/commit argument\n";
                printUsage();
                return 1;
            }
            minigit.checkout(argv[2]);
        } 
        else if (command == "merge") {
            if (argc < 3) {
                std::cerr << "Error: Missing branch to merge\n";
                printUsage();
                return 1;
            }
            minigit.merge(argv[2]);
        } 
        else if (command == "diff") {
            std::string commit1, commit2;
            if (argc == 3) commit1 = argv[2];
            else if (argc == 4) {
                commit1 = argv[2];
                commit2 = argv[3];
            } else if (argc > 4) {
                std::cerr << "Error: Too many arguments for diff\n";
                printUsage();
                return 1;
            }
            minigit.diff(commit1, commit2);
        } 
        else {
            std::cerr << "Error: Unknown command '" << command << "'\n";
            printUsage();
            return 1;
        }
    } 
    catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } 
    catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}