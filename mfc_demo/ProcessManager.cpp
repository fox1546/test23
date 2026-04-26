#include "framework.h"
#include "ProcessManager.h"

ProcessManager& ProcessManager::GetInstance()
{
    static ProcessManager instance;
    return instance;
}

ProcessManager::ProcessManager() : m_nextId(1)
{
}

ProcessManager::~ProcessManager()
{
    for (auto& proc : m_processes)
    {
        if (proc->isRunning)
        {
            if (proc->processInfo.hProcess)
            {
                TerminateProcess(proc->processInfo.hProcess, 0);
                CloseHandle(proc->processInfo.hProcess);
                CloseHandle(proc->processInfo.hThread);
            }
        }
        delete proc;
    }
    m_processes.clear();
    m_processMap.clear();
}

int ProcessManager::AddProcess(const std::wstring& exePath, const std::wstring& arguments)
{
    ProcessInfo* info = new ProcessInfo();
    info->id = m_nextId++;
    info->exePath = exePath;
    info->arguments = arguments;
    info->isRunning = false;

    m_processes.push_back(info);
    m_processMap[info->id] = info;

    return info->id;
}

bool ProcessManager::RemoveProcess(int id)
{
    auto it = m_processMap.find(id);
    if (it == m_processMap.end())
        return false;

    ProcessInfo* info = it->second;
    if (info->isRunning)
    {
        StopProcess(id);
    }

    for (auto vecIt = m_processes.begin(); vecIt != m_processes.end(); ++vecIt)
    {
        if (*vecIt == info)
        {
            m_processes.erase(vecIt);
            break;
        }
    }

    m_processMap.erase(it);
    delete info;
    return true;
}

bool ProcessManager::StartProcess(int id)
{
    auto it = m_processMap.find(id);
    if (it == m_processMap.end())
        return false;

    ProcessInfo* info = it->second;
    if (info->isRunning)
        return true;

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;

    PROCESS_INFORMATION pi = { 0 };

    std::wstring commandLine = info->exePath;
    if (!info->arguments.empty())
    {
        commandLine += L" " + info->arguments;
    }

    if (!CreateProcessW(NULL, &commandLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        return false;
    }

    info->processInfo = pi;
    info->isRunning = true;

    return true;
}

bool ProcessManager::StopProcess(int id)
{
    auto it = m_processMap.find(id);
    if (it == m_processMap.end())
        return false;

    ProcessInfo* info = it->second;
    if (!info->isRunning)
        return true;

    if (info->processInfo.hProcess)
    {
        TerminateProcess(info->processInfo.hProcess, 0);
        CloseHandle(info->processInfo.hProcess);
        CloseHandle(info->processInfo.hThread);
        ZeroMemory(&info->processInfo, sizeof(info->processInfo));
    }

    info->isRunning = false;
    return true;
}

bool ProcessManager::IsProcessRunning(int id)
{
    auto it = m_processMap.find(id);
    if (it == m_processMap.end())
        return false;

    ProcessInfo* info = it->second;
    if (!info->isRunning)
        return false;

    if (info->processInfo.hProcess)
    {
        DWORD exitCode;
        if (GetExitCodeProcess(info->processInfo.hProcess, &exitCode))
        {
            if (exitCode == STILL_ACTIVE)
                return true;
            else
            {
                CloseHandle(info->processInfo.hProcess);
                CloseHandle(info->processInfo.hThread);
                ZeroMemory(&info->processInfo, sizeof(info->processInfo));
                info->isRunning = false;
                return false;
            }
        }
    }

    return false;
}

ProcessInfo* ProcessManager::GetProcess(int id)
{
    auto it = m_processMap.find(id);
    if (it == m_processMap.end())
        return NULL;
    return it->second;
}

const std::vector<ProcessInfo*>& ProcessManager::GetAllProcesses() const
{
    return m_processes;
}

void ProcessManager::UpdateProcessStatuses()
{
    for (auto& info : m_processes)
    {
        IsProcessRunning(info->id);
    }
}
