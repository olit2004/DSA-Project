#ifndef MINIGIT_H
#define MINIGIT_H

#include <string>
#include <vector>
#include <unordered_map>
#include "Commit.h"

class MiniGit {
public:
    MiniGit();
    ~MiniGit();

    // Core commands
    void init();
    void add(const std::string& filename);
    void commit(const std::string& message);
    void log();
    void branch(const std::string& branchName);
    void checkout(const std::string& target);
    void merge(const std::string& branchName);
    void diff(const std::string& commit1_hash = "", const std::string& commit2_hash = "");

private:
    std::unordered_map<std::string, std::string> stagingArea;

    // Helper methods
    std::string getHeadCommitHash();
    void updateHead(const std::string& commitHash, bool isBranch = false, const std::string& branchName = "");
    CommitNode loadCommit(const std::string& commitHash);
    void loadIndex();
    void saveIndex();

    // Merge & Diff helpers
    std::string findLCA(const std::string& commitHash1, const std::string& commitHash2);
    std::string getBlobContent(const std::string& blobHash);
    std::string getWorkingDirectoryFileContent(const std::string& filename);
    void printDiff(const std::string& oldContent, const std::string& newContent, const std::string& filename = "");
};

#endif // MINIGIT_H