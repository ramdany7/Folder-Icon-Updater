#include <windows.h>
#include <shlobj.h>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>

// Helper function to get the attributes of a file
static DWORD GetFileAttributesSafe(const std::wstring& filePath) {
    DWORD attributes = GetFileAttributes(filePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return 0; // No attributes (file may not exist)
    }
    return attributes;
}

// Helper function to set the attributes of a file
static void SetFileAttributesSafe(const std::wstring& filePath, DWORD attributes) {
    if (!SetFileAttributes(filePath.c_str(), attributes)) {
        std::wcout << L"Failed to set attributes for " << filePath << std::endl;
    }
}

// Function to extract IconResource and IconIndex from desktop.ini
static std::pair<std::wstring, int> ParseIconResource(const std::wstring& iconResource) {
    std::wregex iconRegex(L"^(.*?)(,(-?\\d+))?$");
    std::wsmatch match;

    if (std::regex_match(iconResource, match, iconRegex)) {
        std::wstring path = match[1].str();
        int index = match[3].matched ? std::stoi(match[3].str()) : 0;
        return { path, index };
    }

    return { iconResource, 0 };
}

// Function to get IconResource and IconIndex from desktop.ini
static std::pair<std::wstring, int> GetIconResource(const std::wstring& iniFilePath) {
    std::wifstream iniFile(iniFilePath);
    if (!iniFile.is_open()) {
        std::wcout << L"Could not open desktop.ini file: " << iniFilePath << std::endl;
        return { L"", 0 };
    }

    std::wstring line;
    std::wregex iconResourceRegex(L"^IconResource=(.*)");
    std::wsmatch match;

    while (std::getline(iniFile, line)) {
        if (std::regex_match(line, match, iconResourceRegex)) {
            iniFile.close();
            return ParseIconResource(match[1].str());
        }
    }

    iniFile.close();
    return { L"", 0 };
}

// Function to update folder icon
static void UpdateFolderIcon(const std::wstring& folderPath, const std::wstring& iconPath, int iconIndex) {
    SHFOLDERCUSTOMSETTINGS fcs = { 0 };
    fcs.dwSize = sizeof(SHFOLDERCUSTOMSETTINGS);
    fcs.dwMask = FCSM_ICONFILE;
    fcs.pszIconFile = const_cast<LPWSTR>(iconPath.c_str());
    fcs.iIconIndex = iconIndex;

    HRESULT hr = SHGetSetFolderCustomSettings(&fcs, folderPath.c_str(), FCS_FORCEWRITE);
    if (SUCCEEDED(hr)) {
        SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH, folderPath.c_str(), NULL);
        std::wcout << L"Folder icon updated successfully for " << folderPath << std::endl;
    }
    else {
        std::wcout << L"Failed to update folder icon for " << folderPath << L", HRESULT: " << hr << std::endl;
    }
}

// Function to process a single folder
static void ProcessFolder(const std::wstring& folderPath) {
    std::wstring iniFilePath = folderPath + L"\\desktop.ini";

    DWORD originalAttributes = GetFileAttributesSafe(iniFilePath);

    if (std::filesystem::exists(iniFilePath)) {
        auto [iconPath, iconIndex] = GetIconResource(iniFilePath);
        if (!iconPath.empty()) {
            UpdateFolderIcon(folderPath, iconPath, iconIndex);
        }
        else {
            std::wcout << L"No valid IconResource found in desktop.ini for " << folderPath << std::endl;
        }

        // Restore original attributes
        if (originalAttributes != 0) {
            SetFileAttributesSafe(iniFilePath, originalAttributes);
        }
    }
    else {
        std::wcout << L"desktop.ini not found in folder: " << folderPath << std::endl;
    }
}

// Main program logic
int wmain(int argc, wchar_t* argv[]) {
    std::wstring folderPath;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"/f" && i + 1 < argc) {
            folderPath = argv[++i];
        }
    }

    // If folder path is specified, process it
    if (!folderPath.empty()) {
        ProcessFolder(folderPath);
    }
    else {
        std::wcout << L"Usage: FolderIconUpdater.exe /f \"C:\\Path\\ThisFolder\"" << std::endl;
    }

    return 0;
}
