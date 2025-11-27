#pragma once
#include "Archive.h"
#include "UEContainer.h"
#include <fstream>

class FWindowsBinReader : public FArchive
{
public:
	FWindowsBinReader(const FString& Filename)
		: FArchive(true, false) // Loading 모드
	{
		File.open(Filename, std::ios::binary | std::ios::in);
	}

	~FWindowsBinReader() override
	{
		if (File.is_open())
		{
			File.close();
		}
	}

	// 파일이 성공적으로 열렸는지 확인하는 메서드
	bool IsOpen() const
	{
		return File.is_open();
	}

	void Serialize(void* Data, int64 Length) override
	{
		File.read(static_cast<char*>(Data), Length);
	}

	void Seek(size_t Position)
	{
		File.seekg(static_cast<std::streamoff>(Position), std::ios::beg);
	}

	size_t Tell()
	{
		return File.tellg();
	}

	bool Close() override
	{
		if (File.is_open())
		{
			File.close();
			return true;
		}
		return false;
	}

private:
	std::ifstream File;
};
