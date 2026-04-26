// mfc_demo.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "mfc_demo.h"
#include "ProcessManager.h"
#include "HttpServer.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

#define MAX_LOADSTRING 100
#define ID_TIMER_REFRESH 1

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

HWND hWndList;       // 列表控件句柄
HWND hWndEditExe;    // 程序路径编辑框
HWND hWndEditArgs;   // 启动参数编辑框
HWND hWndBtnAdd;     // 添加按钮
HWND hWndStaticInfo; // 信息显示

int g_nSelectedItem = -1; // 当前选中的列表项

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void CreateControls(HWND hWnd);
void ResizeControls(HWND hWnd);
void RefreshProcessList();
void ShowContextMenu(HWND hWnd, int x, int y);
int GetListSelectedProcessId();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MFCDEMO, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 启动HTTP服务器
    HttpServer& server = HttpServer::GetInstance();
    if (server.Start(8000))
    {
        OutputDebugStringW(L"HTTP服务器已启动，端口: 8000\n");
    }

    // 执行应用程序初始化:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MFCDEMO));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    server.Stop();

    return (int)msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MFCDEMO));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MFCDEMO);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 将实例句柄存储在全局变量中

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateControls(hWnd);
        SetTimer(hWnd, ID_TIMER_REFRESH, 1000, NULL);
        RefreshProcessList();
        break;

    case WM_SIZE:
        ResizeControls(hWnd);
        break;

    case WM_TIMER:
        if (wParam == ID_TIMER_REFRESH)
        {
            ProcessManager::GetInstance().UpdateProcessStatuses();
            RefreshProcessList();
        }
        break;

    case WM_CONTEXTMENU:
    {
        POINT pt;
        pt.x = LOWORD(lParam);
        pt.y = HIWORD(lParam);

        if ((HWND)wParam == hWndList)
        {
            LVHITTESTINFO hitTest;
            ScreenToClient(hWndList, &pt);
            hitTest.pt = pt;

            ListView_HitTest(hWndList, &hitTest);

            if (hitTest.flags & LVHT_ONITEM)
            {
                ListView_SetItemState(hWndList, hitTest.iItem, LVIS_SELECTED, LVIS_SELECTED);
                g_nSelectedItem = hitTest.iItem;
                ShowContextMenu(hWnd, LOWORD(lParam), HIWORD(lParam));
            }
        }
        break;
    }

    case WM_NOTIFY:
    {
        LPNMHDR pnmhdr = (LPNMHDR)lParam;
        if (pnmhdr->hwndFrom == hWndList)
        {
            switch (pnmhdr->code)
            {
            case LVN_ITEMCHANGED:
            {
                LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                if (pnmv->uNewState & LVIS_SELECTED)
                {
                    g_nSelectedItem = pnmv->iItem;
                }
                break;
            }
            }
        }
        break;
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // 分析菜单选择:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        case IDC_BTN_ADD:
        {
            int lenExe = GetWindowTextLengthW(hWndEditExe);
            int lenArgs = GetWindowTextLengthW(hWndEditArgs);

            if (lenExe > 0)
            {
                std::wstring exePath(lenExe + 1, L'\0');
                std::wstring arguments(lenArgs > 0 ? lenArgs + 1 : 0, L'\0');

                GetWindowTextW(hWndEditExe, &exePath[0], lenExe + 1);
                exePath.resize(lenExe);

                if (lenArgs > 0)
                {
                    GetWindowTextW(hWndEditArgs, &arguments[0], lenArgs + 1);
                    arguments.resize(lenArgs);
                }

                ProcessManager::GetInstance().AddProcess(exePath, arguments);
                RefreshProcessList();

                SetWindowTextW(hWndEditExe, L"");
                SetWindowTextW(hWndEditArgs, L"");
            }
            break;
        }
        case IDM_START_PROCESS:
        {
            int id = GetListSelectedProcessId();
            if (id > 0)
            {
                ProcessManager::GetInstance().StartProcess(id);
                RefreshProcessList();
            }
            break;
        }
        case IDM_STOP_PROCESS:
        {
            int id = GetListSelectedProcessId();
            if (id > 0)
            {
                ProcessManager::GetInstance().StopProcess(id);
                RefreshProcessList();
            }
            break;
        }
        case IDM_REMOVE_PROCESS:
        {
            int id = GetListSelectedProcessId();
            if (id > 0)
            {
                ProcessManager::GetInstance().RemoveProcess(id);
                RefreshProcessList();
            }
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        KillTimer(hWnd, ID_TIMER_REFRESH);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CreateControls(HWND hWnd)
{
    hWndStaticInfo = CreateWindowExW(0, L"STATIC",
        L"HTTP服务器已启动: http://localhost:8000",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        10, 10, 400, 20,
        hWnd, (HMENU)IDC_STATIC_INFO, hInst, NULL);

    HWND hStaticExe = CreateWindowExW(0, L"STATIC",
        L"程序路径:",
        WS_VISIBLE | WS_CHILD | SS_RIGHT,
        10, 40, 70, 23,
        hWnd, (HMENU)IDC_STATIC_EXE, hInst, NULL);

    hWndEditExe = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
        L"",
        WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
        85, 40, 500, 23,
        hWnd, (HMENU)IDC_EDIT_EXE, hInst, NULL);

    hWndBtnAdd = CreateWindowExW(0, L"BUTTON",
        L"添加",
        WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        595, 40, 70, 23,
        hWnd, (HMENU)IDC_BTN_ADD, hInst, NULL);

    HWND hStaticArgs = CreateWindowExW(0, L"STATIC",
        L"启动参数:",
        WS_VISIBLE | WS_CHILD | SS_RIGHT,
        10, 70, 70, 23,
        hWnd, (HMENU)IDC_STATIC_ARGS, hInst, NULL);

    hWndEditArgs = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
        L"",
        WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
        85, 70, 500, 23,
        hWnd, (HMENU)IDC_EDIT_ARGS, hInst, NULL);

    hWndList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEW,
        L"",
        WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        10, 100, 600, 300,
        hWnd, (HMENU)IDC_PROCESS_LIST, hInst, NULL);

    ListView_SetExtendedListViewStyle(hWndList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    WCHAR szCol1[] = L"程序路径";
    WCHAR szCol2[] = L"启动参数";
    WCHAR szCol3[] = L"运行状态";

    LVCOLUMN lvColumn;
    lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    lvColumn.fmt = LVCFMT_LEFT;

    lvColumn.cx = 250;
    lvColumn.pszText = szCol1;
    ListView_InsertColumn(hWndList, 0, &lvColumn);

    lvColumn.cx = 150;
    lvColumn.pszText = szCol2;
    ListView_InsertColumn(hWndList, 1, &lvColumn);

    lvColumn.cx = 80;
    lvColumn.pszText = szCol3;
    ListView_InsertColumn(hWndList, 2, &lvColumn);

    ResizeControls(hWnd);
}

void ResizeControls(HWND hWnd)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    int cx = rcClient.right - rcClient.left;
    int cy = rcClient.bottom - rcClient.top;

    MoveWindow(hWndStaticInfo, 10, 10, cx - 20, 20, TRUE);
    MoveWindow(hWndEditExe, 85, 40, cx - 260, 23, TRUE);
    MoveWindow(hWndBtnAdd, cx - 165, 40, 70, 23, TRUE);
    MoveWindow(hWndEditArgs, 85, 70, cx - 260, 23, TRUE);
    MoveWindow(hWndList, 10, 100, cx - 20, cy - 110, TRUE);

    ListView_SetColumnWidth(hWndList, 0, cx - 300);
    ListView_SetColumnWidth(hWndList, 1, 150);
    ListView_SetColumnWidth(hWndList, 2, 80);
}

void RefreshProcessList()
{
    ListView_DeleteAllItems(hWndList);

    ProcessManager& pm = ProcessManager::GetInstance();
    const auto& processes = pm.GetAllProcesses();

    int index = 0;
    for (const auto& proc : processes)
    {
        bool isRunning = pm.IsProcessRunning(proc->id);

        LVITEM lvItem;
        ZeroMemory(&lvItem, sizeof(lvItem));
        lvItem.mask = LVIF_TEXT | LVIF_PARAM;
        lvItem.iItem = index;
        lvItem.iSubItem = 0;
        lvItem.pszText = (LPWSTR)proc->exePath.c_str();
        lvItem.lParam = proc->id;

        ListView_InsertItem(hWndList, &lvItem);

        ListView_SetItemText(hWndList, index, 1, (LPWSTR)proc->arguments.c_str());
        ListView_SetItemText(hWndList, index, 2, (LPWSTR)(isRunning ? L"运行中" : L"已停止"));

        index++;
    }
}

void ShowContextMenu(HWND hWnd, int x, int y)
{
    HMENU hMenu = CreatePopupMenu();

    int id = GetListSelectedProcessId();
    bool isRunning = false;

    if (id > 0)
    {
        ProcessInfo* info = ProcessManager::GetInstance().GetProcess(id);
        if (info)
        {
            isRunning = ProcessManager::GetInstance().IsProcessRunning(id);
        }
    }

    AppendMenuW(hMenu, MF_STRING | (isRunning ? MF_GRAYED : 0), IDM_START_PROCESS, L"启动");
    AppendMenuW(hMenu, MF_STRING | (!isRunning ? MF_GRAYED : 0), IDM_STOP_PROCESS, L"停止");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_REMOVE_PROCESS, L"删除");

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
        x, y, 0, hWnd, NULL);

    DestroyMenu(hMenu);
}

int GetListSelectedProcessId()
{
    int selected = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
    if (selected >= 0)
    {
        LVITEM lvItem;
        ZeroMemory(&lvItem, sizeof(lvItem));
        lvItem.mask = LVIF_PARAM;
        lvItem.iItem = selected;
        ListView_GetItem(hWndList, &lvItem);
        return (int)lvItem.lParam;
    }
    return -1;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
