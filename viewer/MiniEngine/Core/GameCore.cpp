//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#include "pch.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "SystemTime.h"
#include "GameInput.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "PostEffects.h"

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    #pragma comment(lib, "runtimeobject.lib")
#else
    #include <agile.h>
    using namespace Windows::ApplicationModel;
    using namespace Windows::UI::Core;
    using namespace Windows::UI::ViewManagement;
    using Windows::ApplicationModel::Core::CoreApplication;
    using Windows::ApplicationModel::Core::CoreApplicationView;
    using Windows::ApplicationModel::Activation::IActivatedEventArgs;
    using Windows::Foundation::TypedEventHandler;
#endif

namespace Graphics
{
    extern ColorBuffer g_GenMipsBuffer;
    extern uint32_t g_NativeWidth;
    extern uint32_t g_NativeHeight;
}

namespace GameCore
{
    using namespace Graphics;
    const bool TestGenerateMips = false;
    bool g_computeOnly = false;

    void InitializeApplication( IGameApp& app )
    {
        Graphics::Initialize();
        SystemTime::Initialize();
        GameInput::Initialize();
        EngineTuning::Initialize();

        app.Startup();
    }

    void TerminateApplication( IGameApp& app )
    {
        Graphics::Terminate();
        app.Cleanup();

        GameInput::Shutdown();
        Graphics::Shutdown();
    }

    bool UpdateApplication( IGameApp& app )
    {
        if (!g_computeOnly) EngineProfiling::Update();

        float DeltaTime = Graphics::GetFrameTime();
    
        if (!g_computeOnly)
        {
            GameInput::Update(DeltaTime);
            EngineTuning::Update(DeltaTime);
        }

        app.Update(DeltaTime);
        app.RenderScene();

        if (!g_computeOnly)
        {
            PostEffects::Render();

            if (TestGenerateMips)
            {
                GraphicsContext& MipsContext = GraphicsContext::Begin();

                // Exclude from timings this copy necessary to setup the test
                MipsContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
                MipsContext.TransitionResource(g_GenMipsBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
                MipsContext.CopySubresource(g_GenMipsBuffer, 0, g_SceneColorBuffer, 0);

                EngineProfiling::BeginBlock(L"GenerateMipMaps()", &MipsContext);
                g_GenMipsBuffer.GenerateMipMaps(MipsContext);
                EngineProfiling::EndBlock(&MipsContext);

                MipsContext.Finish();
            }

            GraphicsContext& UiContext = GraphicsContext::Begin(L"Render UI");
            UiContext.TransitionResource(g_OverlayBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
            UiContext.ClearColor(g_OverlayBuffer);
            UiContext.SetRenderTarget(g_OverlayBuffer.GetRTV());
            UiContext.SetViewportAndScissor(0, 0, g_OverlayBuffer.GetWidth(), g_OverlayBuffer.GetHeight());
            app.RenderUI(UiContext);

            EngineTuning::Display( UiContext, 0.0f, 40.0f, 1900.0f, 1040.0f );

            UiContext.Finish();
        }

        Graphics::Present();

        return !app.IsDone();
    }

    // Default implementation to be overridden by the application
    bool IGameApp::IsDone( void )
    {
        return GameInput::IsFirstPressed(GameInput::kKey_escape);
    }

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    IGameApp* m_game;
    Platform::Agile<CoreWindow> g_window;

    ref class MyApplicationView sealed : public Core::IFrameworkView
    {
    public:
        MyApplicationView() {}

        // IFrameworkView Methods.
        virtual void Initialize(CoreApplicationView^ applicationView);
        virtual void Load(Platform::String^ entryPoint);
        virtual void Run(void);
        virtual void SetWindow(CoreWindow^ window);
        virtual void Uninitialize(void);

    protected:
        // Event Handlers.
        void OnActivated(Core::CoreApplicationView^ applicationView, Activation::IActivatedEventArgs^ args);
        void OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args);
        void OnResuming(Platform::Object^ sender, Platform::Object^ args);
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_TV_TITLE)
        void OnWindowSizeChanged(CoreWindow^ sender, WindowSizeChangedEventArgs^ args);
        void OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args);
        void OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args);
        void OnPointerPressed(CoreWindow^ sender, PointerEventArgs^ args);
        void OnPointerMoved(CoreWindow^ sender, PointerEventArgs^ args);
        void OnKeyDown(CoreWindow^ sender, KeyEventArgs^ args);
        void OnKeyUp(CoreWindow^ sender, KeyEventArgs^ args);
#endif

    private:
        bool m_windowClosed;
        bool m_windowVisible;
        volatile bool m_IsRunning;
        volatile bool m_IsCapturingPointer;
        float m_PointerX, m_PointerY;
    };

    ref class ApplicationViewSource sealed : Core::IFrameworkViewSource
    {
    public:
        virtual Core::IFrameworkView^ CreateView()
        {
            return ref new MyApplicationView();
        }
    };


    // Called by the system.  Perform application initialization here, hooking application wide events, etc.
    void MyApplicationView::Initialize(CoreApplicationView^ applicationView)
    {
        applicationView->Activated += ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &MyApplicationView::OnActivated);
    }

    // Called when we are provided a window.
    void MyApplicationView::SetWindow(CoreWindow^ window)
    {
        // We record the window pointer now, but you can also call this function to retrieve it:
        //     CoreWindow::GetForCurrentThread()
        g_window = window;

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_TV_TITLE)
        window->SizeChanged += ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &MyApplicationView::OnWindowSizeChanged);
        window->VisibilityChanged += ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &MyApplicationView::OnVisibilityChanged);
        window->Closed += ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &MyApplicationView::OnWindowClosed);
        window->KeyDown += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &MyApplicationView::OnKeyDown);
        window->KeyUp += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &MyApplicationView::OnKeyUp);
        window->PointerPressed += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &MyApplicationView::OnPointerPressed);
        window->PointerMoved += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &MyApplicationView::OnPointerMoved);
#endif
    }

    void MyApplicationView::Load(Platform::String^ entryPoint)
    {
        InitializeApplication(*m_game);
    }

    // Called by the system after initialization is complete.  This implements the traditional game loop.
    void MyApplicationView::Run()
    {
        while (m_IsRunning)
        {
            // ProcessEvents will throw if the process is exiting, allowing us to break out of the loop.  This will be
            // cleaned up when we get proper process lifetime management in a future release.
            g_window->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

            m_IsRunning = UpdateApplication(*m_game);
        }
    }

    void MyApplicationView::Uninitialize()
    {
        Graphics::Terminate();
        TerminateApplication(*m_game);
        Graphics::Shutdown();
    }

    // Called when the application is activated.  For now, there is just one activation kind - Launch.
    void MyApplicationView::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
    {
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_TV_TITLE)
        float DpiScale = 96.0f / Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi;
        ApplicationView::PreferredLaunchWindowingMode = ApplicationViewWindowingMode::PreferredLaunchViewSize;
        Windows::Foundation::Size DesiredSize(Graphics::g_DisplayWidth * DpiScale, Graphics::g_DisplayHeight * DpiScale);
        ApplicationView::PreferredLaunchViewSize = DesiredSize;
        ApplicationView::GetForCurrentView()->TryResizeView(DesiredSize);
#endif

        m_IsRunning = true;
        m_IsCapturingPointer = false;
        g_window->Activate();
    }

    void MyApplicationView::OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args) {}
    void MyApplicationView::OnResuming(Platform::Object^ sender, Platform::Object^ args) {}

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_TV_TITLE)
    void MyApplicationView::OnWindowSizeChanged(CoreWindow^ sender, WindowSizeChangedEventArgs^ args)
    {
        Graphics::Resize((uint32_t)sender->Bounds.Width, (uint32_t)sender->Bounds.Height);
    }

    void MyApplicationView::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args) 
    {
        m_IsRunning = false;
    }

    void MyApplicationView::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args) {}

    void MyApplicationView::OnPointerPressed(CoreWindow^ sender, PointerEventArgs^ args)
    {
        //DEBUGPRINT("Pointer pressed (%f, %f)", args->CurrentPoint->RawPosition.X, args->CurrentPoint->RawPosition.Y);
        if (m_IsCapturingPointer)
        {
            g_window->ReleasePointerCapture();
            m_IsCapturingPointer = false;
            g_window->PointerCursor = ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::Arrow, 0);
            //DEBUGPRINT("Pointer released");
        }
        else
        {
            g_window->SetPointerCapture();
            m_IsCapturingPointer = true;
            g_window->PointerCursor = ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::Hand, 0);
            m_PointerX = args->CurrentPoint->RawPosition.X;
            m_PointerY = args->CurrentPoint->RawPosition.Y;
            //DEBUGPRINT("Pointer captured");
        }
    }
    
    void MyApplicationView::OnPointerMoved(CoreWindow^ sender, PointerEventArgs^ args)
    {
        if (!m_IsCapturingPointer)
            return;

        float OldX = m_PointerX;
        float OldY = m_PointerY;

        m_PointerX = args->CurrentPoint->RawPosition.X;
        m_PointerY = args->CurrentPoint->RawPosition.Y;

        //DEBUGPRINT("Pointer moved (%f, %f)", m_PointerX, m_PointerY);
        //DEBUGPRINT("Pointer was (%f, %f)", OldX, OldY);
        //GameInput::SetMouseMovement(m_PointerX)
    }

    void MyApplicationView::OnKeyDown(CoreWindow^ sender, KeyEventArgs^ args)
    {
        GameInput::SetKeyState(args->VirtualKey, true);
    }

    void MyApplicationView::OnKeyUp(CoreWindow^ sender, KeyEventArgs^ args)
    {
        GameInput::SetKeyState(args->VirtualKey, false);
    }
#endif

    void RunApplication( IGameApp& app, const wchar_t* className )
    {
        m_game = &app;
        (void)className;
        auto applicationViewSource = ref new ApplicationViewSource();
        CoreApplication::Run(applicationViewSource);
    }

#else // Win32

    HWND g_hWnd = nullptr;

    void InitWindow( const wchar_t* className );
    LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );

    void RunApplication( IGameApp& app, const wchar_t* className, int icon )
    {
        //ASSERT_SUCCEEDED(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED));
        Microsoft::WRL::Wrappers::RoInitializeWrapper InitializeWinRT(RO_INIT_MULTITHREADED);
        ASSERT_SUCCEEDED(InitializeWinRT);

        HINSTANCE hInst = GetModuleHandle(0);

        // Register class
        WNDCLASSEX wcex;
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInst;
        wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCEW(icon));
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = className;
        wcex.hIconSm = LoadIcon(hInst, IDI_APPLICATION);
        ASSERT(0 != RegisterClassEx(&wcex), "Unable to register a window");

        // Create window
        RECT rc = { 0, 0, (LONG)g_DisplayWidth, (LONG)g_DisplayHeight };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        g_hWnd = CreateWindow(className, className, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

        ASSERT(g_hWnd != 0);

        InitializeApplication(app);
        if (g_computeOnly) return;

        if (g_Device)
        {
            ShowWindow( g_hWnd, SW_SHOWDEFAULT );
            do
            {
                MSG msg = {};
                while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                if (msg.message == WM_QUIT)
                    break;
            }
            while (UpdateApplication(app));    // Returns false to quit loop
        }

        TerminateApplication(app);
    }

    //--------------------------------------------------------------------------------------
    // Called every time the application receives a message
    //--------------------------------------------------------------------------------------
    static bool g_resizing = false;

    LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
    {
        switch( message )
        {
            case WM_ENTERSIZEMOVE:
            {
                g_resizing = true;
                break;
            }
            case WM_EXITSIZEMOVE:
            {
                g_resizing = false;
                //force aspect ratio and resize window again
                RECT rc;
                GetClientRect(hWnd, &rc);
                float aspect = g_NativeWidth / (float)g_NativeHeight;
                int width = rc.right - rc.left;
                int height = rc.bottom - rc.top;
                width = width > (int)g_DisplayWidth || height > (int)g_DisplayHeight ? std::max(width, height) : std::min(width, height);
                height = (int)(width * aspect);
                rc = { 0, 0, width, height };
                AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
                SetWindowPos(hWnd, 0, 0, 0, rc.right-rc.left, rc.bottom-rc.top, SWP_NOMOVE | SWP_NOZORDER);
                Graphics::Resize(width, height);
                GameInput::UnaquireKbm(true);
                break;
            }
            case WM_LBUTTONDOWN:
            {
                GameInput::g_waitForClick = false;
                break;
            }
            case WM_SIZE:
            {
                if (g_resizing) break;
                //force aspect ratio and resize window again
                float aspect = g_NativeWidth / (float)g_NativeHeight;
                int width = (UINT)(UINT64)lParam & 0xFFFF;
                int height = (UINT)(UINT64)lParam >> 16;
                width = width > (int)g_DisplayWidth || height > (int)g_DisplayHeight ? std::max(width, height) : std::min(width, height);
                height = (int)(width * aspect);
                RECT rc = { 0, 0, width, height };
                AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
                SetWindowPos(hWnd, 0, 0, 0, rc.right-rc.left, rc.bottom-rc.top, SWP_NOMOVE | SWP_NOZORDER);
                Graphics::Resize(width, height);
                break;
            }

            case WM_DESTROY:
                PostQuitMessage(0);
                break;

            default:
                return DefWindowProc( hWnd, message, wParam, lParam );
        }

        return 0;
    }
#endif
}
