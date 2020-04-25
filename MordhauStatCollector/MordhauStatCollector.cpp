#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>

const std::string path("\\LastLog");
const std::string filename("LastMatchLog.sav");
const std::string stringStartLine("STARTMATCHINFOSTRING");
const std::string stringEndLine("ENDMATCHINFOSTRING");

bool OnChangeAction();
void RunWindowsListenerThread();
std::string ExePath();

int main()
{
	Sleep(1000);
	RunWindowsListenerThread();
}

bool OnChangeAction()
{
	std::string line;
	std::string outputString;
	std::ifstream saveFile(ExePath() + path + "\\" + filename, std::ios::binary);
	std::cout << ExePath() + path + "\\" + filename << std::endl;
	bool isMatchInfo = false;
	size_t found;
	if (saveFile.is_open()) {
		while (std::getline(saveFile, line)) {
			//std::cout << line << std::endl;
			found = line.find(stringEndLine);
			if (found != std::string::npos) {
				isMatchInfo = false;
			}
			if (isMatchInfo) {
				outputString += line + "\n";
			}
			found = line.find(stringStartLine);
			if (found != std::string::npos) {
				isMatchInfo = true;
			}
		}
		saveFile.close();
	}
	std::cout << "Something changed in save directory" << std::endl;
	std::cout << outputString << std::endl;
	return false;
}

// Windows Change Notification

std::string ExePath() {
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	return std::string(buffer).substr(0, pos);
}

void RunWindowsListenerThread()
{	
	std::string absolutePath = ExePath() + path;
	const char* absolutePathP = absolutePath.c_str();
	std::cout << "Monitoring changes in:" << std::endl;
	std::cout << absolutePath << std::endl;
	HANDLE WriteEventHook = FindFirstChangeNotificationA(absolutePathP, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);
	for (;;) {
		DWORD EventListener = WaitForSingleObject(WriteEventHook, INFINITE);
		if (EventListener == WAIT_OBJECT_0) {
			FindCloseChangeNotification(WriteEventHook);
			Sleep(1000);
			OnChangeAction();
			WriteEventHook = FindFirstChangeNotificationA(absolutePathP, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);
		}
		else {
			break;
		}
	}
}