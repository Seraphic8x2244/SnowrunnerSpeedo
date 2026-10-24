#include <Windows.h>
#include <algorithm>
#include <thread>
#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <sstream>
#include <fstream>
#include <atomic>
#include <vector>      
#include <map>         
#include <Psapi.h>     
#include <filesystem>  
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <iomanip>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Psapi.lib") 

// ----------------------------
// Globals
// ----------------------------
typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain*, UINT, UINT);
PresentFn oPresent = nullptr;

ID3D11Device* g_Device = nullptr;
ID3D11DeviceContext* g_Context = nullptr;
ID3D11RenderTargetView* g_RTV = nullptr;

std::atomic<bool> g_ImGuiInitialized(false);

// HUD
uintptr_t g_SpeedAddress = 0x2A8FA2C;

// HUD state
bool g_ShowSpeed = true;
bool g_ShowDistance = true;
bool g_ShowDecimalSpeed = false;  // false = integer, true = decimal
float g_DistanceMeters = 0.0f;

// Menu and scale
bool g_ShowMenu = false;
float g_HudFontScale = 3.0f;
float g_MenuFontScale = 3.0f;
ImVec2 g_HudPos = ImVec2(10, 10);
bool g_HudPosInitialized = false;

// INI
std::string g_IniPath;
std::string g_DllDirectory;  // Added to store DLL directory
std::string g_FontFile;      // Font file name from config

// Cached module handle
HMODULE g_GameModule = nullptr;

// Input debouncing - track previous key states
std::map<int, bool> g_PrevKeyStates;

// ----------------------------
// UTILITY FUNCTIONS
// ----------------------------
inline void CreateRenderTarget(IDXGISwapChain* swapChain)
{
    if (g_RTV)
    {
        g_RTV->Release();
        g_RTV = nullptr;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
    {
        g_Device->CreateRenderTargetView(backBuffer, nullptr, &g_RTV);
        backBuffer->Release();
    }
}

inline void ClampHudPosition(ImVec2& pos, const ImVec2& textSize, const ImVec2& screenSize)
{
    pos.x = std::max(0.0f, std::min(pos.x, screenSize.x - textSize.x));
    pos.y = std::max(0.0f, std::min(pos.y, screenSize.y - textSize.y));
}

std::vector<BYTE> ParseHexBytes(const std::string& hexStr)
{
    std::vector<BYTE> result;
    std::istringstream iss(hexStr);
    std::string byteStr;
    while (iss >> byteStr)
    {
        if (byteStr == "??" || byteStr == "?")
            result.push_back(0);
        else
            result.push_back(static_cast<BYTE>(std::stoul(byteStr, nullptr, 16)));
    }
    return result;
}

std::string BytesToHexString(const std::vector<BYTE>& bytes)
{
    std::ostringstream oss;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if (i > 0) oss << " ";
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

void CreateDefaultConfig()
{
    std::ofstream ini(g_IniPath);
    if (ini.is_open())
    {
        ini << "[HUD]\n"
            "PosX=10\n"
            "PosY=10\n"
            "Scale=3.0\n"
            "ShowSpeed=1\n"
            "ShowDistance=1\n"
            "ShowDecimalSpeed=0\n"
            "FontFile=\n"
            "\n"
            "[ADDRESS]\n"
            "SpeedAddress=0x2A8FA2C\n"
            ;
    }
}

void LoadConfig()
{
    std::ifstream f(g_IniPath);
    if (!f.good())
        CreateDefaultConfig();

    char buffer[512];

    GetPrivateProfileStringA("HUD", "PosX", "10", buffer, sizeof(buffer), g_IniPath.c_str());
    g_HudPos.x = static_cast<float>(atof(buffer));

    GetPrivateProfileStringA("HUD", "PosY", "10", buffer, sizeof(buffer), g_IniPath.c_str());
    g_HudPos.y = static_cast<float>(atof(buffer));

    GetPrivateProfileStringA("HUD", "Scale", "3.0", buffer, sizeof(buffer), g_IniPath.c_str());
    g_HudFontScale = static_cast<float>(atof(buffer));

    GetPrivateProfileStringA("HUD", "ShowSpeed", "1", buffer, sizeof(buffer), g_IniPath.c_str());
    g_ShowSpeed = (buffer[0] == '1');

    GetPrivateProfileStringA("HUD", "ShowDistance", "1", buffer, sizeof(buffer), g_IniPath.c_str());
    g_ShowDistance = (buffer[0] == '1');

    GetPrivateProfileStringA("HUD", "ShowDecimalSpeed", "0", buffer, sizeof(buffer), g_IniPath.c_str());
    g_ShowDecimalSpeed = (buffer[0] == '1');

    GetPrivateProfileStringA("HUD", "FontFile", "", buffer, sizeof(buffer), g_IniPath.c_str());
    g_FontFile = buffer;

    GetPrivateProfileStringA("ADDRESS", "SpeedAddress", "0x2A8FA2C", buffer, sizeof(buffer), g_IniPath.c_str());
    g_SpeedAddress = static_cast<uintptr_t>(strtoull(buffer, nullptr, 0));

}

void SaveConfig()
{
    char buffer[32];

    snprintf(buffer, sizeof(buffer), "%.1f", g_HudPos.x);
    WritePrivateProfileStringA("HUD", "PosX", buffer, g_IniPath.c_str());

    snprintf(buffer, sizeof(buffer), "%.1f", g_HudPos.y);
    WritePrivateProfileStringA("HUD", "PosY", buffer, g_IniPath.c_str());

    snprintf(buffer, sizeof(buffer), "%.2f", g_HudFontScale);
    WritePrivateProfileStringA("HUD", "Scale", buffer, g_IniPath.c_str());

    WritePrivateProfileStringA("HUD", "ShowSpeed", g_ShowSpeed ? "1" : "0", g_IniPath.c_str());
    WritePrivateProfileStringA("HUD", "ShowDistance", g_ShowDistance ? "1" : "0", g_IniPath.c_str());
    WritePrivateProfileStringA("HUD", "ShowDecimalSpeed", g_ShowDecimalSpeed ? "1" : "0", g_IniPath.c_str());

    snprintf(buffer, sizeof(buffer), "0x%llx", (unsigned long long)g_SpeedAddress);
    WritePrivateProfileStringA("ADDRESS", "SpeedAddress", buffer, g_IniPath.c_str());

}

inline bool HandleDebouncedInput(int vkey)
{
    bool currentState = (GetAsyncKeyState(vkey) & 0x8000) != 0;
    bool prevState = g_PrevKeyStates[vkey];
    g_PrevKeyStates[vkey] = currentState;

    // Return true only on the down edge (key just pressed, not held)
    return currentState && !prevState;
}

// ----------------------------
// FONT LOADING FUNCTIONS
// ----------------------------

// Find TTF file based on config or search directory
std::string FindTTFFile(const std::string& directory)
{
    namespace fs = std::filesystem;

    // Create path to speedfonts folder
    std::string fontDir = directory + "\\speedfonts";

    // First, check if a specific font is specified in config
    if (!g_FontFile.empty())
    {
        // Build full path in speedfonts folder
        std::string fullPath = fontDir + "\\" + g_FontFile;

        try
        {
            if (fs::exists(fullPath) && fs::is_regular_file(fullPath))
            {
                std::string extension = fs::path(fullPath).extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                if (extension == ".ttf")
                {
                    return fullPath;
                }
            }
        }
        catch (...)
        {
            // If check fails, fall through to auto-detection
        }
    }

    // If no font specified in config or file doesn't exist, search for any .ttf file in speedfonts folder
    try
    {
        // Check if speedfonts directory exists
        if (!fs::exists(fontDir) || !fs::is_directory(fontDir))
        {
            return "";  // No speedfonts folder, return empty
        }

        for (const auto& entry : fs::directory_iterator(fontDir))
        {
            if (entry.is_regular_file())
            {
                std::string extension = entry.path().extension().string();
                // Convert extension to lowercase for case-insensitive comparison
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                if (extension == ".ttf")
                {
                    return entry.path().string();
                }
            }
        }
    }
    catch (...)
    {
        // If directory iteration fails, return empty string
    }

    return "";
}

// Load custom font or default with anti-aliasing
void LoadImGuiFont()
{
    ImGuiIO& io = ImGui::GetIO();

    // Try to find a .ttf file in the DLL directory
    std::string ttfPath = FindTTFFile(g_DllDirectory);

    if (!ttfPath.empty())
    {
        // Load custom TTF font
        // Using a reasonable default size, you can adjust this
        ImFont* font = io.Fonts->AddFontFromFileTTF(ttfPath.c_str(), 16.0f);

        if (font != nullptr)
        {
            // Font loaded successfully
            io.Fonts->Build();
            return;
        }
        // If loading failed, fall through to default font
    }

    // Use default font with anti-aliasing (smoothened)
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 3;  // Horizontal oversampling for smoother fonts
    fontConfig.OversampleV = 2;  // Vertical oversampling
    fontConfig.PixelSnapH = false;  // Don't snap to pixel grid for smoother appearance

    io.Fonts->AddFontDefault(&fontConfig);
    io.Fonts->Build();
}

// ----------------------------
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HRESULT __stdcall hkPresent(IDXGISwapChain* swapChain, UINT sync, UINT flags)
{
    if (!g_ImGuiInitialized.load(std::memory_order_acquire))
    {
        if (SUCCEEDED(swapChain->GetDevice(IID_PPV_ARGS(&g_Device))))
        {
            g_Device->GetImmediateContext(&g_Context);

            DXGI_SWAP_CHAIN_DESC sd;
            swapChain->GetDesc(&sd);

            CreateRenderTarget(swapChain);

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

            ImGui_ImplWin32_Init(sd.OutputWindow);
            ImGui_ImplDX11_Init(g_Device, g_Context);

            g_GameModule = GetModuleHandleA(nullptr);

            // Get DLL directory and INI path
            char dllPath[MAX_PATH];
            HMODULE hModule = nullptr;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCSTR)&hkPresent, &hModule);
            GetModuleFileNameA(hModule, dllPath, MAX_PATH);

            std::string path(dllPath);
            size_t pos = path.find_last_of("\\/");
            if (pos != std::string::npos)
            {
                g_DllDirectory = path.substr(0, pos);
                g_IniPath = g_DllDirectory + "\\speedhud.ini";
            }

            // Load config first to get font file setting
            LoadConfig();

            // Load custom font or default smoothened font
            LoadImGuiFont();

            g_ImGuiInitialized.store(true, std::memory_order_release);
        }
    }

    if (g_ImGuiInitialized.load(std::memory_order_acquire))
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGuiIO& io = ImGui::GetIO();

        // Cached pointer calculation
        static float* speedPtr = reinterpret_cast<float*>((uintptr_t)g_GameModule + g_SpeedAddress);
        float speedKmh = *speedPtr;

        // Distance update
        float speedMps = speedKmh * 0.277778f;
        g_DistanceMeters += speedMps * io.DeltaTime;

        // Analogue needle speedometer; existing Home controls position and scale.
        ImVec2 textSize(0, 0);
        if (g_ShowSpeed || g_ShowDistance)
        {
            const float scale = std::max(0.1f, g_HudFontScale / 3.0f);
            const float radius = 75.0f * scale;
            const float bottomText = g_ShowDistance ? 22.0f * scale : 0.0f;
            const ImVec2 extent(radius * 2.0f, radius * 2.0f + bottomText);
            ImGui::SetNextWindowPos(g_HudPos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(extent, ImGuiCond_Always);
            ImGui::Begin("##Speedo", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoBackground);
            const ImVec2 origin = ImGui::GetWindowPos();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 center(origin.x + radius, origin.y + radius);
            const ImU32 face = IM_COL32(30, 33, 39, 175);
            const ImU32 ink = IM_COL32(235, 235, 235, 245);
            const ImU32 needle = IM_COL32(225, 65, 60, 255);
            const float pi = 3.14159265359f;
            const float begin = 0.7f * pi;
            const float sweep = 1.6f * pi;
            if (g_ShowSpeed)
            {
                draw->AddCircleFilled(center, radius, face, 64);
                draw->AddCircle(center, radius - scale, ink, 64, std::max(1.0f, 1.5f * scale));
                for (int n = 0; n <= 10; ++n)
                {
                    float a = begin + sweep * (float(n) / 10.0f);
                    float ca = cosf(a), sa = sinf(a);
                    draw->AddLine(ImVec2(center.x + ca * radius * 0.78f, center.y + sa * radius * 0.78f),
                                  ImVec2(center.x + ca * radius * 0.9f, center.y + sa * radius * 0.9f),
                                  ink, std::max(1.0f, scale));
                }
                // Speed telemetry remains the verified v44 float from SpeedHud.
                float ratio = std::clamp(speedKmh / 150.0f, 0.0f, 1.0f);
                float a = begin + sweep * ratio;
                draw->AddLine(center,
                    ImVec2(center.x + cosf(a) * radius * 0.68f, center.y + sinf(a) * radius * 0.68f),
                    needle, std::max(2.0f, scale * 2.5f));
                draw->AddCircleFilled(center, std::max(2.0f, 3.0f * scale), needle);
                char speedText[32];
                snprintf(speedText, sizeof(speedText), g_ShowDecimalSpeed ? "%.1f" : "%.0f", speedKmh);
                ImGui::SetWindowFontScale(scale);
                ImVec2 size = ImGui::CalcTextSize(speedText);
                draw->AddText(ImVec2(center.x - size.x * 0.5f, center.y + radius * 0.27f), ink, speedText);
                const char* unit = "km/h";
                size = ImGui::CalcTextSize(unit);
                draw->AddText(ImVec2(center.x - size.x * 0.5f, center.y + radius * 0.49f), ink, unit);
            }
            if (g_ShowDistance)
            {
                char dist[40];
                snprintf(dist, sizeof(dist), "%.0f m", g_DistanceMeters);
                ImGui::SetWindowFontScale(scale);
                ImVec2 size = ImGui::CalcTextSize(dist);
                draw->AddText(ImVec2(center.x - size.x * 0.5f, origin.y + radius * 2.0f), ink, dist);
            }
            ImGui::End();
            textSize = extent;
        }

        // Menu toggle
        static bool wasMenuOpen = false;
        if (GetAsyncKeyState(VK_HOME) & 1)
            g_ShowMenu = !g_ShowMenu;

        if (wasMenuOpen && !g_ShowMenu)
            SaveConfig();
        wasMenuOpen = g_ShowMenu;

        // HUD Settings Menu
        if (g_ShowMenu)
        {
            float oldScale = io.FontGlobalScale;
            io.FontGlobalScale = g_MenuFontScale;

            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Once, ImVec2(0.5f, 0.5f));

            ImGui::Begin("Settings", &g_ShowMenu,
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoCollapse);

            // Check which keys are currently pressed
            bool arrowPressed = (GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000) ||
                (GetAsyncKeyState(VK_UP) & 0x8000) || (GetAsyncKeyState(VK_DOWN) & 0x8000);
            bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool plusPressed = (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000) || (GetAsyncKeyState(VK_ADD) & 0x8000);
            bool minusPressed = (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) || (GetAsyncKeyState(VK_SUBTRACT) & 0x8000);
            bool key0Pressed = (GetAsyncKeyState('0') & 0x8000);
            bool key9Pressed = (GetAsyncKeyState('9') & 0x8000);
            bool key8Pressed = (GetAsyncKeyState('8') & 0x8000);
            bool key7Pressed = (GetAsyncKeyState('7') & 0x8000);

            ImVec4 activeColor = ImVec4(0.0f, 1.0f, 0.5f, 1.0f);  // Green when active
            ImVec4 normalColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White normally

            ImGui::SeparatorText("HUD Controls");

            // Arrow keys
            if (arrowPressed && !ctrlPressed)
                ImGui::TextColored(activeColor, "Arrow keys: move HUD");
            else
                ImGui::TextUnformatted("Arrow keys: move HUD");

            // Ctrl +/-
            if (ctrlPressed && (plusPressed || minusPressed))
                ImGui::TextColored(activeColor, "Ctrl +/-: scale HUD");
            else
                ImGui::TextUnformatted("Ctrl +/-: scale HUD");

            // Ctrl + 0
            if (ctrlPressed && key0Pressed)
                ImGui::TextColored(activeColor, "Ctrl + 0 : Toggle speed");
            else
                ImGui::TextUnformatted("Ctrl + 0 : Toggle speed");

            // Ctrl + 9
            if (ctrlPressed && key9Pressed)
                ImGui::TextColored(activeColor, "Ctrl + 9 : Toggle distance");
            else
                ImGui::TextUnformatted("Ctrl + 9 : Toggle distance");

            // Ctrl + 8
            if (ctrlPressed && key8Pressed)
                ImGui::TextColored(activeColor, "Ctrl + 8 : Reset distance");
            else
                ImGui::TextUnformatted("Ctrl + 8 : Reset distance");

            // Ctrl + 7
            if (ctrlPressed && key7Pressed)
                ImGui::TextColored(activeColor, "Ctrl + 7 : Toggle speed format");
            else
                ImGui::TextUnformatted("Ctrl + 7 : Toggle speed format");

            ImGui::Spacing();
            // Status display
            ImGui::Spacing();
            ImGui::SeparatorText("Status");
            ImGui::Text("Speed Format: %s", g_ShowDecimalSpeed ? "Decimal (25.4 km/h)" : "Integer (25 km/h)");
            ImGui::End();
            io.FontGlobalScale = oldScale;

            // Input handling (only works when menu is open)
            if (!ctrlPressed)
            {
                // Movement controls
                if (GetAsyncKeyState(VK_LEFT) & 0x8000)  g_HudPos.x -= 200.0f * io.DeltaTime;
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g_HudPos.x += 200.0f * io.DeltaTime;
                if (GetAsyncKeyState(VK_UP) & 0x8000)    g_HudPos.y -= 200.0f * io.DeltaTime;
                if (GetAsyncKeyState(VK_DOWN) & 0x8000)  g_HudPos.y += 200.0f * io.DeltaTime;

                ClampHudPosition(g_HudPos, textSize, io.DisplaySize);
            }

            if (ctrlPressed)
            {
                if (HandleDebouncedInput('0')) g_ShowSpeed = !g_ShowSpeed;
                if (HandleDebouncedInput('9')) g_ShowDistance = !g_ShowDistance;
                if (HandleDebouncedInput('8')) g_DistanceMeters = 0.0f;
                if (HandleDebouncedInput('7')) g_ShowDecimalSpeed = !g_ShowDecimalSpeed;

                // Smooth scaling like arrow keys
                if (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000 || GetAsyncKeyState(VK_ADD) & 0x8000)
                    g_HudFontScale = std::min(g_HudFontScale + 2.0f * io.DeltaTime, 10.0f);

                if (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000 || GetAsyncKeyState(VK_SUBTRACT) & 0x8000)
                    g_HudFontScale = std::max(g_HudFontScale - 2.0f * io.DeltaTime, 0.1f);
            }
        }

        ImGui::Render();
        g_Context->OMSetRenderTargets(1, &g_RTV, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return oPresent(swapChain, sync, flags);
}

// ----------------------------
DWORD WINAPI InitHook(LPVOID)
{
    constexpr DXGI_SWAP_CHAIN_DESC sd = {
        .BufferDesc = {.Format = DXGI_FORMAT_R8G8B8A8_UNORM },
        .SampleDesc = {.Count = 1 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 1,
        .OutputWindow = nullptr,
        .Windowed = TRUE
    };

    DXGI_SWAP_CHAIN_DESC sdCopy = sd;
    sdCopy.OutputWindow = GetForegroundWindow();

    IDXGISwapChain* swapChain = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sdCopy, &swapChain, &device, nullptr, &context);

    if (SUCCEEDED(hr))
    {
        void** vtable = *reinterpret_cast<void***>(swapChain);

        if (MH_Initialize() == MH_OK)
        {
            MH_CreateHook(vtable[8], &hkPresent, reinterpret_cast<void**>(&oPresent));
            MH_EnableHook(vtable[8]);
        }

        context->Release();
        device->Release();
        swapChain->Release();
    }
    return 0;
}

// ----------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, InitHook, nullptr, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {

        // Cleanup
        if (g_ImGuiInitialized.load())
        {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }

        if (g_RTV) g_RTV->Release();
        if (g_Context) g_Context->Release();
        if (g_Device) g_Device->Release();

        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
    return TRUE;
}