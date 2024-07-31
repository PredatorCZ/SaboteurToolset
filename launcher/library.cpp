/*  LibModStgame
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

#define NOMINMAX
#include <stdio.h>
#include <windows.h>

#include "detours.h"

#include <stdexcept>

/*
static auto HashStringOld =
    reinterpret_cast<DWORD (*)(const char *data, bool unk)>(0xDB7C10);
DWORD HashString(const char *data, bool unk) {
  return HashStringOld(data, unk);
}

static auto HashString8Old = reinterpret_cast<void *(
    __thiscall *)(void *this_, const char *data, bool unk)>(0xDB7E10);

void *__thiscall HashString8(void *this_, const char *data, bool unk) {
  return HashString8Old(this_, data, unk);
}
*/

static auto OldReadStreamData = reinterpret_cast<int(__thiscall *)(
    uint32_t thisBase, char *outBuffer, int compressedSize,
    int uncompressedSize, int *processed)>(0x65E2B0);

static constexpr uint32_t STREAM_JOB_BASE = 0x400028;

struct WSStreamBlockReader {
  char *streamBegin;
  char *streamEnd;
  char **field_8;
  bool field_C;
  uint32_t field_10;
  uint32_t field_14;
  uint32_t field_18;
  uint32_t field_1C;
  uint32_t field_20;
  uint32_t field_24;
};

struct UnkStruct0 {
  void *unk0;
  uint32_t pad_;
  LARGE_INTEGER perf;
};

struct RingBuffers {
  UnkStruct0 group0[0x32];
  UnkStruct0 group1[0x32];
} *RING_BUFFERS = reinterpret_cast<RingBuffers *>(0x12F8EB8);

struct RingIndices {
  uint32_t index0;
  uint32_t index1;
} *RB_INDICES = reinterpret_cast<RingIndices *>(0x12F9E60);

void AppendToBuffer(void *buffer, UnkStruct0 rbuffers[0x32], uint32_t &count) {
  UnkStruct0 &rbuffer = rbuffers[count];
  rbuffer.unk0 = buffer;
  QueryPerformanceCounter(&rbuffer.perf);
  count++;
  if (count >= 0x32) {
    count = 0;
  }
}

/*
void AppendToBuffer0(void *buffer) {
  AppendToBuffer(buffer, RING_BUFFERS->group0, RB_INDICES->index1);
}

void AppendToBuffer1(void *buffer) {
  AppendToBuffer(buffer, RING_BUFFERS->group1, RB_INDICES->index0);
}*/

static auto AppendToBuffer1 = reinterpret_cast<void (*)(void *)>(0x65E1D0);
static auto AppendToBuffer0 = reinterpret_cast<void (*)(void *)>(0x65E220);

int __thiscall ReadStreamData(uint32_t thisBase, char *outBuffer,
                              int compressedSize, int uncompressedSize,
                              int *processed) {
  const bool isUncompressed = compressedSize == uncompressedSize;
  const bool hasBuffer = outBuffer;

  if (!isUncompressed && !hasBuffer) {
    return -1;
  }

  if (compressedSize == 0) {
    return 0;
  }

  WSStreamBlockReader *streamer =
      reinterpret_cast<WSStreamBlockReader *>(thisBase + STREAM_JOB_BASE);
  uint32_t toBeReadBytes = compressedSize;
  uint32_t toBeReadBytes2 = compressedSize;
  uint32_t nextBase = thisBase + sizeof(WSStreamBlockReader);
  char *curBuffer = outBuffer;
  uint32_t totalOut = 0;
  int v24 = 0;

  while (true) {
    while (streamer->streamEnd >= streamer->streamBegin) {
      if (streamer->streamBegin + compressedSize <= streamer->streamEnd) {
        break;
      }

      Sleep(1);
    }

    /*
      v11 = (const char *)(v10->field_0 + thisBase + 0x28);
      if ( *(_BYTE *)(thisBase + 0x400028 + 0xC) )
        v20 = (unsigned int)&v11[v22] >= *(_DWORD *)(thisBase + 0x400028 + 0x8)
      + thisBase + 0x28; else v20 = &v11[v22] > (const char *)v10;
    */

    char *streamedData = streamer->streamBegin + nextBase;
    const bool unk1 = [&] {
      if (streamer->field_C) {
        return streamedData >= *streamer->field_8 + nextBase;
      }

      return streamedData > (const char *)streamer;
    }();

    /*
      if ( unk1 )
      {
        v12 = *(_DWORD *)(thisBase + 0x400028 + 0x8);
        if ( !*(_BYTE *)(thisBase + 0x400028 + 0xC) )
          v12 = 0x400000;
        v9 = v12 - (_DWORD)v11 + thisBase + 0x28;
      }
    */

    if (unk1) {
      char *v12 = *streamer->field_8;
      if (!streamer->field_C) {
        v12 = (char *)0x400000;
      }
      toBeReadBytes = v12 - streamedData + nextBase;
    }

    if (toBeReadBytes) {
      if (!isUncompressed) {
        AppendToBuffer1(streamer->streamBegin);
        // v24 = inflate(&strm, 0);
        AppendToBuffer0(streamer->streamBegin + toBeReadBytes);

        /*totalOut = strm.total_out;
        v23 = &outBuffer[strm.total_out];
        if (v24 < 0)
          v24 = 0;*/
      } else {
        if (outBuffer) {
          AppendToBuffer1(streamer->streamBegin);
          memcpy(curBuffer, streamedData, toBeReadBytes);
          AppendToBuffer0(streamer->streamBegin + toBeReadBytes);
        }

        curBuffer += toBeReadBytes;
      }

      streamer->streamBegin += toBeReadBytes;
      toBeReadBytes2 -= toBeReadBytes;
    }

    if (unk1) {
      streamer->streamBegin = 0;
      if (streamer->field_C) {
        streamer->field_C = false;
        streamer->field_8 = 0;
      }
    }
    if (!toBeReadBytes2) {
      break;
    }
    toBeReadBytes = toBeReadBytes2;
  }

  if (processed) {
    *processed = totalOut;
  }
  return v24;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  (void)hinst;
  (void)reserved;

  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH) {
    DetourRestoreAfterWith();
    printf("libsbmod.dll: Starting.\n");
    fflush(stdout);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&OldReadStreamData, ReadStreamData);

    LONG error = DetourTransactionCommit();

    if (error != NO_ERROR) {
      throw std::runtime_error("Failed to attach function");
    }
  } else if (dwReason == DLL_PROCESS_DETACH) {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&OldReadStreamData, ReadStreamData);
    DetourTransactionCommit();
  }

  return TRUE;
}
