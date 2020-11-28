#ifndef AL_FILE_LOAD_H
#define AL_FILE_LOAD_H

#include <cstddef>
#include <stdio.h>
#include <string_view>

#include "engine/debug/debug.h"
#include "engine/memory/allocator_base.h"

namespace al::engine
{
    enum class FileLoadMode
    {
        READ,
        WRITE
    };

    struct FileHandle
    {
        enum class State
        {
            FREE,
            LOADED,
            LOADING
        };

        std::size_t size;
        State state;
        std::byte* memory;
    };

    static const char* SYNC_LOAD_MODE_TO_STR[] =
    {
        "rb",
        "wb"
    };

    FileHandle sync_load(std::string_view name, AllocatorBase* allocator, FileLoadMode mode)
    {
        // @TODO : add ability to change open mode
        std::FILE* file = std::fopen(name.data(), SYNC_LOAD_MODE_TO_STR[static_cast<int>(mode)]);   
        al_assert(file);

        std::size_t fileSize;
        std::fseek(file, 0, SEEK_END);
        fileSize = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        std::byte* memory = allocator->allocate(fileSize + 1);
        al_assert(memory);

        std::size_t readSize = std::fread(memory, sizeof(std::byte), fileSize, file);
        al_assert(readSize == fileSize);

        std::fclose(file);
        memory[fileSize] = std::byte{ 0 };
        return
        {
            .size{ fileSize + 1 },
            .state{ FileHandle::State::LOADED },
            .memory{ memory }
        };
    }
}

#endif
