#pragma once

#include <cstddef>

namespace GamePaths
{

void Init();
bool IsInitialized();
const char *GetUserPath();
bool IsJapanese();
bool IsAssetPath(const char *path);
void Resolve(char *outBuf, std::size_t outBufSize, const char *path);
void EnsureParentDir(const char *resolvedPath);

} // namespace GamePaths
