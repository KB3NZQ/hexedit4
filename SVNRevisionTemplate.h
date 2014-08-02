#pragma once

// This file is used to get version control info into the program.
// A custom build step uses this as input and produces SVNRevision.h.

#define SVNRevision $WCREV$
#define SVNRevisionStr "$WCREV$"
char *SVNDate     = "$WCDATE$";

// This prevents the build unless all files are checked in
#if $WCMODS?1:0$
#error Please commit all files before creating a release build
#endif
