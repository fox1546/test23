#pragma execution_character_set("utf-8")

#include "framework.h"
#include "HttpServer.h"
#include "ProcessManager.h"
#include <sstream>
#include <unordered_map>
#include <codecvt>
#include <locale>

HttpServer& HttpServer::GetInstance()
{
    static HttpServer instance;
    return instance;
}

HttpServer::HttpServer() : m_serverSocket(INVALID_SOCKET), m_port(8000), m_isRunning(false)
{
}

HttpServer::~HttpServer()
{
    Stop();
}

bool HttpServer::Start(int port)
{
    if (m_isRunning)
        return true;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return false;
    }

    m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_serverSocket == INVALID_SOCKET)
    {
        WSACleanup();
        return false;
    }

    int optval = 1;
    setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

    sockaddr_in serverAddr;
    ZeroMemory(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(m_serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        closesocket(m_serverSocket);
        WSACleanup();
        return false;
    }

    if (listen(m_serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(m_serverSocket);
        WSACleanup();
        return false;
    }

    m_port = port;
    m_isRunning = true;
    m_serverThread = std::thread(&HttpServer::ServerThread, this);

    return true;
}

void HttpServer::Stop()
{
    if (!m_isRunning)
        return;

    m_isRunning = false;

    if (m_serverSocket != INVALID_SOCKET)
    {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
    }

    if (m_serverThread.joinable())
    {
        m_serverThread.join();
    }

    WSACleanup();
}

void HttpServer::ServerThread()
{
    fd_set readfds;
    timeval tv;

    while (m_isRunning)
    {
        FD_ZERO(&readfds);
        FD_SET(m_serverSocket, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int selectResult = select(0, &readfds, NULL, NULL, &tv);
        if (selectResult == SOCKET_ERROR || !m_isRunning)
            break;

        if (selectResult > 0)
        {
            sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(m_serverSocket, (sockaddr*)&clientAddr, &addrLen);

            if (clientSocket != INVALID_SOCKET)
            {
                HandleClient(clientSocket);
            }
        }
    }
}

void HttpServer::HandleClient(SOCKET clientSocket)
{
    std::string request;
    char buffer[4096];
    int bytesReceived;

    timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));

    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[bytesReceived] = '\0';
        request += buffer;

        if (request.find("\r\n\r\n") != std::string::npos)
            break;
    }

    std::string response;

    if (request.find("GET /api/") != std::string::npos || request.find("POST /api/") != std::string::npos)
    {
        response = HandleApiRequest(request);
    }
    else if (request.find("GET /") != std::string::npos)
    {
        std::string html = GenerateProcessListHtml();
        response = GetHttpResponse(html, "text/html");
    }
    else
    {
        std::string notFound = "<html><body><h1>404 Not Found</h1></body></html>";
        response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: " +
            std::to_string(notFound.length()) + "\r\nConnection: close\r\n\r\n" + notFound;
    }

    send(clientSocket, response.c_str(), (int)response.length(), 0);
    closesocket(clientSocket);
}

std::string HttpServer::GetHttpResponse(const std::string& content, const std::string& contentType)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: " << contentType << "; charset=utf-8\r\n";
    oss << "Content-Length: " << content.length() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    return oss.str();
}

std::string HttpServer::GenerateProcessListHtml()
{
    ProcessManager& pm = ProcessManager::GetInstance();
    const auto& processes = pm.GetAllProcesses();

    std::ostringstream html;
    html << "<!DOCTYPE html>";
    html << "<html><head><meta charset=\"UTF-8\"><title>Process Remote Control</title>";
    html << "<style>";
    html << "body { font-family: 'Microsoft YaHei', Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }";
    html << "h1 { color: #333; text-align: center; }";
    html << ".container { max-width: 1000px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
    html << "table { width: 100%; border-collapse: collapse; margin-top: 20px; }";
    html << "th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }";
    html << "th { background-color: #4CAF50; color: white; }";
    html << "tr:hover { background-color: #f5f5f5; }";
    html << ".status-running { color: #4CAF50; font-weight: bold; }";
    html << ".status-stopped { color: #f44336; font-weight: bold; }";
    html << ".btn { padding: 8px 16px; margin: 2px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }";
    html << ".btn-start { background-color: #4CAF50; color: white; }";
    html << ".btn-start:hover { background-color: #45a049; }";
    html << ".btn-start:disabled { background-color: #cccccc; cursor: not-allowed; }";
    html << ".btn-stop { background-color: #f44336; color: white; }";
    html << ".btn-stop:hover { background-color: #da190b; }";
    html << ".btn-stop:disabled { background-color: #cccccc; cursor: not-allowed; }";
    html << ".btn-refresh { background-color: #2196F3; color: white; float: right; margin-bottom: 10px; }";
    html << ".btn-refresh:hover { background-color: #1976D2; }";
    html << ".add-form { margin-bottom: 20px; padding: 15px; background-color: #e8f5e9; border-radius: 4px; }";
    html << ".add-form input { padding: 8px; margin: 5px; border: 1px solid #ddd; border-radius: 4px; }";
    html << ".add-form .btn-add { background-color: #2196F3; color: white; }";
    html << ".add-form .btn-add:hover { background-color: #1976D2; }";
    html << ".add-form label { display: inline-block; width: 100px; }";
    html << "</style>";
    html << "<script>";
    html << "function refreshPage() { location.reload(); }";
    html << "function startProcess(id) {";
    html << "  fetch('/api/start?id=' + id).then(function(response) { return response.json(); })";
    html << "  .then(function(data) { if(data.success) refreshPage(); else alert('Start failed: ' + data.message); });";
    html << "}";
    html << "function stopProcess(id) {";
    html << "  fetch('/api/stop?id=' + id).then(function(response) { return response.json(); })";
    html << "  .then(function(data) { if(data.success) refreshPage(); else alert('Stop failed: ' + data.message); });";
    html << "}";
    html << "function removeProcess(id) {";
    html << "  if(confirm('Are you sure you want to remove this process?')) {";
    html << "    fetch('/api/remove?id=' + id).then(function(response) { return response.json(); })";
    html << "    .then(function(data) { if(data.success) refreshPage(); else alert('Remove failed: ' + data.message); });";
    html << "  }";
    html << "}";
    html << "function addProcess() {";
    html << "  var exe = document.getElementById('exePath').value;";
    html << "  var args = document.getElementById('arguments').value;";
    html << "  if(!exe) { alert('Please enter the program path'); return; }";
    html << "  var formData = new FormData();";
    html << "  formData.append('exe', exe);";
    html << "  formData.append('args', args);";
    html << "  fetch('/api/add', { method: 'POST', body: formData })";
    html << "  .then(function(response) { return response.json(); })";
    html << "  .then(function(data) { if(data.success) refreshPage(); else alert('Add failed: ' + data.message); });";
    html << "}";
    html << "</script>";
    html << "</head><body>";
    html << "<div class='container'>";
    html << "<h1>Process Remote Control</h1>";
    html << "<button class='btn btn-refresh' onclick='refreshPage()'>Refresh</button>";
    html << "<div style='clear:both;'></div>";

    html << "<div class='add-form'>";
    html << "<h3>Add New Program</h3>";
    html << "<div><label>Program Path:</label><input type='text' id='exePath' size='60' placeholder='e.g. C:\\\\Windows\\\\notepad.exe'></div>";
    html << "<div><label>Arguments:</label><input type='text' id='arguments' size='60' placeholder='optional arguments'></div>";
    html << "<div><label>&nbsp;</label><button class='btn btn-add' onclick='addProcess()'>Add</button></div>";
    html << "</div>";

    html << "<table>";
    html << "<tr><th>ID</th><th>Program Path</th><th>Arguments</th><th>Status</th><th>Action</th></tr>";

    if (processes.empty())
    {
        html << "<tr><td colspan='5' style='text-align:center;'>No programs yet, please add one</td></tr>";
    }
    else
    {
        for (const auto& proc : processes)
        {
            bool isRunning = pm.IsProcessRunning(proc->id);
            html << "<tr>";
            html << "<td>" << proc->id << "</td>";
            html << "<td>" << EscapeHtml(WStringToString(proc->exePath)) << "</td>";
            html << "<td>" << EscapeHtml(WStringToString(proc->arguments)) << "</td>";
            html << "<td class='" << (isRunning ? "status-running" : "status-stopped") << "'>"
                << (isRunning ? "Running" : "Stopped") << "</td>";
            html << "<td>";
            if (isRunning)
            {
                html << "<button class='btn btn-start' disabled>Start</button>";
                html << "<button class='btn btn-stop' onclick='stopProcess(" << proc->id << ")'>Stop</button>";
            }
            else
            {
                html << "<button class='btn btn-start' onclick='startProcess(" << proc->id << ")'>Start</button>";
                html << "<button class='btn btn-stop' disabled>Stop</button>";
            }
            html << "<button class='btn' style='background-color: #ff9800; color: white;' onclick='removeProcess(" << proc->id << ")'>Remove</button>";
            html << "</td>";
            html << "</tr>";
        }
    }

    html << "</table>";
    html << "</div>";
    html << "</body></html>";

    return html.str();
}

std::string HttpServer::HandleApiRequest(const std::string& request)
{
    std::unordered_map<std::string, std::string> params;
    std::string response;

    size_t getPos = request.find("GET /api/");
    size_t postPos = request.find("POST /api/");

    if (getPos != std::string::npos)
    {
        size_t start = getPos + 9;
        size_t end = request.find(" ", start);
        size_t queryPos = request.find("?");

        std::string action;
        if (queryPos != std::string::npos && queryPos < end)
        {
            action = request.substr(start, queryPos - start);
            std::string queryString = request.substr(queryPos + 1, end - queryPos - 1);

            size_t ampPos = 0;
            while (ampPos != std::string::npos)
            {
                size_t nextAmp = queryString.find("&", ampPos);
                std::string pair;
                if (nextAmp != std::string::npos)
                {
                    pair = queryString.substr(ampPos, nextAmp - ampPos);
                    ampPos = nextAmp + 1;
                }
                else
                {
                    pair = queryString.substr(ampPos);
                    ampPos = std::string::npos;
                }

                size_t eqPos = pair.find("=");
                if (eqPos != std::string::npos)
                {
                    std::string key = pair.substr(0, eqPos);
                    std::string value = pair.substr(eqPos + 1);
                    params[key] = value;
                }
            }
        }
        else
        {
            action = request.substr(start, end - start);
        }

        ProcessManager& pm = ProcessManager::GetInstance();
        bool success = false;
        std::string message = "";

        if (action == "start")
        {
            if (params.find("id") != params.end())
            {
                int id = std::stoi(params["id"]);
                success = pm.StartProcess(id);
                message = success ? "Started" : "Failed to start";
            }
            else
            {
                message = "Missing parameter: id";
            }
        }
        else if (action == "stop")
        {
            if (params.find("id") != params.end())
            {
                int id = std::stoi(params["id"]);
                success = pm.StopProcess(id);
                message = success ? "Stopped" : "Failed to stop";
            }
            else
            {
                message = "Missing parameter: id";
            }
        }
        else if (action == "remove")
        {
            if (params.find("id") != params.end())
            {
                int id = std::stoi(params["id"]);
                success = pm.RemoveProcess(id);
                message = success ? "Removed" : "Failed to remove";
            }
            else
            {
                message = "Missing parameter: id";
            }
        }
        else if (action == "list")
        {
            success = true;
            message = "OK";
        }

        std::ostringstream json;
        json << "{\"success\":" << (success ? "true" : "false") << ",\"message\":\"" << message << "\"}";
        response = GetHttpResponse(json.str(), "application/json");
    }
    else if (postPos != std::string::npos)
    {
        size_t start = postPos + 10;
        size_t end = request.find(" ", start);
        std::string action = request.substr(start, end - start);

        size_t bodyPos = request.find("\r\n\r\n");
        std::string body;
        if (bodyPos != std::string::npos)
        {
            body = request.substr(bodyPos + 4);
        }

        ProcessManager& pm = ProcessManager::GetInstance();
        bool success = false;
        std::string message = "";

        if (action == "add")
        {
            std::wstring exePath;
            std::wstring arguments;

            size_t exePos = body.find("name=\"exe\"");
            if (exePos != std::string::npos)
            {
                size_t dataStart = body.find("\r\n\r\n", exePos);
                if (dataStart != std::string::npos)
                {
                    dataStart += 4;
                    size_t dataEnd = body.find("\r\n", dataStart);
                    if (dataEnd != std::string::npos)
                    {
                        std::string exeStr = body.substr(dataStart, dataEnd - dataStart);
                        exePath = std::wstring(exeStr.begin(), exeStr.end());
                    }
                }
            }

            size_t argsPos = body.find("name=\"args\"");
            if (argsPos != std::string::npos)
            {
                size_t dataStart = body.find("\r\n\r\n", argsPos);
                if (dataStart != std::string::npos)
                {
                    dataStart += 4;
                    size_t dataEnd = body.find("\r\n", dataStart);
                    if (dataEnd != std::string::npos)
                    {
                        std::string argsStr = body.substr(dataStart, dataEnd - dataStart);
                        arguments = std::wstring(argsStr.begin(), argsStr.end());
                    }
                }
            }

            if (!exePath.empty())
            {
                int id = pm.AddProcess(exePath, arguments);
                success = id > 0;
                message = success ? "Added" : "Failed to add";
            }
            else
            {
                message = "Missing program path";
            }
        }

        std::ostringstream json;
        json << "{\"success\":" << (success ? "true" : "false") << ",\"message\":\"" << message << "\"}";
        response = GetHttpResponse(json.str(), "application/json");
    }

    return response;
}

std::string HttpServer::EscapeHtml(const std::string& str)
{
    std::string result;
    for (char c : str)
    {
        switch (c)
        {
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '&': result += "&amp;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&#39;"; break;
        default: result += c; break;
        }
    }
    return result;
}

std::string HttpServer::WStringToString(const std::wstring& wstr)
{
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}
