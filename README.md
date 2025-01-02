
I made this to fix the issue where folder icons don't update immediately after modifying desktop.ini with a batch script. This program lets me refresh and update with the following command:
```cmd
FolderIconUpdater.exe /f "C:\Target\Folder"
```
You can also assign a folder icon directly:
```cmd
FolderIconUpdater.exe /f "C:\Target\Folder" /i "C:\Folder\Icon.ico"
```
<br>

It works, but there are still some issues to address:
1. Antivirus may detect it as malicious.
2. Sometimes, F5 or manual refresh is still needed.
3. I want to add options to set file attributes for desktop.ini and .ico.

This is as far as I’ve gotten, but I might update it later.
I haven’t made a release for this repo yet, but you can check out the compiled version of the program here:
[FolderIconUpdater.exe](https://github.com/ramdany7/RightClick-Folder-Icon-Tools/tree/main/resources)
