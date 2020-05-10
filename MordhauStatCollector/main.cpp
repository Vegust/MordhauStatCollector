#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif
#include "HTTPRequest.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#include <unistd.h>
#include <limits.h>
#include <sys/inotify.h>
#endif
//#include "AES.h" NOT USED RIGHT NOW


// Exe file should be placed in Servers SaveGame folder
// Location of UE4 .sav file for a custom skirmish server (relative to exe)
#define LOG_PATH "\\CustomSkirmish\\LastLog"
#define LOG_FILENAME "LastMatchLog.sav"

// Location of UE4 .sav file for a lobby server (relative to exe)
#define LOBBY_INTERFACE_PATH "\\Skalagrad"
#define LOBBY_INTERFACE_FILENAME "Interface.sav"

// Used to mark extractable string in UE4 .sav files
#define EXTRACT_FIRST_LINE "STARTMATCHINFOSTRING"
#define EXTRACT_LAST_LINE "ENDMATCHINFOSTRING"

// DAWASTI's server is 81.169.241.157:8081
// Webhook.site for testing is http://webhook.site/73902a10-1240-438b-8391-2f4fbdfa0b8a
#define SERVER_IP "http://webhook.site/73902a10-1240-438b-8391-2f4fbdfa0b8a"

// Time to wait for file change completion after first time system hook was triggered
#define ACTION_DELAY 100

// Function that contains actions for log collection (extracting JSON string from UE4 .sav file and sending it to server)
bool OnChangeActionLog();
// Function that contains actions for interpreting changes in lobby .sav file and interfacing with the server
bool OnChangeActionLobbyInterface();
// System-dependent loop function that calls given function whenever there are changes in provided directory path
void ListenerLoop(bool(*OnChangeAction)(), std::string AbsolutePath);
// System-dependent function that returns path to the exe working directory
std::string ExePath();
//AES aes(256); NOT USED RIGHT NOW

int main() {
	std::string AbsoluteLogDirectoryPath = ExePath() + LOG_PATH;
	std::cout << "Started monitoring MORDHAU log changes in:" << std::endl;
	std::cout << AbsoluteLogDirectoryPath << std::endl;
	// Start Monitoring for new logs
	ListenerLoop(OnChangeActionLog, AbsoluteLogDirectoryPath);
}

std::string ReadLogFile() {
	std::string line;
	std::string outputString;
	std::string AbsoluteLogFilePath = ExePath() + LOG_PATH + "\\" + LOG_FILENAME;
	std::ifstream saveFile(AbsoluteLogFilePath, std::ios::binary);
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

void SendLog(std::string Log) {
	
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
	std::chrono::milliseconds(ACTION_DELAY);
	std::string outputString = ReadLogFile();
	//std::remove(logfilename.c_str()); Now mordhau mod removes logs by itself
	if (outputString.length() > 0) {
		std::cout << "New log!" << std::endl;
		SendLog(outputString);
		return true;
	}
	else {
		return false;
	}

	/*unsigned char plain[200000];
	std::copy(outputString.begin(), outputString.end(), plain);
	unsigned int outputlen = outputString.length();
	plain[outputlen] = 0;
	unsigned char key[] = "H+MbQeThWmZq4t7w!z%C*F-JaNcRfUjX";
	unsigned int bytesize = outputlen * sizeof(unsigned char);
	unsigned int len = 0;
	unsigned char *out = aes.EncryptECB(plain, bytesize, key, len);
	unsigned char *innew = aes.DecryptECB(out, bytesize, key);
	std::cout << innew << std::endl;*/
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

void ListenerLoop(bool(*OnChangeAction)(), std::string AbsolutePath)
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