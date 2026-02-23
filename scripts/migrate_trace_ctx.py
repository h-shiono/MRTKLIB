#!/usr/bin/env python3
"""Migrate trace function calls to include mrtk_ctx_t *ctx as first argument.

For all .c files in the project (except mrtk_trace.c and mrtk_context.c),
this script inserts NULL as the first argument to every trace function call:

    trace(2, "msg")        -> trace(NULL,2, "msg")
    tracet(3, "msg")       -> tracet(NULL,3, "msg")
    traceopen("file")      -> traceopen(NULL,"file")
    traceclose()           -> traceclose(NULL)
    tracelevel(5)          -> tracelevel(NULL,5)
    tracemat(3, A, n, ...)  -> tracemat(NULL,3, A, n, ...)
    traceobs(3, obs, n)    -> traceobs(NULL,3, obs, n)
    tracenav(3, nav)       -> tracenav(NULL,3, nav)
    tracegnav(3, nav)      -> tracegnav(NULL,3, nav)
    tracehnav(3, nav)      -> tracehnav(NULL,3, nav)
    tracepeph(3, nav)      -> tracepeph(NULL,3, nav)
    tracepclk(3, nav)      -> tracepclk(NULL,3, nav)
    traceb(3, p, n)        -> traceb(NULL,3, p, n)
"""
import os
import re
import sys

PROJ = "/Volumes/SDSSDX3N-2T00-G26/dev/MRTKLIB"

# Files to skip (definitions/declarations handled manually)
SKIP_FILES = {"mrtk_trace.c", "mrtk_trace.h", "mrtk_context.c", "mrtk_context.h"}

# Trace functions that take arguments (insert NULL, before first arg)
TRACE_WITH_ARGS = [
    "tracepclk", "tracepeph", "tracegnav", "tracehnav",
    "tracenav", "traceobs", "tracemat",
    "traceopen", "tracelevel",
    "tracet", "traceb", "trace",
]

# Order matters: longer names first to avoid partial matches
# e.g. "tracet" before "trace", "tracenav" before "trace"
TRACE_WITH_ARGS.sort(key=len, reverse=True)

# traceclose takes no args -> traceclose(NULL)
RE_TRACECLOSE = re.compile(r'\btraceclose\s*\(\s*\)')

# Build regex for functions with args
# Match: word-boundary + funcname + ( + not already NULL
# We need to ensure we don't match function definitions (which have types before them)
# In call sites: the function name is preceded by whitespace, comma, or start-of-line
# In definitions: preceded by return type like "void" or "extern void"
# Safe approach: just match the call pattern and exclude mrtk_trace.c
def build_pattern(funcname):
    # Match: funcname( followed by anything that's NOT "NULL" or "mrtk_ctx_t"
    # This prevents double-application
    return re.compile(
        r'\b' + funcname + r'\s*\(\s*(?!NULL\b|mrtk_ctx_t\b|void\b)',
    )

PATTERNS = [(name, build_pattern(name)) for name in TRACE_WITH_ARGS]


def process_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    original = content
    changes = 0

    # Handle traceclose() -> traceclose(NULL)
    count = len(RE_TRACECLOSE.findall(content))
    if count > 0:
        content = RE_TRACECLOSE.sub('traceclose(NULL)', content)
        changes += count

    # Handle all other trace functions: insert NULL, after (
    for funcname, pattern in PATTERNS:
        def replacer(m):
            # m.group(0) is e.g. "trace(" or "trace( "
            # We need to insert NULL, right after the (
            text = m.group(0)
            # Find the position of ( in the match
            paren_idx = text.index('(')
            # Insert NULL, after (
            return text[:paren_idx+1] + "NULL," + text[paren_idx+1:]

        new_content, n = pattern.subn(replacer, content)
        if n > 0:
            content = new_content
            changes += n

    if content != original:
        with open(filepath, 'w') as f:
            f.write(content)
        return changes
    return 0


def find_c_files(proj):
    """Find all .c and .h files under src/, apps/, include/."""
    files = []
    for dirpath, dirnames, filenames in os.walk(proj):
        # Skip hidden dirs, build dir
        dirnames[:] = [d for d in dirnames if not d.startswith('.') and d != 'build']
        for f in filenames:
            if f in SKIP_FILES:
                continue
            if f.endswith('.c') or f.endswith('.h'):
                # Only process files under src/, apps/, include/
                rel = os.path.relpath(os.path.join(dirpath, f), proj)
                if rel.startswith(('src/', 'apps/', 'include/')):
                    files.append(os.path.join(dirpath, f))
    return sorted(files)


def main():
    files = find_c_files(PROJ)
    total_changes = 0
    modified_files = 0

    for filepath in files:
        n = process_file(filepath)
        if n > 0:
            rel = os.path.relpath(filepath, PROJ)
            print(f"  {n:4d} changes: {rel}")
            total_changes += n
            modified_files += 1

    print(f"\nDone. {total_changes} call sites updated across {modified_files} files.")


if __name__ == "__main__":
    main()
