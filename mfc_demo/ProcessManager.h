#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>

struct ProcessInfo
{
    int id;
    std::wstring exePath;
    std::wstring arguments;
    PROCESS_INFORMATION processInfo;
    bool isRunning;

    ProcessInfo() : id(0), isRunning(false)
    {
        ZeroMemory(&processInfo, sizeof(processInfo));
    }
};

class ProcessManager
{
public:
    static ProcessManager& GetInstance();

    int AddProcess(const std::wstring& exePath, const std::wstring& arguments);
    bool RemoveProcess(int id);
    bool StartProcess(int id);
    bool StopProcess(int id);
    bool IsProcessRunning(int id);
    ProcessInfo* GetProcess(int id);
    const std::vector<ProcessInfo*>& GetAllProcesses() const;
    void UpdateProcessStatuses();

private:
    ProcessManager();
    ~ProcessManager();
    ProcessManager(const ProcessManager&) = delete;
    ProcessManager& operator=(const ProcessManager&) = delete;

    int m_nextId;
    std::vector<ProcessInfo*> m_processes;
    std::map<int, ProcessInfo*> m_processMap;
};
