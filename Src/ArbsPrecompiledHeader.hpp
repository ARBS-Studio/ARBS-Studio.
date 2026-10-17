#ifndef ARBS_PRECOMPILED_HEADER_HPP
#define ARBS_PRECOMPILED_HEADER_HPP

// GSNew.hpp dhe GSMalloc.hpp duhet te jene te parat (alokuesit e GSRoot)
#include <GSNew.hpp>
#include <GSMalloc.hpp>

#if defined (macintosh)
namespace std {
	void *GS_realloc (void *userData, size_t newSize);
}
#endif

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#if defined(WINDOWS)
#include "Win32Interface.hpp"
#endif

#include "APIEnvir.h"
#include "ACAPinc.h"

#endif // ARBS_PRECOMPILED_HEADER_HPP
