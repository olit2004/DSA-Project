#include "MiniGit.h"
#include "FileUtils.h"
#include "Hashing.h"
#include "Commit.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <set>
#include <queue>
#include <map>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

// Repository structure constants
const string MINIGIT_DIR = ".minigit";
const string OBJECTS_DIR = MINIGIT_DIR + "/objects";
const string REFS_DIR = MINIGIT_DIR + "/refs";
const string HEAD_FILE = REFS_DIR + "/HEAD";
const string HEADS_DIR = REFS_DIR + "/heads";
const string INDEX_FILE = MINIGIT_DIR + "/index";

// Constructor/Destructor
MiniGit::MiniGit() {
    if (FileUtils::directoryExists(MINIGIT_DIR)) {
        loadIndex();
    }
}

MiniGit::~MiniGit() {}

// ==================== CORE COMMANDS ====================

void MiniGit::init() {
    if (FileUtils::directoryExists(MINIGIT_DIR)) {
        cout << "MiniGit repository already initialized\n";
        return;
    }

    if (!FileUtils::createDirectory(MINIGIT_DIR) ||
        !FileUtils::createDirectory(OBJECTS_DIR) ||
        !FileUtils::createDirectory(REFS_DIR) ||
        !FileUtils::createDirectory(HEADS_DIR)) {
        throw runtime_error("Failed to create repository structure");
    }

    if (!FileUtils::writeToFile(HEAD_FILE, "ref: refs/heads/master") ||
        !FileUtils::writeToFile(HEADS_DIR + "/master", "")) {
        throw runtime_error("Failed to initialize HEAD");
    }

    cout << "Initialized empty MiniGit repository\n";
}

void MiniGit::add(const string& filename) {
    if (!FileUtils::fileExists(filename)) {
        throw runtime_error("File not found: " + filename);
    }

    string content;
    if (!FileUtils::readFromFile(filename, content)) {
        throw runtime_error("Failed to read file: " + filename);
    }

    string hash = Hashing::calculateHash(content);
    string blobPath = OBJECTS_DIR + "/" + hash;

    if (!FileUtils::fileExists(blobPath)) {
        if (!FileUtils::writeToFile(blobPath, content)) {
            throw runtime_error("Failed to store blob");
        }
    }

    stagingArea[filename] = hash;
    saveIndex();
    cout << "Added " << filename << " to staging area\n";
}

void MiniGit::commit(const string& message) {
    if (stagingArea.empty()) {
        cout << "Nothing to commit\n";
        return;
    }

    string parentHash = getHeadCommitHash();
    vector<string> parents;
    if (!parentHash.empty()) parents.push_back(parentHash);

    CommitNode commit(message, parents, stagingArea);
    string commitPath = OBJECTS_DIR + "/" + commit.getHash();

    if (!FileUtils::writeToFile(commitPath, commit.serialize())) {
        throw runtime_error("Failed to store commit");
    }

    updateHead(commit.getHash(), true, "master");
    stagingArea.clear();
    saveIndex();

    cout << "Committed " << commit.getHash().substr(0, 7) << ": " << message << "\n";
}

void MiniGit::log() {
    string currentHash = getHeadCommitHash();
    if (currentHash.empty()) {
        cout << "No commits yet\n";
        return;
    }

    while (!currentHash.empty()) {
        CommitNode commit = loadCommit(currentHash);
        cout << "commit " << commit.getHash() << "\n";
        cout << "Date: " << commit.getTimestamp() << "\n";
        cout << "\n    " << commit.getMessage() << "\n\n";

        if (commit.getParentHashes().empty()) break;
        currentHash = commit.getParentHashes()[0];
    }
}

// ==================== BRANCHING ====================

void MiniGit::branch(const string& branchName) {
    string currentHash = getHeadCommitHash();
    if (currentHash.empty()) {
        throw runtime_error("No commits exist yet");
    }

    string branchPath = HEADS_DIR + "/" + branchName;
    if (FileUtils::fileExists(branchPath)) {
        cout << "Branch already exists: " << branchName << "\n";
        return;
    }

    if (!FileUtils::writeToFile(branchPath, currentHash)) {
        throw runtime_error("Failed to create branch");
    }
    cout << "Created branch " << branchName << "\n";
}

void MiniGit::checkout(const string& target) {
    string targetHash;
    bool isBranch = false;
    string branchPath = HEADS_DIR + "/" + target;

    if (FileUtils::fileExists(branchPath)) {
        if (!FileUtils::readFromFile(branchPath, targetHash)) {
            throw runtime_error("Failed to read branch");
        }
        isBranch = true;
    } else {
        try {
            loadCommit(target);
            targetHash = target;
        } catch (...) {
            throw runtime_error("Invalid branch or commit: " + target);
        }
    }

    CommitNode commit = loadCommit(targetHash);
    for (const auto& [file, hash] : commit.getFileBlobs()) {
        string content = getBlobContent(hash);
        FileUtils::writeToFile(file, content);
    }

    updateHead(targetHash, isBranch, isBranch ? target : "");
    stagingArea.clear();
    cout << "Switched to " << (isBranch ? "branch " + target : "commit " + targetHash.substr(0, 7)) << "\n";
}

// ==================== MERGE ====================

void MiniGit::merge(const string& branchName) {
    string currentHash = getHeadCommitHash();
    if (currentHash.empty()) {
        throw runtime_error("No commits to merge from");
    }

    string branchPath = HEADS_DIR + "/" + branchName;
    string targetHash;
    if (!FileUtils::readFromFile(branchPath, targetHash)) {
        throw runtime_error("Branch not found: " + branchName);
    }

    if (currentHash == targetHash) {
        cout << "Already up to date\n";
        return;
    }

    string lcaHash = findLCA(currentHash, targetHash);
    cout << "Merging branch '" << branchName << "' (" << targetHash.substr(0,7) 
         << ") into current branch (" << currentHash.substr(0,7) << ")\n";

    CommitNode currentCommit = loadCommit(currentHash);
    CommitNode targetCommit = loadCommit(targetHash);
    CommitNode lcaCommit = lcaHash.empty() ? CommitNode() : loadCommit(lcaHash);

    bool conflictsExist = false;
    unordered_map<string, string> mergedFiles = currentCommit.getFileBlobs();
    const auto& lcaFiles = lcaCommit.getFileBlobs();
    const auto& targetFiles = targetCommit.getFileBlobs();

    set<string> allFiles;
    for (const auto& pair : mergedFiles) allFiles.insert(pair.first);
    for (const auto& pair : targetFiles) allFiles.insert(pair.first);
    for (const auto& pair : lcaFiles) allFiles.insert(pair.first);

    for (const string& file : allFiles) {
        string lcaBlob = lcaFiles.count(file) ? lcaFiles.at(file) : "";
        string currentBlob = mergedFiles.count(file) ? mergedFiles.at(file) : "";
        string targetBlob = targetFiles.count(file) ? targetFiles.at(file) : "";

        if (lcaBlob.empty() && currentBlob.empty()) {
            cout << "Taking new file from branch '" << branchName << "': " << file << "\n";
            mergedFiles[file] = targetBlob;
            FileUtils::writeToFile(file, getBlobContent(targetBlob));
            continue;
        }

        if (!lcaBlob.empty() && currentBlob == lcaBlob && targetBlob != lcaBlob) {
            cout << "Taking changes from branch '" << branchName << "' for: " << file << "\n";
            mergedFiles[file] = targetBlob;
            FileUtils::writeToFile(file, getBlobContent(targetBlob));
            continue;
        }

        if (!lcaBlob.empty() && currentBlob != lcaBlob && targetBlob != lcaBlob && currentBlob != targetBlob) {
            cout << "CONFLICT (content): " << file << " modified in both branches\n";
            string currentContent = getBlobContent(currentBlob);
            string targetContent = getBlobContent(targetBlob);
            string conflictContent = "<<<<<<< HEAD\n" + currentContent +
                                   "=======\n" + targetContent +
                                   ">>>>>>> " + branchName + "\n";
            FileUtils::writeToFile(file, conflictContent);
            conflictsExist = true;
            continue;
        }

        if (!lcaBlob.empty() && !currentBlob.empty() && targetBlob.empty()) {
            if (lcaBlob == currentBlob) {
                cout << "Removing file deleted in branch '" << branchName << "': " << file << "\n";
                mergedFiles.erase(file);
                fs::remove(file);
            } else {
                cout << "CONFLICT (delete/modify): " << file << " was deleted in branch '" 
                     << branchName << "' but modified in current branch\n";
                conflictsExist = true;
            }
        }
    }

    if (conflictsExist) {
        cout << "Merge conflicts detected. Resolve them and commit the result.\n";
        return;
    }

    vector<string> parents = {currentHash, targetHash};
    CommitNode mergeCommit("Merge branch '" + branchName + "'", parents, mergedFiles);
    string commitPath = OBJECTS_DIR + "/" + mergeCommit.getHash();

    if (!FileUtils::writeToFile(commitPath, mergeCommit.serialize())) {
        throw runtime_error("Failed to create merge commit");
    }

    updateHead(mergeCommit.getHash(), true, "master");
    cout << "Merge successful. New commit: " << mergeCommit.getHash().substr(0,7) << "\n";
}

// ==================== DIFF ====================

void MiniGit::diff(const string& commit1_hash, const string& commit2_hash) {
    bool compareWD = commit2_hash.empty();
    string hash1 = commit1_hash.empty() ? getHeadCommitHash() : commit1_hash;
    
    if (hash1.empty()) {
        cout << "No commits to compare\n";
        return;
    }

    CommitNode commit1 = loadCommit(hash1);
    const auto& files1 = commit1.getFileBlobs();

    unordered_map<string, string> files2;
    if (compareWD) {
        for (const auto& entry : fs::recursive_directory_iterator(fs::current_path())) {
            if (entry.is_regular_file()) {
                string file = fs::relative(entry.path()).string();
                if (file.find(MINIGIT_DIR) != 0) {
                    string content;
                    FileUtils::readFromFile(file, content);
                    files2[file] = Hashing::calculateHash(content);
                }
            }
        }
        cout << "Comparing working directory against commit " << hash1.substr(0,7) << ":\n";
    } else {
        CommitNode commit2 = loadCommit(commit2_hash);
        files2 = commit2.getFileBlobs();
        cout << "Comparing commit " << hash1.substr(0,7) << " with " << commit2_hash.substr(0,7) << ":\n";
    }

    set<string> allFiles;
    for (const auto& pair : files1) allFiles.insert(pair.first);
    for (const auto& pair : files2) allFiles.insert(pair.first);

    for (const string& file : allFiles) {
        bool in1 = files1.count(file);
        bool in2 = files2.count(file);
        
        if (!in1 && in2) {
            cout << "+++ Added: " << file << "\n";
            string content = compareWD ? getWorkingDirectoryFileContent(file) : getBlobContent(files2.at(file));
            printDiff("", content, file);
        } 
        else if (in1 && !in2) {
            cout << "--- Removed: " << file << "\n";
            string content = getBlobContent(files1.at(file));
            printDiff(content, "", file);
        }
        else if (files1.at(file) != files2.at(file)) {
            cout << "*** Modified: " << file << "\n";
            string oldContent = getBlobContent(files1.at(file));
            string newContent = compareWD ? getWorkingDirectoryFileContent(file) : getBlobContent(files2.at(file));
            printDiff(oldContent, newContent, file);
        }
    }
}

// ==================== HELPER METHODS ====================

string MiniGit::getHeadCommitHash() {
    string headContent;
    if (!FileUtils::readFromFile(HEAD_FILE, headContent)) {
        return "";
    }

    if (headContent.rfind("ref: ", 0) == 0) {
        string refPath = headContent.substr(5);
        string refFilePath = MINIGIT_DIR + "/" + refPath;
        string commitHash;
        FileUtils::readFromFile(refFilePath, commitHash);
        return commitHash;
    }
    return headContent;
}

void MiniGit::updateHead(const string& commitHash, bool isBranch, const string& branchName) {
    if (isBranch) {
        string branchPath = HEADS_DIR + "/" + branchName;
        if (!FileUtils::writeToFile(branchPath, commitHash)) {
            throw runtime_error("Failed to update branch");
        }
        if (!FileUtils::writeToFile(HEAD_FILE, "ref: refs/heads/" + branchName)) {
            throw runtime_error("Failed to update HEAD");
        }
    } else {
        if (!FileUtils::writeToFile(HEAD_FILE, commitHash)) {
            throw runtime_error("Failed to update HEAD");
        }
    }
}

CommitNode MiniGit::loadCommit(const string& commitHash) {
    string content;
    if (!FileUtils::readFromFile(OBJECTS_DIR + "/" + commitHash, content)) {
        throw runtime_error("Commit not found: " + commitHash);
    }
    return CommitNode::deserialize(content);
}

void MiniGit::loadIndex() {
    stagingArea.clear();
    string content;
    if (FileUtils::readFromFile(INDEX_FILE, content)) {
        stringstream ss(content);
        string line;
        while (getline(ss, line)) {
            size_t space = line.find(' ');
            if (space != string::npos) {
                string file = line.substr(0, space);
                string hash = line.substr(space + 1);
                stagingArea[file] = hash;
            }
        }
    }
}

void MiniGit::saveIndex() {
    stringstream ss;
    for (const auto& [file, hash] : stagingArea) {
        ss << file << " " << hash << "\n";
    }
    if (!FileUtils::writeToFile(INDEX_FILE, ss.str())) {
        throw runtime_error("Failed to save index");
    }
}

string MiniGit::findLCA(const string& commitHash1, const string& commitHash2) {
    map<string, int> visited1, visited2;
    queue<pair<string, int>> q1, q2;

    q1.push({commitHash1, 0});
    visited1[commitHash1] = 0;
    q2.push({commitHash2, 0});
    visited2[commitHash2] = 0;

    while (!q1.empty() || !q2.empty()) {
        if (!q1.empty()) {
            auto [current, depth] = q1.front(); q1.pop();
            if (visited2.count(current)) return current;

            try {
                CommitNode commit = loadCommit(current);
                for (const string& parent : commit.getParentHashes()) {
                    if (!visited1.count(parent)) {
                        visited1[parent] = depth + 1;
                        q1.push({parent, depth + 1});
                    }
                }
            } catch (...) {}
        }

        if (!q2.empty()) {
            auto [current, depth] = q2.front(); q2.pop();
            if (visited1.count(current)) return current;

            try {
                CommitNode commit = loadCommit(current);
                for (const string& parent : commit.getParentHashes()) {
                    if (!visited2.count(parent)) {
                        visited2[parent] = depth + 1;
                        q2.push({parent, depth + 1});
                    }
                }
            } catch (...) {}
        }
    }

    return "";
}

string MiniGit::getBlobContent(const string& blobHash) {
    string content;
    if (!FileUtils::readFromFile(OBJECTS_DIR + "/" + blobHash, content)) {
        throw runtime_error("Blob not found: " + blobHash);
    }
    return content;
}

string MiniGit::getWorkingDirectoryFileContent(const string& filename) {
    string content;
    FileUtils::readFromFile(filename, content);
    return content;
}

void MiniGit::printDiff(const string& oldContent, const string& newContent, const string& filename) {
    vector<string> oldLines, newLines;
    string line;
    
    stringstream oldSS(oldContent);
    while (getline(oldSS, line)) oldLines.push_back(line);
    
    stringstream newSS(newContent);
    while (getline(newSS, line)) newLines.push_back(line);

    if (!filename.empty()) {
        cout << "--- a/" << filename << "\n";
        cout << "+++ b/" << filename << "\n";
    }

    size_t i = 0, j = 0;
    while (i < oldLines.size() || j < newLines.size()) {
        if (i < oldLines.size() && j < newLines.size() && oldLines[i] == newLines[j]) {
            cout << "  " << oldLines[i] << "\n";
            i++; j++;
        } 
        else {
            if (i < oldLines.size()) {
                cout << "- " << oldLines[i] << "\n";
                i++;
            }
            if (j < newLines.size()) {
                cout << "+ " << newLines[j] << "\n";
                j++;
            }
        }
    }
    cout << "\n";
}