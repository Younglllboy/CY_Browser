// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <windows.h>
#include <string>
#include <filesystem>

#include "include/cef_command_line.h"
#include "include/cef_sandbox_win.h"
#include "include/cef_version_info.h"
#include "simple_app.h"
#include "local_data.h"

namespace {

int RunMain(HINSTANCE hInstance,
            LPWSTR lpCmdLine,
            int nCmdShow,
            void* sandbox_info) {
  int exit_code;

  // Provide CEF with command-line arguments.
  CefMainArgs main_args(hInstance);

  // CEF applications have multiple sub-processes (render, GPU, etc) that share
  // the same executable. This function checks the command-line and, if this is
  // a sub-process, executes the appropriate logic.
  exit_code = CefExecuteProcess(main_args, nullptr, sandbox_info);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }

  // Parse command-line arguments for use in this method.
  CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
  command_line->InitFromString(::GetCommandLineW());

  // Specify CEF global settings here.
  CefSettings settings;

  // Give this application its own cache root so another CEF application
  // cannot take over its process singleton at startup.
  const auto cache_root=LocalDataPath();
  std::filesystem::create_directories(cache_root);
  CefString(&settings.root_cache_path) = cache_root.wstring();
  CefString(&settings.cache_path) = cache_root.wstring();

  if (!sandbox_info) {
    settings.no_sandbox = true;
  }

  // SimpleApp implements application-level callbacks for the browser process.
  // It will create the first browser instance in OnContextInitialized() after
  // CEF has initialized.
  CefRefPtr<SimpleApp> app(new SimpleApp);

  // Initialize the CEF browser process. May return false if initialization
  // fails or if early exit is desired (for example, due to process singleton
  // relaunch behavior).
  if (!CefInitialize(main_args, settings, app.get(), sandbox_info)) {
    const int code = CefGetExitCode();
    if (code == CEF_RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED)
      return 0;
    MessageBoxW(nullptr,
                L"CEF initialization failed. Check the application folder "
                L"and debug.log for details.",
                L"CYBROWSER startup error", MB_OK | MB_ICONERROR);
    return code;
  }

  // Run the CEF message loop. This will block until CefQuitMessageLoop() is
  // called.
  CefRunMessageLoop();

  // Shut down CEF.
  CefShutdown();

  return 0;
}

}  // namespace

#if defined(CEF_USE_BOOTSTRAP)

// Entry point called by bootstrap.exe when built as a DLL.
CEF_BOOTSTRAP_EXPORT int RunWinMain(HINSTANCE hInstance,
                                    LPWSTR lpCmdLine,
                                    int nCmdShow,
                                    void* sandbox_info,
                                    cef_version_info_t* /*version_info*/) {
  return ::RunMain(hInstance, lpCmdLine, nCmdShow, sandbox_info);
}

#else  // !defined(CEF_USE_BOOTSTRAP)

// Entry point function for all processes.
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

#if defined(ARCH_CPU_32_BITS)
  // Run the main thread on 32-bit Windows using a fiber with the preferred 4MiB
  // stack size. This function must be called at the top of the executable entry
  // point function (`main()` or `wWinMain()`). It is used in combination with
  // the initial stack size of 0.5MiB configured via the `/STACK:0x80000` linker
  // flag on executable targets. This saves significant memory on threads (like
  // those in the Windows thread pool, and others) whose stack size can only be
  // controlled via the linker flag.
  int exit_code = CefRunWinMainWithPreferredStackSize(wWinMain, hInstance,
                                                      lpCmdLine, nCmdShow);
  if (exit_code >= 0) {
    // The fiber has completed so return here.
    return exit_code;
  }
#endif

  void* sandbox_info = nullptr;

#if defined(CEF_USE_SANDBOX)
  // Manage the life span of the sandbox information object. This is necessary
  // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
  CefScopedSandboxInfo scoped_sandbox;
  sandbox_info = scoped_sandbox.sandbox_info();
#endif

  return ::RunMain(hInstance, lpCmdLine, nCmdShow, sandbox_info);
}

#endif  // !defined(CEF_USE_BOOTSTRAP)
