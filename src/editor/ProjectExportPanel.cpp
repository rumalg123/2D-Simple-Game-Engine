#include "Engine.h"
#include "ProjectManager.h"
#include "imgui.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

void Engine::renderProjectExportPanel() {
    ImGui::Begin("Build Game");
    ImGui::TextUnformatted("Windows x64 standalone game export");
    ImGui::TextDisabled("Saves are not automatic. Save Scene JSON before exporting.");
    const std::filesystem::path assetPath(config.assetRoot);
    const auto projectRoot = assetPath.parent_path();
    const bool isProject = std::filesystem::is_regular_file(
        projectRoot / ProjectManager::ManifestName);
    if (!isProject) {
        ImGui::TextWrapped("Open a SimpleEngine project to enable Build Game.");
        ImGui::End();
        return;
    }
    const auto exportDir = projectRoot.parent_path() /
        (projectRoot.filename().string() + "_Exports");
    ImGui::TextWrapped("Project: %s",projectRoot.string().c_str());
    ImGui::TextWrapped("Output: %s",exportDir.string().c_str());
    if (playSession.active()) {
        ImGui::TextDisabled("Stop Play Mode before building from saved scene data.");
    }
    static bool useCompiledTextures = true;
    ImGui::Checkbox("Use compiled texture artifacts",&useCompiledTextures);
    ImGui::TextDisabled("Optimized RGBA textures stay in the ZIP next to source assets.");
    ImGui::BeginDisabled(playSession.active());
    if (ImGui::Button("Build Windows Game")) {
        wchar_t executableBuffer[32768]{};
        const DWORD copied=GetModuleFileNameW(nullptr,executableBuffer,32768);
        if (!copied || copied>=32768) {
            editorStatus="Cannot locate SimpleEngineEditor executable.";
        } else {
            const auto editorDir=std::filesystem::path(executableBuffer).parent_path();
            // Local source tree: build/../tools ; packaged editor: ./tools.
            auto script=editorDir / "tools/export-project.ps1";
            if (!std::filesystem::is_regular_file(script))
                script=editorDir / "../tools/export-project.ps1";
            if (!std::filesystem::is_regular_file(script)) {
                editorStatus="Exporter script missing. Use a full developer build or recent editor ZIP.";
            } else if (!std::filesystem::is_regular_file(editorDir/"SimpleEnginePlayer.exe") ||
                       !std::filesystem::is_regular_file(editorDir/"simpleengine_project.exe")) {
                editorStatus="Build SimpleEnginePlayer and simpleengine_project before exporting.";
            } else {
                // Each argument is individually quoted; no shell/cmd.exe is
                // involved. Windows paths cannot contain a double quote.
                const std::wstring command =
                    L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" +
                    std::filesystem::absolute(script).wstring() +
                    L"\" -ProjectPath \"" + projectRoot.wstring() +
                    L"\" -BuildDir \"" + editorDir.wstring() +
                    L"\" -OutputDir \"" + exportDir.wstring() +
                    L"\" -SkipBuild" +
                    (useCompiledTextures ? L" -UseImportedTextures" : L"");
                std::vector<wchar_t> mutableCommand(command.begin(),command.end());
                mutableCommand.push_back(L'\0');
                STARTUPINFOW startup{};
                startup.cb=sizeof(startup);
                PROCESS_INFORMATION process{};
                const BOOL launched=CreateProcessW(nullptr,mutableCommand.data(),
                    nullptr,nullptr,FALSE,CREATE_NEW_CONSOLE,nullptr,nullptr,
                    &startup,&process);
                if(launched) {
                    CloseHandle(process.hThread);
                    CloseHandle(process.hProcess);
                    editorStatus="Export launched in its own console. Check its result before distributing.";
                } else {
                    editorStatus="Cannot start PowerShell exporter (Win32 error " +
                                 std::to_string(GetLastError()) + ").";
                }
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("The exporter validates Lua, scenes and assets, then bundles "
                       "a player-only EXE, required DLLs and a ZIP.");
    ImGui::TextDisabled("Windows is supported. Other platform templates are future work.");
    ImGui::End();
}
