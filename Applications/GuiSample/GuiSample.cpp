// Dear ImGui: standalone example application for Platform
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include <imgui.h>
#include "backends/imgui_impl_win32.h"
#include "Platform/ImGui/imgui_impl_platform.h"
#include "Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Platform/DirectX12/DeviceManager.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/Core/CommandLineParams.h"
#include "Platform/Core/Timer.h"
#include <tchar.h>
#ifdef _MSC_VER
#include "Platform/Windows/VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(true, NULL, 128);
#endif

using namespace simul;
// Data
dx12::DeviceManager deviceManager;
crossplatform::GraphicsDeviceInterface* graphicsDeviceInterface = &deviceManager;
crossplatform::DisplaySurfaceManager displaySurfaceManager;
platform::core::CommandLineParams commandLineParams;
simul::crossplatform::RenderPlatform* renderPlatform = nullptr;

// Forward declarations of helper functions
bool CreateDevice(HWND hWnd);
void CleanupDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


class PlatformRenderer:public crossplatform::PlatformRendererInterface
{
public:
    PlatformRenderer(crossplatform::RenderPlatform* r) :renderPlatform(r)
    {
    }
    //! Add a view. This tells the renderer to create any internal stuff it needs to handle a viewport, so that it is ready when Render() is called. It returns an identifier for that view.
    int					AddView()
    {
        return 0;
    }
    //! Remove the view. This might not have an immediate effect internally, but is a courtesy to the interface.
    void				RemoveView(int)
    {
    }
    //! For a view that has already been created, this ensures that it has the requested size and format.
    void				ResizeView(int view_id, int w, int h)
    {
    }
    //! Render the specified view. It's up to the renderer to decide what that means. The renderTexture is required because many API's don't allow querying of the current state.
    //! It will be assumed for simplicity that the viewport should be restored to the entire size of the renderTexture.
    void				Render(int view_id, void* context, void* renderTexture, int w, int h, long long framenumber, void* context_allocator)
    {
        simul::crossplatform::GraphicsDeviceContext	deviceContext;

        // Store back buffer, depth buffer and viewport information
        deviceContext.defaultTargetsAndViewport.num = 1;
        deviceContext.defaultTargetsAndViewport.m_rt[0] = renderTexture;
        deviceContext.defaultTargetsAndViewport.rtFormats[0] = crossplatform::UNKNOWN; //To be later defined in the pipeline
        deviceContext.defaultTargetsAndViewport.m_dt = nullptr;
        deviceContext.defaultTargetsAndViewport.depthFormat = crossplatform::UNKNOWN;
        deviceContext.defaultTargetsAndViewport.viewport = { 0,0,w,h };
        renderPlatform->BeginFrame( framenumber);
        deviceContext.platform_context = context;
        deviceContext.renderPlatform = renderPlatform;
        deviceContext.viewStruct.view_id = view_id;
        deviceContext.viewStruct.depthTextureStyle = crossplatform::PROJECTION;
        //deviceContext.viewStruct.Init();
        crossplatform::Viewport vp={0, 0, w, h};
        renderPlatform->SetViewports(deviceContext,1,&vp);

        // Pre-Render Update
        static platform::core::Timer timer;
        float real_time = timer.UpdateTimeSum() / 1000.0f;
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
       // renderPlatform->SetRenderTargets(1, &g_mainRenderTargetView, NULL);
        renderPlatform->Clear(deviceContext,clear_color_with_alpha);
        
        ImGui_ImplPlatform_RenderDrawData(deviceContext,ImGui::GetDrawData());
    }
    void				SetRenderDelegate(int /*view_id*/, crossplatform::RenderDelegate /*d*/)
    {
    }
    simul::crossplatform::RenderPlatform* renderPlatform = nullptr;
};
PlatformRenderer *platformRenderer=nullptr;

// Main code
#ifdef _MSC_VER
int APIENTRY WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR    lpCmdLine,
    int       nCmdShow)
#else
int main(int, char**)
#endif
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Dear ImGui Simul Platform Example"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDevice(hwnd))
    {
        CleanupDevice();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplPlatform_Init(renderPlatform);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplPlatform_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("A");               // Display some text (you can use a format strings too)
           ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        displaySurfaceManager.Render(hwnd);
        displaySurfaceManager.EndFrame();

        //g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }
    // Cleanup
    ImGui_ImplPlatform_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDevice();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDevice(HWND hWnd)
{
    renderPlatform = new dx12::RenderPlatform(); 
    platformRenderer = new PlatformRenderer(renderPlatform);
    graphicsDeviceInterface->Initialize(true, false, false);//commandLineParams("debug")

#define STRING_OF_MACRO1(x) #x
#define STRING_OF_MACRO(x) STRING_OF_MACRO1(x)
    // Shader binaries: we want to use a shared common directory under Simul/Media. But if we're running from some other place, we'll just create a "shaderbin" directory.
    std::string cmake_binary_dir = STRING_OF_MACRO(PLATFORM_BUILD_DIR);
    std::string cmake_source_dir = STRING_OF_MACRO(CMAKE_SOURCE_DIR);
    if (cmake_binary_dir.length())
    {
        platformRenderer->renderPlatform->PushShaderPath(((std::string(STRING_OF_MACRO(PLATFORM_SOURCE_DIR)) + "/") + platformRenderer->renderPlatform->GetPathName() + "/HLSL").c_str());
        platformRenderer->renderPlatform->PushShaderBinaryPath(((cmake_binary_dir + "/") + platformRenderer->renderPlatform->GetPathName() + "/shaderbin").c_str());
        std::string platform_build_path = ((cmake_binary_dir + "/Platform/") + platformRenderer->renderPlatform->GetPathName());
        platformRenderer->renderPlatform->PushShaderBinaryPath((platform_build_path + "/shaderbin").c_str());
        platformRenderer->renderPlatform->PushTexturePath((cmake_source_dir + "/Resources/Textures").c_str());
    }
    platformRenderer->renderPlatform->PushShaderBinaryPath((std::string("shaderbin/") + platformRenderer->renderPlatform->GetPathName()).c_str());
    platformRenderer->renderPlatform->RestoreDeviceObjects(graphicsDeviceInterface->GetDevice());
    displaySurfaceManager.Initialize(platformRenderer->renderPlatform);
    displaySurfaceManager.SetRenderer(hWnd, platformRenderer, -1);
    return true;
}

void CleanupDevice()
{
    delete platformRenderer;
    delete renderPlatform;
    displaySurfaceManager.Shutdown();
    deviceManager.Shutdown();
}


// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (hWnd != NULL && wParam != SIZE_MINIMIZED)
        {
            displaySurfaceManager.ResizeSwapChain(hWnd);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
