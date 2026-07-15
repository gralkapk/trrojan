#pragma once

#if (defined(_MSC_VER) && !defined(TRROJANOSPRAY_STATIC))

#ifdef TRROJANOSPRAY_EXPORTS
#define TRROJANOSPRAY_API __declspec(dllexport)
#else /* TRROJANOSPRAY_EXPORTS */
#define TRROJANOSPRAY_API __declspec(dllimport)
#endif /* TRROJANOSPRAY_EXPORTS*/

#else /* (defined(_MSC_VER) && !defined(TRROJANOSPRAY_STATIC)) */

#define TRROJANOSPRAY_API

#endif /* (defined(_MSC_VER) && !defined(TRROJANOSPRAY_STATIC)) */
