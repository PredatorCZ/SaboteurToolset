/*  ModStgame
    Copyright(C) 2023 Lukas Cone

    This program is free software : you can redistribute it and / or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.If not, see <https://www.gnu.org/licenses/>.
*/

#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <string>
#include <tchar.h>
#include <windows.h>

#include "detours.h"

void PrintError(const char *funcName) {
  LPSTR lpMsgBuf = NULL;
  DWORD dw = GetLastError();

  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), lpMsgBuf,
                 0, NULL);

  printf("mod_saboteur.exe: %s failed: %s\n", funcName, lpMsgBuf);
  LocalFree(lpMsgBuf);
  ExitProcess(9009);
}

int main(int argc, char *argv[]) {
  STARTUPINFOA startInfo{};
  startInfo.cb = sizeof(startInfo);
  PROCESS_INFORMATION processInfo{};
  SetLastError(0);
  std::string args("/c");

  for (int a = 1; a < argc; a++) {
    args.push_back(' ');
    args.append(argv[a]);
  }

  const char *exePath = "/media/data/Games/The Saboteur/PC/Saboteur.exe";
  std::string laaPath = exePath;
  laaPath.replace(laaPath.size() - 3, 3, "4GB");

  {
    HANDLE hdl = CreateFileA(laaPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hdl == INVALID_HANDLE_VALUE) {
      printf("Generating 4GB patched exe\n");
      hdl = CreateFileA(exePath, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hdl == INVALID_HANDLE_VALUE) {
        printf("mod_saboteur.exe: cannot open: %s\n", exePath);
        ExitProcess(9009);
      }

      DWORD exeSize = GetFileSize(hdl, NULL);
      std::string exeBuffer;
      exeBuffer.resize(exeSize);
      if (!ReadFile(hdl, exeBuffer.data(), exeBuffer.size(), NULL, NULL)) {
        PrintError("ReadFile");
      }

      CloseHandle(hdl);

      PIMAGE_DOS_HEADER pidh =
          reinterpret_cast<PIMAGE_DOS_HEADER>(exeBuffer.data());
      PIMAGE_NT_HEADERS pinh = reinterpret_cast<PIMAGE_NT_HEADERS>(
          exeBuffer.data() + pidh->e_lfanew);

      pinh->FileHeader.Characteristics |= IMAGE_FILE_LARGE_ADDRESS_AWARE;

      hdl = CreateFileA(laaPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hdl == INVALID_HANDLE_VALUE) {
        printf("mod_saboteur.exe: cannot create: %s\n", laaPath.c_str());
        ExitProcess(9009);
      }

      if (!WriteFile(hdl, exeBuffer.data(), exeBuffer.size(), NULL, NULL)) {
        PrintError("WriteFile");
      }

      CloseHandle(hdl);
    }
  }

  if (!DetourCreateProcessWithDllA(
          laaPath.c_str(),
          const_cast<char *>(args.c_str()),             // args
          NULL,                                         // lpProcessAttributes,
          NULL,                                         // lpThreadAttributes,
          TRUE,                                         // bInheritHandles,
          CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED, // dwCreationFlags,
          NULL,                                         // lpEnvironment,
          "/media/data/Games/The Saboteur/PC/",         // lpCurrentDirectory,
          &startInfo,                                   //
          &processInfo,
          "/home/lukas/github/saboteur_toolset/build/launcher/libsbmod.dll",
          NULL // pfCreateProcessW
          )) {
    PrintError("DetourCreateProcessWithDll");
  }

  ResumeThread(processInfo.hThread);

  WaitForSingleObject(processInfo.hProcess, INFINITE);

  CloseHandle(processInfo.hProcess);
  CloseHandle(processInfo.hThread);

  DWORD dwResult = 0;
  if (!GetExitCodeProcess(processInfo.hProcess, &dwResult)) {
    PrintError("GetExitCodeProcess");
  }
  return 0;
}
