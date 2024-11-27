#include <windows.h>
#include <shlobj.h>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>

static std::wstring GetIconResource(const std::wstring& iniFilePath) {
    std::wifstream iniFile(iniFilePath.c_str()); // Convert std::wstring to const wchar_t*
    std::wstring line;
    std::wregex iconResourceRegex(L"^IconResource=(.*)$");
    std::wsmatch match;

    while (std::getline(iniFile, line)) {
        if (std::regex_search(line, match, iconResourceRegex)) {
            std::wstring iconResource = match[1].str();
            // Remove ",0" suffix if it exists
            std::wregex suffixRegex(L",(0)$");
            iconResource = std::regex_replace(iconResource, suffixRegex, L"");
            return iconResource;
        }
    }

    return L"";
}

static void UpdateFolderIcon(const std::wstring& folderPath, const std::wstring& iconPath) {
    SHFOLDERCUSTOMSETTINGS fcs = { 0 };
    fcs.dwSize = sizeof(SHFOLDERCUSTOMSETTINGS);
    fcs.dwMask = FCSM_ICONFILE;
    fcs.pszIconFile = const_cast<LPWSTR>(iconPath.c_str());
    fcs.iIconIndex = 0; // Set to 0 to avoid adding suffix

    HRESULT hr = SHGetSetFolderCustomSettings(&fcs, folderPath.c_str(), FCS_FORCEWRITE);
    if (SUCCEEDED(hr)) {
        std::wcout << L"Folder icon updated successfully for " << folderPath << std::endl;
    }
    else {
        std::wcout << L"Failed to update folder icon for " << folderPath << std::endl;
    }
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3 || argc > 5) {
        std::wcout << L"Usage: FI-Updater.exe /F \"folderPath\" [/I \"iconPath\"]" << std::endl;
        return 1;
    }

    std::wstring folderPath;
    std::wstring iconPath;

    for (int i = 1; i < argc; i += 2) {
        std::wstring arg = argv[i];
        if (arg == L"/F" || arg == L"/f") {
            folderPath = argv[i + 1];
        }
        else if (arg == L"/I" || arg == L"/i") {
            iconPath = argv[i + 1];
        }
    }

    if (!folderPath.empty()) {
        std::wstring iniFilePath = folderPath + L"\\desktop.ini";
        if (std::filesystem::exists(iniFilePath)) {
            if (iconPath.empty()) {
                iconPath = GetIconResource(iniFilePath);
            }

            if (!iconPath.empty()) {
                // Update the icon for the specified folder
                UpdateFolderIcon(folderPath, iconPath);
            }
            else {
                std::wcout << L"No value found for IconResource in desktop.ini." << std::endl;
                return 1;
            }
        }
        else {
            std::wcout << L"desktop.ini not found in the specified folder." << std::endl;
            return 1;
        }
    }
    else {
        std::wcout << L"Invalid arguments." << std::endl;
        return 1;
    }

    return 0;
}
