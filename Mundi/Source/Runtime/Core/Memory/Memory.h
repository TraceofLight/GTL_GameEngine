#pragma once
#include <cstdlib>
#include <cstring>

/**
 * @brief 메모리 할당/해제 유틸리티 (언리얼 FMemory 스타일)
 * @details malloc/realloc/free를 래핑하여 언리얼 스타일 API 제공
 */
struct FMemory
{
	/**
	 * Allocate memory
	 * @param Size - Number of bytes to allocate
	 * @return Pointer to allocated memory, or nullptr if failed
	 */
	static void* Malloc(size_t Size)
	{
		if (Size == 0)
		{
			return nullptr;
		}
		return malloc(Size);
	}

	/**
	 * Reallocate memory (preserves existing data)
	 * @param Original - Pointer to existing memory (can be nullptr)
	 * @param Size - New size in bytes
	 * @return Pointer to reallocated memory, or nullptr if failed
	 */
	static void* Realloc(void* Original, size_t Size)
	{
		if (Size == 0)
		{
			Free(Original);
			return nullptr;
		}
		return realloc(Original, Size);
	}

	/**
	 * Free memory
	 * @param Original - Pointer to memory to free (can be nullptr)
	 */
	static void Free(void* Original)
	{
		if (Original)
		{
			free(Original);
		}
	}

	/**
	 * Zero-initialize memory
	 * @param Dest - Pointer to memory to zero
	 * @param Size - Number of bytes to zero
	 */
	static void Memzero(void* Dest, size_t Size)
	{
		if (Dest && Size > 0)
		{
			memset(Dest, 0, Size);
		}
	}

	/**
	 * Copy memory
	 * @param Dest - Destination pointer
	 * @param Src - Source pointer
	 * @param Size - Number of bytes to copy
	 */
	static void Memcpy(void* Dest, const void* Src, size_t Size)
	{
		if (Dest && Src && Size > 0)
		{
			memcpy(Dest, Src, Size);
		}
	}
};
