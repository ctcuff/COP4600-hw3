#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>

#define HISTORY_FILE_PATH "./mysh.history"

std::unordered_set<pid_t> activePids{};
std::unordered_set<std::string> VALID_COMMANDS = {
    "background", "byebye", "history", "repeat",
    "replay", "start", "terminate", "terminateall"};

void parseCommand(
    const std::string& command,
    const std::vector<std::string>& args,
    std::vector<std::string>& history);

// If no arguments are passed, this prints all history (current application history plus the
// history saved in mysh.history). If "-c" is passed, all history will be cleared (including
// this history in the history file).
void executeHistoryCommand(std::vector<std::string>& history, const std::vector<std::string>& args);

// Re-executes the command at the given number in history.
void executeReplayCommand(std::vector<std::string>& history, const int index);

// If the parameter background is true, this will start the specified program and return
// execution back to this program. Otherwise, execution will halt until the given
// program finishes executing.
void executeStartCommand(const std::vector<std::string>& args, bool background);

// Takes a program name and (optionally) additional arguments that are passed to
// that program
void executeRepeatCommand(const std::vector<std::string>& args, std::vector<std::string>& history);
void terminateProcess(const int pid);
void terminateAllProcesses();

namespace Util {
    std::vector<std::string> splitString(const std::string& string, const char delimeter) {
        auto tokens = std::vector<std::string>{};
        std::string token;
        std::stringstream ss(string);

        while (std::getline(ss, token, delimeter)) {
            tokens.push_back(token);
        }

        return tokens;
    }

    bool doesFileExist(const std::string& path) {
        return access(path.c_str(), F_OK) != -1;
    }

    int writeHistory(const std::vector<std::string>& history) {
        try {
            std::ofstream file;
            file.open(HISTORY_FILE_PATH);

            for (auto& command : history) {
                if (command != "byebye") {
                    file << command << std::endl;
                }
            }

            std::cout << "mysh: History saved to " << HISTORY_FILE_PATH << std::endl;
            file.close();
        } catch (const std::exception& err) {
            std::cout << "mysh: Error writing to history file" << err.what() << std::endl;
            return -1;
        }

        return 0;
    }

    std::vector<std::string> loadHistory() {
        auto history = std::vector<std::string>{};

        std::ifstream file;
        std::string line;
        file.open(HISTORY_FILE_PATH, std::ios_base::in);

        if (file.fail()) {
            return history;
        }

        while (std::getline(file, line, '\n')) {
            history.push_back(line);
        }

        file.close();

        return history;
    }
}

int main() {
    std::string line;

    // This history stores all commands from the history file
    // as well as commands from the current session's history
    auto history = Util::loadHistory();

    while (line != "byebye") {
        std::cout << "# ";
        // Need to use std::getline instead of std::cin because cin skips spaces
        std::getline(std::cin, line, '\n');

        history.push_back(line);

        std::vector<std::string> tokens = Util::splitString(line, ' ');

        if (tokens.size() == 0) continue;

        std::string command = tokens[0];
        auto args = std::vector<std::string>(tokens.begin() + 1, tokens.end());

        parseCommand(command, args, history);
    }

    return Util::writeHistory(history);
}

void parseCommand(
    const std::string& command,
    const std::vector<std::string>& args,
    std::vector<std::string>& history) {

    if (VALID_COMMANDS.find(command) == VALID_COMMANDS.end()) {
        std::cerr << "mysh: " << command << ": command not found" << std::endl;
        return;
    }

    if (command == "background" || command == "start") {
        if (args.size() < 1) {
            std::cerr << "mysh: Missing argument [program]" << std::endl;
            return;
        }

        executeStartCommand(args, command == "background");
    }

    if (command == "byebye") {
        exit(Util::writeHistory(history));
    }

    if (command == "history") {
        executeHistoryCommand(history, args);
    }

    if (command == "repeat") {
        executeRepeatCommand(args, history);
    }

    if (command == "replay") {
        if (args.size() < 1) {
            std::cerr << "mysh: Missing argument [index]" << std::endl;
            return;
        }

        try {
            executeReplayCommand(history, std::stoi(args[0]));
        } catch (const std::invalid_argument& err) {
            std::cerr << "mysh: Argument must be a number" << std::endl;
        } catch (const std::out_of_range& err) {
            std::cerr << "mysh: Argument out of range" << std::endl;
        }
    }

    if (command == "terminate") {
        if (args.size() < 1) {
            std::cerr << "mysh: Missing argument [pid]" << std::endl;
            return;
        }

        try {
            terminateProcess((pid_t)std::stoi(args[0]));
        } catch (const std::invalid_argument& err) {
            std::cerr << "mysh: Argument must be a number" << std::endl;
        } catch (const std::out_of_range& err) {
            std::cerr << "mysh: Argument out of range" << std::endl;
        }
    }

    if (command == "terminateall") {
        terminateAllProcesses();
    }
}

void executeHistoryCommand(std::vector<std::string>& history, const std::vector<std::string>& args) {
    int historySize = history.size();

    if (args.size() == 0) {
        for (int i = historySize - 1; i >= 0; i--) {
            int index = historySize - (i + 1);
            std::cout << index << ": " << history[i] << std::endl;
        }
        return;
    }

    if (args[0] == "-c") {
        history.clear();
        std::cout << "mysh: History cleared" << std::endl;
    }
}

void executeReplayCommand(std::vector<std::string>& history, const int index) {
    if (index == history.size()) {
        std::cerr << "mysh: Index out of range" << std::endl;
        return;
    }

    // Need to subtract 2 here because "replay" will be added to the history
    // before the command is run and we need to get the command that was executed
    // at position index - 1.
    std::string command = history[history.size() - index - 2];

    auto tokens = Util::splitString(command, ' ');
    auto args = std::vector<std::string>(tokens.begin() + 1, tokens.end());

    // Don't replay a replay command since it might cause an infinite loop
    if (command.find("replay") != 0) {
        parseCommand(tokens[0], args, history);
    } else {
        std::cerr << "mysh: Cannot replay a replay command" << std::endl;
    }
}

void executeStartCommand(const std::vector<std::string>& args, bool background) {
    // Check if the file exists before running so we don't unnecessarily fork
    if (!Util::doesFileExist(args[0])) {
        std::cerr << "mysh: " << args[0] << ": No such file or directory" << std::endl;
        return;
    }

    // execv ony takes a char** array so we need to convert the string vector
    // arguments to a char** array
    const char** programArgs = new const char*[args.size() + 1];

    for (int i = 0; i < args.size(); i++) {
        programArgs[i] = args[i].c_str();
    }

    programArgs[args.size()] = NULL;

    pid_t pid = fork();

    if (pid == -1) {
        std::cerr << "mysh: Couldn't fork process." << std::endl;
        return;
    }

    activePids.insert(pid);

    if (pid == 0) {
        // This process is the child process
        int statusCode = execv(programArgs[0], (char**)programArgs);

        if (statusCode != 0) {
            std::cerr << "mysh: " << std::strerror(errno) << std::endl;
        }

        activePids.erase(pid);
        exit(statusCode);
    } else {
        // This is the original process (the one that started the child process)
        if (background) {
            std::cout << "mysh: Spawned process with pid " << pid << std::endl;
        } else {
            int status;
            waitpid(pid, &status, 0);
            activePids.erase(pid);
        }
    }
}

void terminateProcess(const pid_t pid) {
    if (pid < 0) {
        std::cerr << "mysh: Argument [pid] must be >= 0" << std::endl;
        return;
    }

    int statusCode = kill(pid, SIGTERM);

    if (statusCode != 0) {
        std::cerr << "mysh: " << std::strerror(errno) << std::endl;
        return;
    }

    std::cout << "mysh: Terminated process with pid " << pid << std::endl;
}

void executeRepeatCommand(const std::vector<std::string>& args, std::vector<std::string>& history) {
    if (args.size() < 2) {
        std::cerr << "mysh: Usage: repeat [repetitions] [command]" << std::endl;
        return;
    }

    int repetitions;
    auto command = std::vector<std::string>(args.begin() + 1, args.end());

    try {
        repetitions = std::stoi(args[0]);
    } catch (const std::invalid_argument& err) {
        std::cerr << "mysh: Argument [repetitions] must be a number" << std::endl;
        return;
    } catch (const std::out_of_range& err) {
        std::cerr << "mysh: Argument [repetitions] out of range" << std::endl;
        return;
    }

    for (int i = 0; i < repetitions; i++) {
        executeStartCommand(command, true);
    }
}

void terminateAllProcesses() {
    if (activePids.size() == 0) {
        std::cout << "mysh: No precesses to terminate" << std::endl;
        return;
    }

    size_t numPids = activePids.size();

    for (auto pid : activePids) {
        terminateProcess(pid);
    }

    activePids.clear();

    std::cout << "mysh: Terminated "
              << numPids
              << (numPids == 1 ? " process" : " processes")
              << std::endl;
}
