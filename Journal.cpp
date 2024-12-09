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

using namespace std;

#define EVENT_SIZE (sizeof(inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

// Function to get current timestamp
string getCurrentTime() {
    time_t now = time(nullptr);
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buf);
}

// Function to read the file content into a map
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

// Function to log changes to the journal
void logChanges(const string &journalPath, const map<int, string> &oldContent, const map<int, string> &newContent) {
    ofstream journal(journalPath, ios::app);
    string timestamp = getCurrentTime();

    // Check for added lines
    for (const auto &pair : newContent) {
        if (oldContent.find(pair.first) == oldContent.end()) {
            journal << timestamp << " + l" << pair.first << ":" << pair.second << endl;
        }
    }

    // Check for removed lines
    for (const auto &pair : oldContent) {
        if (newContent.find(pair.first) == newContent.end()) {
            journal << timestamp << " - l" << pair.first << ":" << pair.second << endl;
        }
    }

    journal.close();
    cout << "Journal updated: " << journalPath << endl;
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

    map<string, map<int, string>> fileStates; // Stores the current state of each watched file

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
                string journalPath = journalFolder + "/j1_" + filename + ".DAT";

                // Skip non-txt files and .journal files
                if (filename.find(".txt") == string::npos || filename.find(".journal") != string::npos || filename.find(".swp") != string::npos) {
                    i += EVENT_SIZE + event->len;
                    continue;
                }

                if (event->mask & (IN_CREATE | IN_MODIFY)) {
                    cout << "File " << filename << " was modified or created.\n";

                    map<int, string> newContent = readFileContent(filePath);
                    if (fileStates.find(filename) == fileStates.end()) {
                        // Log all lines as added for a new file
                        ofstream journal(journalPath, ios::app);
                        for (const auto &pair : newContent) {
                            journal << getCurrentTime() << " + l" << pair.first << ":" << pair.second << endl;
                        }
                        journal.close();
                        cout << "New journal created: " << journalPath << endl;

                    } else {
                        // Compare old and new content and log changes
                        logChanges(journalPath, fileStates[filename], newContent);
                    }
                    fileStates[filename] = newContent; // Update the stored state
                } else if (event->mask & IN_DELETE) {
                    cout << "File " << filename << " was deleted.\n";
                    fileStates.erase(filename); // Remove the file from state tracking
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }

    close(fd);
    return 0;
}
