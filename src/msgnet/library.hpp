#pragma once

#ifdef _WIN32
#ifdef MSGNET_EXPORTS
#define MSGNET_API __declspec(dllexport)
#else
#define MSGNET_API __declspec(dllimport)
#endif
#else
#define MSGNET_API
#endif
