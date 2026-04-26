#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

class HttpServer
{
public:
    static HttpServer& GetInstance();

    bool Start(int port = 8000);
    void Stop();
    bool IsRunning() const { return m_isRunning; }
    int GetPort() const { return m_port; }

private:
    HttpServer();
    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void ServerThread();
    void HandleClient(SOCKET clientSocket);
    std::string GetHttpRequest(const std::string& request);
    std::string GetHttpResponse(const std::string& content, const std::string& contentType = "text/html");
    std::string GenerateProcessListHtml();
    std::string HandleApiRequest(const std::string& request);
    std::string EscapeHtml(const std::string& str);
    std::string WStringToString(const std::wstring& wstr);

    SOCKET m_serverSocket;
    int m_port;
    std::atomic<bool> m_isRunning;
    std::thread m_serverThread;
};
