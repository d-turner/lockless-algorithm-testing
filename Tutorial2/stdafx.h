//
//  stdafx.h
//

#pragma once

#ifdef WIN32
#include "targetver.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include <tchar.h>
#include <Windows.h>
#endif
#include "helper.h"
const int MAXTHREADS = 2 * ncpu;

// eof