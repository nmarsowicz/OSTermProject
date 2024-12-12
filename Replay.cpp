#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

// Function to replay file content up to a specific timestamp
void replayFile(const string &journalFilePath, const string &endTime, const string &outputFileName) {
    ifstream journalFile(journalFilePath);
    if (!journalFile) {
        cerr << "Error opening journal file." << endl;
        return;
    }

    ofstream outputFile(outputFileName);
    if (!outputFile) {
        cerr << "Error creating output replay file." << endl;
        return;
    }

    string line;
    vector<string> fileContent;  // Stores the current state of the file

    while (getline(journalFile, line)) {
        // Skip separator lines
        if (line == "----------") {
            continue;
        }

        // Extract the timestamp (first 19 characters)
        if (line.length() >= 19) {
            string timestamp = line.substr(0, 19);

            // Only apply changes that occurred on or before the specified end time
            if (timestamp <= endTime) {
                if (line.find("+ l") != string::npos) {
                    // This is an addition: extract the content after ": "
                    size_t pos = line.find(": ");
                    if (pos != string::npos) {
                        string content = line.substr(pos + 2);
                        fileContent.push_back(content);
                    }
                }
            }
        }
    }

    // Write the final reconstructed content to the output file
    for (const string &content : fileContent) {
        outputFile << content << endl;
    }

    cout << "Replay file created: " << outputFileName << endl;

    journalFile.close();
    outputFile.close();
}

int main() {
    string journalFilePath, endTime, outputFileName;

    // Ask for journal file path
    cout << "Enter the journal file path (e.g., /home/user/WatchDir/.JournalDir/Test.txt.128328.DAT): ";
    cin >> journalFilePath;

    // Ask for the end timestamp
    cout << "Enter the end timestamp (YYYY-MM-DD HH:MM:SS): ";
    cin.ignore();  // Clear the input buffer
    getline(cin, endTime);

    // Generate the output file name based on the journal file name
    size_t pos = journalFilePath.rfind('/');
    string filename = (pos != string::npos) ? journalFilePath.substr(pos + 1) : journalFilePath;
    outputFileName = filename + ".replay.txt";

    // Call the replay function
    replayFile(journalFilePath, endTime, outputFileName);

    return 0;
}

