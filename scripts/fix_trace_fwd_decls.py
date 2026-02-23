#!/usr/bin/env python3
"""Fix trace forward declarations corrupted by migrate_trace_ctx.py.

Many .c files have local `extern void trace...(NULL, ...)` forward declarations
that were incorrectly modified by the migration script.  This script:

1. Removes ALL local `extern void traceXXX(...)` forward declarations
2. Adds `#include "mrtklib/mrtk_trace.h"` if not already present

Files to process: all .c files under src/ that have broken trace forward decls.
Excludes: mrtk_trace.c, mrtk_context.c (implementation files)
"""
import os
import re

PROJ = "/Volumes/SDSSDX3N-2T00-G26/dev/MRTKLIB"

SKIP_FILES = {"mrtk_trace.c", "mrtk_trace.h", "mrtk_context.c", "mrtk_context.h"}

# Pattern to match trace forward declarations (both correct and broken)
# Examples:
#   extern void trace(NULL,int level, const char *format, ...);
#   extern void trace(int level, const char *format, ...);
#   extern void trace   (NULL,int level, const char *format, ...);
#   extern void traceclose(void);
RE_TRACE_DECL = re.compile(
    r'^extern\s+void\s+trace\w*\s*\(.*\)\s*;\s*$'
)

TRACE_INCLUDE = '#include "mrtklib/mrtk_trace.h"'


def process_file(filepath):
    with open(filepath, 'r') as f:
        lines = f.readlines()

    # First pass: identify lines to remove and check for existing include
    remove_indices = set()
    has_trace_include = False
    last_include_idx = -1

    for i, line in enumerate(lines):
        stripped = line.strip()

        if TRACE_INCLUDE in stripped:
            has_trace_include = True

        if stripped.startswith('#include'):
            last_include_idx = i

        if RE_TRACE_DECL.match(stripped):
            remove_indices.add(i)

    if not remove_indices:
        return 0

    # Build new content: remove forward decls, add include if needed
    new_lines = []
    insert_done = False

    for i, line in enumerate(lines):
        if i in remove_indices:
            continue

        new_lines.append(line)

        # Insert #include right after the last #include line
        if not has_trace_include and not insert_done and i == last_include_idx:
            new_lines.append(TRACE_INCLUDE + '\n')
            insert_done = True

    # Clean up consecutive blank lines left by removal
    cleaned = []
    prev_blank = False
    for line in new_lines:
        is_blank = line.strip() == ''
        if is_blank and prev_blank:
            continue
        cleaned.append(line)
        prev_blank = is_blank

    with open(filepath, 'w') as f:
        f.writelines(cleaned)

    return len(remove_indices)


def find_c_files(proj):
    files = []
    for dirpath, dirnames, filenames in os.walk(proj):
        dirnames[:] = [d for d in dirnames if not d.startswith('.') and d != 'build']
        for f in filenames:
            if f in SKIP_FILES:
                continue
            if f.endswith('.c'):
                rel = os.path.relpath(os.path.join(dirpath, f), proj)
                if rel.startswith(('src/', 'apps/')):
                    files.append(os.path.join(dirpath, f))
    return sorted(files)


def main():
    files = find_c_files(PROJ)
    total = 0
    modified = 0

    for filepath in files:
        n = process_file(filepath)
        if n > 0:
            rel = os.path.relpath(filepath, PROJ)
            print(f"  {n:2d} decls removed: {rel}")
            total += n
            modified += 1

    print(f"\nDone. {total} forward declarations removed from {modified} files.")
    print(f"Replaced with #include \"mrtklib/mrtk_trace.h\" where needed.")


if __name__ == "__main__":
    main()
