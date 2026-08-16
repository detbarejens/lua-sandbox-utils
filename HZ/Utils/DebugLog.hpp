#pragma once

#if defined(LICENSE_AUTH) && LICENSE_AUTH
#define MELLO_DBG(...) ((void)0)
#else
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <windows.h>

inline void HzDevLog(const char* fmt, ...)
{
	char buf[1024]{};
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	static FILE* file = nullptr;
	static bool inited = false;
	if (!inited)
	{
		inited = true;
		char path[MAX_PATH]{};
		if (GetModuleFileNameA(nullptr, path, MAX_PATH))
		{
			if (char* slash = strrchr(path, '\\'))
			{
				const size_t remain = static_cast<size_t>(MAX_PATH - (slash + 1 - path));
				strcpy_s(slash + 1, remain, "hz-dev.log");
				file = fopen(path, "a");
			}
		}
	}

	SYSTEMTIME st{};
	GetLocalTime(&st);
	if (file)
	{
		fprintf(file, "[%02u:%02u:%02u.%03u] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
		fflush(file);
	}
}

#define MELLO_DBG(fmt, ...) HzDevLog(fmt, ##__VA_ARGS__)
#endif
