#include <windows.h>
#include <shlobj.h>
#include <string>
#include <iostream>
#include <fstream>
#include <regex>
#include <filesystem>

// Helper function to convert UTF-8 string to wide string
static std::wstring Utf8ToWide(const std::string& utf8Str) {
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    if (wideSize == 0) {
        return L"";
    }
    std::wstring wideStr(wideSize - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wideStr[0], wideSize - 1);
    return wideStr;
}

// Utility to extract folder name
std::wstring GetFolderName(const std::wstring& folderPath) {
    return std::filesystem::path(folderPath).filename().wstring();
}

// Utility to extract file name
std::wstring GetFileName(const std::wstring& filePath) {
    return std::filesystem::path(filePath).filename().wstring();
}

// Function to validate the icon file
static bool ValidateIconFile(const std::wstring& iconPath) {
    if (!std::filesystem::exists(iconPath)) {
        std::wcerr << L"Icon file not found: " << iconPath << std::endl;
        return false;
    }

    std::wstring ext = std::filesystem::path(iconPath).extension();
    if (ext != L".ico" && ext != L".dll") {
        std::wcerr << L"Invalid icon file type: " << iconPath << L". Only .ico and .dll are supported." << std::endl;
        return false;
    }
    return true;
}

// Function to validate and parse the /n argument
static int ParseIconIndex(const std::wstring& iconIndexStr) {
    std::wregex numberRegex(LR"(^"?\d+"?$)");
    if (!std::regex_match(iconIndexStr, numberRegex)) {
        std::wcerr << L"Invalid icon index specified. /n must be a numeric value." << std::endl;
        return -1;
    }
    return std::stoi(iconIndexStr);
}

// Function to prompt user for icon index
static int PromptIconIndex(const std::wstring& dllPath) {
    int index = -1;
    std::wcout << L"Enter the icon index for " << dllPath << L": ";
    std::wcin >> index;
    return index;
}


// Function to trim quotes from a string
static std::wstring TrimQuotes(const std::wstring& str) {
    if (!str.empty() && str.front() == L'"' && str.back() == L'"') {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

static std::wstring ResolveRelativePath(const std::wstring& basePath, const std::wstring& relativePath) {
    std::filesystem::path base(basePath);
    std::filesystem::path resolved = base.parent_path() / relativePath;
    return resolved.wstring();
}

// Function to read IconResource from desktop.ini and validate it
static std::wstring ReadAndValidateIconResource(const std::wstring& iniFilePath) {
    std::wifstream iniFile(iniFilePath);
    std::wstring line, iconResource;

    if (!iniFile.is_open()) {
        std::wcerr << L"Failed to open desktop.ini: " << iniFilePath << std::endl;
        return L"";
    }

    // Read the desktop.ini file and find the IconResource entry
    while (std::getline(iniFile, line)) {
        if (line.find(L"IconResource=") == 0) {
            iconResource = line.substr(13); // Extract value after "IconResource="
            break;
        }
    }
    iniFile.close();

    if (iconResource.empty()) {
        std::wcerr << L"No IconResource found in " << iniFilePath << std::endl;
        return L"";
    }

    // Extract the icon file path from the IconResource value
    size_t commaPos = iconResource.find_last_of(L",");
    std::wstring iconPath = (commaPos != std::wstring::npos) ? iconResource.substr(0, commaPos) : iconResource;

    // Trim quotes from the iconPath
    iconPath = TrimQuotes(iconPath);

    // Resolve relative path
    if (!std::filesystem::path(iconPath).is_absolute()) {
        iconPath = ResolveRelativePath(iniFilePath, iconPath);
    }

    // Check if the file exists
    bool fileExists = std::filesystem::exists(iconPath);
    if (!fileExists) {
        std::wcerr << L"IconResource file does not exist: " << iconPath << std::endl;
    }

    // Validate the file extension
    std::wstring extension = std::filesystem::path(iconPath).extension().wstring();
    if (extension != L".ico" && extension != L".dll") {
        std::wcerr << L"Unsupported IconResource file type in IconResource: "
            << L"\"" << GetFileName(iconPath) << L"\""
            << L" (Only .ico and .dll are supported.)" << std::endl;
        // Even if the file doesn't exist, we still validate and warn about the type
    }

    // If the file doesn't exist or has an invalid type, return an empty string
    if (!fileExists || (extension != L".ico" && extension != L".dll")) {
        return L"";
    }

    return iconResource; // Return the valid IconResource string
}

// Function to update folder icon
static bool UpdateFolderIcon(const std::wstring& folderPath, const std::wstring& iconPath, int iconIndex) {
    SHFOLDERCUSTOMSETTINGS fcs = { 0 };
    fcs.dwSize = sizeof(SHFOLDERCUSTOMSETTINGS);
    fcs.dwMask = FCSM_ICONFILE;
    fcs.pszIconFile = const_cast<LPWSTR>(iconPath.c_str());

    fcs.iIconIndex = iconIndex;

    HRESULT hr = SHGetSetFolderCustomSettings(&fcs, folderPath.c_str(), FCS_FORCEWRITE);
    if (SUCCEEDED(hr)) {
        SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH, folderPath.c_str(), NULL);
        std::wcout << L"Folder icon updated successfully for \"" << GetFolderName(folderPath) << L"\"" << std::endl;
        return true;
    }
    else {
        std::wcerr << L"Failed to update folder icon for " << folderPath << L", HRESULT: " << hr << std::endl;
        return false;
    }
}

// Function to apply attributes to a single file
static bool ApplyAttributes(const std::wstring& filePath, const std::wstring& attributeOption) {
    DWORD attributes = GetFileAttributes(filePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        std::wcerr << L"Failed to get attributes for: " << filePath << std::endl;
        return false;
    }

    // Apply +H, -H, +S, -S based on the attributeOption
    if (attributeOption.find(L"+H") != std::wstring::npos) {
        attributes |= FILE_ATTRIBUTE_HIDDEN;
    }
    if (attributeOption.find(L"-H") != std::wstring::npos) {
        attributes &= ~FILE_ATTRIBUTE_HIDDEN;
    }
    if (attributeOption.find(L"+S") != std::wstring::npos) {
        attributes |= FILE_ATTRIBUTE_SYSTEM;
    }
    if (attributeOption.find(L"-S") != std::wstring::npos) {
        attributes &= ~FILE_ATTRIBUTE_SYSTEM;
    }

    // Apply the modified attributes to the file
    if (SetFileAttributes(filePath.c_str(), attributes)) {
        std::wcout << L"Attributes applied to: " << filePath << std::endl;
        return true;
    }
    else {
        std::wcerr << L"Failed to apply attributes to: " << filePath << std::endl;
        return false;
    }
}

// Helper function to parse attributes from /a
static DWORD ParseAttributes(const std::wstring& attributeOption) {
    DWORD attributes = 0;

    // Default to +H -S (Hidden, Not System) if no attributes specified
    if (attributeOption.empty()) {
        attributes |= FILE_ATTRIBUTE_HIDDEN;
        attributes |= FILE_ATTRIBUTE_SYSTEM;
    }

    if (attributeOption.find(L"+H") != std::wstring::npos) {
        attributes |= FILE_ATTRIBUTE_HIDDEN;
    }
    if (attributeOption.find(L"-H") != std::wstring::npos) {
        attributes &= ~FILE_ATTRIBUTE_HIDDEN;
    }

    if (attributeOption.find(L"+S") != std::wstring::npos) {
        attributes |= FILE_ATTRIBUTE_SYSTEM;
    }
    if (attributeOption.find(L"-S") != std::wstring::npos) {
        attributes &= ~FILE_ATTRIBUTE_SYSTEM;
    }

    return attributes;
}

// Function to handle /a logic
static int HandleAttributes(const std::wstring& folderPath, const std::wstring& attributeOption) {
    std::wstring iniFilePath = folderPath + L"\\desktop.ini";
    if (!std::filesystem::exists(iniFilePath)) {
        std::wcerr << L"desktop.ini not found in " << folderPath << std::endl;
        return 1;
    }

    // Validate desktop.ini
    std::wstring iconResource = ReadAndValidateIconResource(iniFilePath);
    if (iconResource.empty()) {
        std::wcerr << L"Invalid IconResource in desktop.ini. Skipping attribute changes." << std::endl;
        return 1;
    }

    // Extract icon file path from IconResource
    size_t commaPos = iconResource.find_last_of(L",");
    std::wstring iconFilePath = (commaPos != std::wstring::npos) ? iconResource.substr(0, commaPos) : iconResource;
    iconFilePath = TrimQuotes(iconFilePath);

    // Resolve relative paths for the icon file
    if (!std::filesystem::path(iconFilePath).is_absolute()) {
        iconFilePath = ResolveRelativePath(iniFilePath, iconFilePath);
        return 1;
    }

    // Validate the icon file
    if (!ValidateIconFile(iconFilePath)) {
        std::wcerr << L"Invalid IconResource file. Skipping attribute changes." << std::endl;
        return 1;
    }

    // Apply attributes to desktop.ini and IconResource
    ApplyAttributes(iniFilePath, attributeOption);  // Pass the correct attribute options here
    ApplyAttributes(iconFilePath, attributeOption);  // Same for the icon
    return 0;
}

// Function to refresh or update folder icon
static int ProcessFolder(const std::wstring& folderPath, const std::wstring& iconPath, int iconIndex, const std::wstring& attributeOption) {
    DWORD attributes = attributeOption.empty() ? INVALID_FILE_ATTRIBUTES : ParseAttributes(attributeOption);
    std::wstring iniFilePath = folderPath + L"\\desktop.ini";
    std::wstring iconResource;
    DWORD originalAttributes = INVALID_FILE_ATTRIBUTES;

    // Check if desktop.ini exists and get its attributes
    if (std::filesystem::exists(iniFilePath)) {
        originalAttributes = GetFileAttributes(iniFilePath.c_str());
        if (originalAttributes == INVALID_FILE_ATTRIBUTES) {
            std::wcerr << L"Failed to get attributes for desktop.ini: " << iniFilePath << std::endl;
            return 1;
        }
    }

    if (!iconPath.empty()) {
        // Validate the icon file
        if (!ValidateIconFile(iconPath)) {
            return 1; // Return an error code if validation fails
        }

        // Determine the icon resource value
        if (std::filesystem::path(iconPath).extension() == L".ico") {
            iconIndex = (iconIndex == -1) ? 0 : iconIndex;
        }
        else if (std::filesystem::path(iconPath).extension() == L".dll") {
            if (iconIndex == -1) {
                iconIndex = PromptIconIndex(iconPath);
            }
        }
        iconResource = iconPath + L"," + std::to_wstring(iconIndex);

        // Update the folder icon
        UpdateFolderIcon(folderPath, iconPath, iconIndex);

        // Apply attributes to desktop.ini and icon file if specified
        if (!attributeOption.empty()) {
            ApplyAttributes(iniFilePath, attributeOption);  // Apply to desktop.ini
            ApplyAttributes(iconPath, attributeOption);  // Apply to icon file
        }
    }
    else if (std::filesystem::exists(iniFilePath)) {
        // Read the existing IconResource
        iconResource = ReadAndValidateIconResource(iniFilePath);
        if (iconResource.empty()) {
            std::wcerr << L"An error found in IconResource." << std::endl;
            return 1;
        }

        // Extract icon path and index from IconResource
        size_t commaPos = iconResource.find_last_of(L",");
        std::wstring existingIconPath = (commaPos != std::wstring::npos) ? iconResource.substr(0, commaPos) : iconResource;
        int existingIconIndex = (commaPos != std::wstring::npos) ? std::stoi(iconResource.substr(commaPos + 1)) : 0;

        // Refresh the folder icon
        UpdateFolderIcon(folderPath, existingIconPath, existingIconIndex);
    }
    else {
        std::wcerr << L"No desktop.ini found. Use /f /i to assign a folder icon." << std::endl;
        return 1;
    }

    // Reapply the original attributes of desktop.ini, if it existed
    if (originalAttributes != INVALID_FILE_ATTRIBUTES) {
        if (!SetFileAttributes(iniFilePath.c_str(), originalAttributes)) {
            std::wcerr << L"Failed to restore original attributes for desktop.ini: " << iniFilePath << std::endl;
            return 1;
        }
    }

    // If all operations succeed, return 0
    return 0;
}


// Main function
int wmain(int argc, wchar_t* argv[]) {
    if (argc == 1) { // No arguments were passed
        // Construct the command to open cmd.exe with the help command
        std::wstring currentExePath(MAX_PATH, L'\0');
        GetModuleFileNameW(NULL, &currentExePath[0], static_cast<DWORD>(currentExePath.size()));
        currentExePath.resize(wcslen(currentExePath.c_str()));

        // Command to open cmd.exe and execute the program with /?
        std::wstring command = L"cmd.exe /k \"\"" + currentExePath + L"\" /?\"";

        // Start cmd.exe with the command
        STARTUPINFO si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        if (CreateProcessW(
                NULL, &command[0], NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            std::wcerr << L"Failed to open cmd.exe. Error code: " << GetLastError() << std::endl;
            return 3; // File or path not found
        }
        return 0; // Exit After launching CMD
    }

    // Existing argument parsing and functionality
    std::wstring folderPath, iconPath, attributeOption;
    int iconIndex = -1;
    int result = 0; // Default to success

    try {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"/f" && i + 1 < argc) {
                folderPath = argv[++i];
            }
            else if (arg == L"/i" && i + 1 < argc) {
                iconPath = argv[++i];
            }
            else if (arg == L"/n" && i + 1 < argc) {
                if (folderPath.empty() || iconPath.empty()) {
                    std::wcerr << L"Error: /n requires both /f and /i. Use /? for help." << std::endl;
                    return 1;
                }
                iconIndex = ParseIconIndex(argv[++i]);
                if (iconIndex == -1) {
                    return 1;
                }
            }
            // Additional argument handling
            else if (arg == L"/a") {
                std::wstring attributeOption;
                if (i + 1 < argc && argv[i + 1][0] != L'/') {
                    attributeOption = argv[++i];
                }
                HandleAttributes(folderPath, attributeOption);
            }
            else if (arg == L"/?" || arg == L"-?" || arg == L"--help") {
                std::wcout
                    << L"Usage:\n\n"
                    << L"  FolderIconUpdater.exe /f <folder> [/i <icon_path>] [/n <icon_index>] [/a <attributes>]\n\n"
                    << L"Options:\n"
                    << L"  /f: Specifies the folder whose icon will be updated.\n"
                    << L"  /i: Specifies the path to the icon file (.ico or .dll).\n"
                    << L"  /n: Specifies the icon index (applicable only for .dll files; optional for .ico files).\n"
                    << L"  /a: Specifies file attributes for \"desktop.ini\" and the icon file.\n"
                    << L"      Attributes:\n"
                    << L"        +H: Hidden          -H: Not Hidden\n"
                    << L"        +S: System          -S: Not System\n"
                    << L"      Default: +H -S (Hidden, Not System)\n\n"
                    << L"Examples:\n\n"
                    << L"  1. Refresh the folder icon based on existing desktop.ini:\n"
                    << L"     FolderIconUpdater.exe /f \"C:\\MyFolder\"\n\n"
                    << L"  2. Assign an .ico file as the folder icon:\n"
                    << L"     FolderIconUpdater.exe /f \"C:\\MyFolder\" /i \"C:\\Icons\\Icon.ico\"\n\n"
                    << L"  3. Assign an icon from a .dll file with a specific index:\n"
                    << L"     FolderIconUpdater.exe /f \"C:\\MyFolder\" /i \"C:\\Icons\\IconPack.dll\" /n 5\n\n"
                    << L"  4. Assign an icon with specific file attributes:\n"
                    << L"     FolderIconUpdater.exe /f \"C:\\MyFolder\" /i \"C:\\Icons\\Icon.ico\" /a +H -S\n\n"
                    << L"Notes:\n"
                    << L"  - If /a is used, the specified attributes will be applied to \"desktop.ini\" and the icon file.\n"
                    << L"  - If /f and /i are used without /a, \"desktop.ini\" and the icon file will default to +H -S.\n"
                    << L"  - If only /f is used, existing file attributes for \"desktop.ini\" and the icon file will remain unchanged.\n\n";
                return 0;
            }
            else {
                std::wcerr << L"Error: Unrecognized argument \"" << arg << L"\". Use /? for help." << std::endl;
                return 2;
            }
        }
    

    // Ensure /f is specified
    if (folderPath.empty()) {
        std::wcerr << L"Error: /f must be specified. Use /? for help." << std::endl;
        return 1;
    }

    // Ensure /n is not used without /i
    if (!iconPath.empty() && iconIndex == -1 && std::filesystem::path(iconPath).extension() == L".dll") {
        iconIndex = PromptIconIndex(iconPath);
    }
    else if (iconIndex != -1 && iconPath.empty()) {
        std::wcerr << L"Error: /n requires both /f and /i. Use /? for help." << std::endl;
        return 1;
    }

    // Process the folder
    result = ProcessFolder(folderPath, iconPath, iconIndex, attributeOption);
    }
    catch (const std::exception& e) {
        std::wcerr << L"Unexpected error: " << Utf8ToWide(e.what()) << std::endl;
        result = 1;
    }
    catch (...) {
        std::wcerr << L"Unknown error occurred." << std::endl;
        result = 1;
    }

    return result;
}
