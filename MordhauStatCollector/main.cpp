#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif
#include "HTTPRequest.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#include <unistd.h>
#include <limits.h>
#include <sys/inotify.h>
#endif
//#include "AES.h" NOT USED RIGHT NOW. TODO Encrypted http requests

// This program working directory should be the Servers SaveGame folder
// Location of UE4 .sav file for a custom skirmish server (relative to programs working directory)
#define LOG_PATH "\\CustomSkirmish\\LastLog"
#define LOG_FILENAME "LastMatchLog.sav"

// Location of the file that Custom Skirmish mod checks for custom whitelist and which this program provides
// Mod rewrites it whenever map starts which triggers function that sends http request to server for current whitelist
#define SKIRMISH_WHITELIST_PATH "\\CustomSkirmish\\Whitelist"
#define SKIRMISH_WHITELIST_FILENAME "Whitelist.sav"

// Location of UE4 .sav file for a lobby server (relative to exe)
#define LOBBY_INTERFACE_MOD_OUTPUT_PATH "\\Skalagrad\\ModOutput"
#define LOBBY_INTERFACE_MOD_INPUT_PATH "\\Skalagrad\\ModInput"
#define LOBBY_INTERFACE_MOD_OUTPUT_FILENAME "InterfaceModOutput.sav" // File that is changed by Mordhau lobby mod and moniotered here
#define LOBBY_INTERFACE_MOD_INPUT_FILENAME "InterfaceModInput.sav" // File that is changed here and monitored by Mordhau lobby mod

// Used to mark extractable string in UE4 .sav files
#define EXTRACT_FIRST_LINE "STARTMATCHINFOSTRING"
#define EXTRACT_LAST_LINE "ENDMATCHINFOSTRING"

// Server ip (can use webhook.site to test but you should not use secure http)
#define SERVER_IP "http://webhook.site/73902a10-1240-438b-8391-2f4fbdfa0b8a"

// Time to wait for file change completion after first time system hook was triggered
#define ACTION_DELAY 100

// A string that is provided by user on program start that is used to identify this Mordhau server by Skalagrad site
// No actual validity check on program side exists right now ( TODO Server identifier check? )
std::string ServerIdentifier("");

// Function that contains actions for log collection (extracting JSON string from UE4 .sav file and sending it to server)
bool OnChangeActionLog();
// Function that contains actions for interpreting changes in lobby .sav file and interfacing with the server
bool OnChangeActionLobbymodOutput();
// Pools data from Skalagrad site and updates mod input UE4 .sav accordingly
bool PoolingIteration();
// Whenever skirmish map starts mod rewrites whitelist UE4 .sav file so program can trigger and pool info into it from Skalagrad site
bool OnChangeWhitelist();
// Loop function that calls given function, waits X seconds and repeats
void RepeatLoop(bool(*FunctionToCall)(), int Seconds);
// System-dependent loop function that calls given function whenever there are changes in provided directory path
void ListenerLoop(bool(*OnChangeAction)(), std::string const& AbsolutePath); // TODO fix: will silently break if no such folder
// System-dependent function that returns path to the program working directory
std::string ExePath();


int main() {
	std::string WorkingDirectory(ExePath());
	std::string AbsoluteLogDirectoryPath(WorkingDirectory + LOG_PATH);
	std::string AbsoluteLobbyOutDirectoryPath(WorkingDirectory + LOBBY_INTERFACE_MOD_OUTPUT_PATH);
	std::string AbsoluteWhitelistDirectoryPath(WorkingDirectory + SKIRMISH_WHITELIST_PATH);

	std::cout << "Enter server identifier:" << std::endl;
	std::cin >> ServerIdentifier;

	std::cout << "Started monitoring from:" << std::endl;
	std::cout << WorkingDirectory << std::endl;

	// Start Monitoring for new logs
	std::thread MatchLogThread (ListenerLoop, OnChangeActionLog, AbsoluteLogDirectoryPath);

	// Start Monitoring lobby UE4 .sav
	std::thread LobbyInterfaceThread (ListenerLoop, OnChangeActionLobbymodOutput, AbsoluteLobbyOutDirectoryPath);

	// Start Monitoring whitelist folder
	std::thread SkirmishWhitelistThread (ListenerLoop, OnChangeWhitelist, AbsoluteWhitelistDirectoryPath);

	// Start pooling info from Skalagrad site
	std::thread InfoPoolingThread (RepeatLoop, PoolingIteration, 5);

	MatchLogThread.join();
}

std::string GetStringFromSav(std::string const& filepath) {
	std::string line;
	std::string outputString;
	std::ifstream saveFile(filepath, std::ios::binary);
	//std::cout << logfilename << std::endl;
	bool isMatchInfo = false;
	size_t found;
	if (saveFile.is_open()) {
		while (std::getline(saveFile, line)) {
			//std::cout << line << std::endl;
			found = line.find(EXTRACT_FIRST_LINE);
			if (found != std::string::npos) {
				isMatchInfo = false;
			}
			if (isMatchInfo) {
				outputString += line + "\n";
			}
			found = line.find(EXTRACT_LAST_LINE);
			if (found != std::string::npos) {
				isMatchInfo = true;
			}
		}
		saveFile.close();
	}
	return outputString;
}

void LoadSav(std::string const& filename, std::vector<std::string>& dest) {

}

void SendLog(std::string const& Log) {
	
	try {
		http::Request request(SERVER_IP);

		const http::Response response = request.send("POST", Log, {
			"Content-Type: text/plain"
			});
		std::cout << "Log sent successfully" << '\n';
	}
	catch (const std::exception& e) {
		std::cerr << "Request failed, error: " << e.what() << '\n';
	}
}

bool OnChangeActionLog() {
	std::this_thread::sleep_for(std::chrono::milliseconds(ACTION_DELAY));
	std::string outputString = GetStringFromSav(ExePath() + LOG_PATH + "\\" + LOG_FILENAME);
	//std::remove(logfilename.c_str()); Now mordhau mod removes logs by itself
	if (outputString.length() > 0) {
		std::cout << "New log!" << std::endl;
		SendLog(outputString);
		return true;
	}
	else {
		return false;
	}
}

bool OnChangeActionLobbymodOutput() {
	std::this_thread::sleep_for(std::chrono::milliseconds(ACTION_DELAY));
	std::string outputString = GetStringFromSav(ExePath() + LOBBY_INTERFACE_MOD_OUTPUT_PATH + "\\" + LOBBY_INTERFACE_MOD_OUTPUT_FILENAME);
	if (outputString.length() > 0) {
		// doing stuff
		return true;
	}
	else {
		return false;
	}
}

bool OnChangeWhitelist() {
	std::this_thread::sleep_for(std::chrono::milliseconds(ACTION_DELAY));
	std::string outputString = GetStringFromSav(ExePath() + SKIRMISH_WHITELIST_PATH + "\\" + SKIRMISH_WHITELIST_FILENAME);
	if (outputString.length() > 0) {
		// doing stuff
		return true;
	}
	else {
		return false;
	}
}

bool PoolingIteration() {
	// make http request and get info block

	return false;
}

std::string ExePath() {
#ifdef _WIN32
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
#else
	char arg1[20];
	char buffer[PATH_MAX + 1] = { 0 };
	sprintf(arg1, "/proc/%d/exe", getpid());
	readlink(arg1, buffer, 1024);
#endif
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	return std::string(buffer).substr(0, pos);
}

void RepeatLoop(bool(*FunctionToCall)(), int Seconds) {
	for (;;) {
		FunctionToCall();
		std::this_thread::sleep_for(std::chrono::seconds(Seconds));
	}
}

void ListenerLoop(bool(*OnChangeAction)(), std::string const& AbsolutePath)
{	
#ifdef _WIN32
	const char* absolutePathP = AbsolutePath.c_str();
	HANDLE WriteEventHook = FindFirstChangeNotificationA(absolutePathP, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);
	for (;;) {
		DWORD EventListener = WaitForSingleObject(WriteEventHook, INFINITE);
		if (EventListener == WAIT_OBJECT_0) {
			FindCloseChangeNotification(WriteEventHook);
			OnChangeAction();
			WriteEventHook = FindFirstChangeNotificationA(absolutePathP, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);
		}
		else {
			break;
		}
	}
#else
	int length, i = 0;
	int fd;
	int wd;
	char buffer[(1024 * (sizeof(struct inotify_event) + 16))];
	fd = inotify_init();
	if (fd < 0) {
		perror("inotify_init");
	}
	wd = inotify_add_watch(fd, (ExePath() + LOG_PATH).c_str(),
		IN_MODIFY | IN_CREATE | IN_DELETE);

	for (;;) {
		length = read(fd, buffer, (1024 * (sizeof(struct inotify_event) + 16)));
		OnChangeAction();
	}
#endif
}
