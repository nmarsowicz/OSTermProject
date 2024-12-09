#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/types.h>

using namespace std;

#define EVENT_SIZE (sizeof(inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

string getCurrentTime() {
    time_t now = time(nullptr);
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d at %H:%M:%S", localtime(&now));
    return string(buf);
}

// Function to read the contents of a file and return as a vector of strings
vector<string> readFileContents(const string& filePath) {
    vector<string> lines;
    ifstream file(filePath);
    string line;
    while (getline(file, line)) {
        lines.push_back(line);
    }
    return lines;
}

int main() {
    const char* watchedfolder = "/home/Shag/WatchDir";
    string EventType;

    int fd = inotify_init();
    if (fd < 0) {
        cerr << "Error starting inotify.\n";
        return 1;
    }

    // Add watch for file creation, modification, and deletion
    int watch = inotify_add_watch(fd, watchedfolder, IN_MODIFY | IN_DELETE | IN_CREATE);
    if (watch < 0) {
        cerr << "Error adding watch to folder.\n";
        return 1;
    }

    char buffer[BUF_LEN];
    cout << "Watching the folder for file changes..." << endl;

    while (true) {
        int length = read(fd, buffer, BUF_LEN);
        if (length < 0) {
            cerr << "Error reading inotify events.\n";
            continue;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            if (event->len) {
                string filename = event->name;

                // Skip .journal files to prevent the loop
                if (filename.find(".journal") != string::npos) {
                    i += EVENT_SIZE + event->len;
                    continue;
                }

                // Only process .txt files
                if (filename.find(".txt") == string::npos) {
                    i += EVENT_SIZE + event->len;
                    continue;
                }

                string filePath = string(watchedfolder) + "/" + filename;
                string journalFilePath = string(watchedfolder) + "/.JournalDir/" + filename + ".journal";

                if (event->mask & IN_MODIFY) {
                    EventType = "Modified";
                    cout << "File: " << filename << " Was Modified\n";

                    // Read the contents of the modified file
                    vector<string> lines = readFileContents(filePath);

                    // Log the file contents line by line to the journal
                    ofstream journalFile(journalFilePath, ios::app);
                    if (journalFile) {
                        journalFile << "[" << getCurrentTime() << "] The file: " << filename << " was: " << EventType << endl;
                        for (size_t lineNum = 0; lineNum < lines.size(); ++lineNum) {
                            journalFile << "Line " << lineNum + 1 << ": \"" << lines[lineNum] << "\"" << endl;
                        }
                    } else {
                        cerr << "Error while writing to the journal file: " << journalFilePath << endl;
                    }

                } else if (event->mask & IN_DELETE) {
                    EventType = "Deleted";
                    cout << "File: " << filename << " Was Deleted\n";
                } else if (event->mask & IN_CREATE) {
                    EventType = "Created";
                    cout << "File: " << filename << " Was Created\n";
                }
	}
	i += EVENT_SIZE + event->len;
}
}
close(fd);
return 0; }
