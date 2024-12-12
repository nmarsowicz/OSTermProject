#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sstream>
#include <ctime>
#include <sys/stat.h>
#include <cstdlib>  // For rand()
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

#define EVENT_SIZE (sizeof(inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

// Function to get current timestamp
string getCurrentTime() {
    time_t now = time(nullptr);
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buf);
}

// Function to generate a random 6-digit number
string generateRandomCode() {
    return to_string(rand() % 1000000);  // Generates a random number between 0 and 999999
}

// Read file content into a map
map<int, string> readFileContent(const string &filePath) {
    ifstream file(filePath);
    map<int, string> content;
    string line;
    int lineNumber = 1;

    while (getline(file, line)) {
        content[lineNumber++] = line;
    }
    return content;
}

// Log changes between old and new content
void logChanges(const string &journalPath, const map<int, string> &oldContent, const map<int, string> &newContent) {
    ofstream journal(journalPath, ios::app);
    string timestamp = getCurrentTime();
    bool hasChanges = false;

    // Vectors to track old and new content lines
    vector<string> oldLines;
    vector<string> newLines;

    for (const auto &pair : oldContent) {
        oldLines.push_back(pair.second);
    }

    for (const auto &pair : newContent) {
        newLines.push_back(pair.second);
    }

    // Track deletions
    for (size_t i = 0; i < oldLines.size(); ++i) {
        if (find(newLines.begin(), newLines.end(), oldLines[i]) == newLines.end()) {
            journal << timestamp << " - l" << i + 1 << ": " << oldLines[i] << endl;
            hasChanges = true;
        }
    }

    // Track additions
    for (const auto &pair : newContent) {
        if (oldContent.find(pair.first) == oldContent.end()) {
            journal << timestamp << " + l" << pair.first << ": " << pair.second << endl;
            hasChanges = true;
        }
    }

    // Add separator only if changes occurred
    if (hasChanges) {
        journal << "----------" << endl;
    }

    journal.close();
}

// Function to get the journal filename
string getJournalFilename(const string &filePath, const string &journalFolder, map<string, string> &fileJournalMap) {
    string filename = filePath.substr(filePath.find_last_of("/\\") + 1);  // Get the filename (without path)
    string journalBaseName = filename + ".DAT";  // Default journal name (without 6-digit code)

    // Check if the journal file already exists
    if (fileJournalMap.find(filename) != fileJournalMap.end()) {
        return journalFolder + "/" + filename + "." + fileJournalMap[filename] + ".DAT";  // Reuse existing 6-digit code
    }

    // Generate a new unique journal name if this is the first occurrence of the filename
    string uniqueCode = generateRandomCode();
    fileJournalMap[filename] = uniqueCode;  // Store the unique code for this file

    return journalFolder + "/" + filename + "." + uniqueCode + ".DAT";  // Return the journal with unique code
}

int main() {
    const char *watchedFolder = "/home/Shag/WatchDir";
    const string journalFolder = string(watchedFolder) + "/.JournalDir";

    int fd = inotify_init();
    if (fd < 0) {
        cerr << "Error starting inotify.\n";
        return 1;
    }

    int watch = inotify_add_watch(fd, watchedFolder, IN_MODIFY | IN_CREATE | IN_DELETE);
    if (watch < 0) {
        cerr << "Error adding watch to folder.\n";
        return 1;
    }

    char buffer[BUF_LEN];
    cout << "Watching the folder for file changes..." << endl;

    map<string, map<int, string>> fileStates;
    map<string, string> fileJournalMap;  // Map to store unique codes for each file

    while (true) {
        int length = read(fd, buffer, BUF_LEN);
        if (length < 0) {
            cerr << "Error reading inotify events.\n";
            continue;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];

            if (event->len) {
                string filename = event->name;
                string filePath = string(watchedFolder) + "/" + filename;

                // Get the correct journal filename (with unique number if necessary)
                string journalPath = getJournalFilename(filePath, journalFolder, fileJournalMap);

                // Skip irrelevant files
                if (filename.find(".txt") == string::npos || filename.find(".journal") != string::npos || filename.find(".swp") != string::npos) {
                    i += EVENT_SIZE + event->len;
                    continue;
                }

                if (event->mask & IN_CREATE) {
                    cout << "File created: " << filename << endl;

                    map<int, string> newContent = readFileContent(filePath);
                    ofstream journal(journalPath, ios::app);
                    for (const auto &pair : newContent) {
                        journal << getCurrentTime() << " + l" << pair.first << ": " << pair.second << endl;
                    }
                    journal << "----------" << endl;
                    journal.close();

                    cout << "New journal created: " << journalPath << endl;
                    fileStates[filename] = newContent;

                } else if (event->mask & IN_MODIFY) {
                    cout << "File modified: " << filename << endl;

                    if (fileStates.find(filename) != fileStates.end()) {
                        map<int, string> newContent = readFileContent(filePath);
                        logChanges(journalPath, fileStates[filename], newContent);
                        cout << "Journal updated: " << journalPath << endl;
                        fileStates[filename] = newContent;
                    }

                } else if (event->mask & IN_DELETE) {
                    // Change the journal filename by appending ".deleted" to indicate deletion
                    cout << "File deleted: " << filename << endl;

                    // Modify the journal filename to include the "deleted" tag
                    string deletedJournalPath = journalPath.substr(0, journalPath.find_last_of('.')) + ".deleted.DAT";

                    // Rename the journal to reflect the deletion
                    if (fs::exists(journalPath)) {
                        fs::rename(journalPath, deletedJournalPath);
                    }

                    cout << "Journal updated (file deleted): " << deletedJournalPath << endl;
                    fileStates.erase(filename);
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }

    close(fd);
    return 0;
}

