/*
    whereami - programmer's orientation tool by Edwin Steiner

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org>
 */

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <cstdint>
#include <cinttypes>
#include <cassert>
#include <cctype>
#include <cerrno>

#ifdef WIN32
#include "windows.h"
#endif


namespace {
#ifdef WIN32
    void print_windows_system_error(FILE *file)
    {
        DWORD error = ::GetLastError();

        fprintf(file, ": (0x%08X) ", error);

        char buffer[1024] = { 0 };
        DWORD bufsize = (DWORD)min(sizeof(buffer), (size_t)65535);
        DWORD result = ::FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // dwFlags
            NULL, // lpSource
            error, // dwMessageId
            0, // dwLanguageId
            (LPSTR)buffer, // lpBuffer
            bufsize, // nSize
            NULL); // Arguments
        assert(result < sizeof(buffer));
        if (result > 0)
            fputs(buffer, file);
        else
            fputs("UNKNOWN ERROR (FormatMessage failed)", file);

        // only add the newline if FormatMessage did not supply it
        if (result == 0 || buffer[result - 1] != '\n')
            fputc('\n', file);
    }

    void exit_windows_system_error(const char *fmt, ...)
    {
        va_list vl;
        fputs("error: ", stderr);
        va_start(vl, fmt);
        vfprintf(stderr, fmt, vl);
        va_end(vl);
        print_windows_system_error(stderr);
        exit(EXIT_FAILURE);
    }
#else
    void exit_clib_error(const char *fmt, ...)
    {
        va_list vl;
        fputs("error: ", stderr);
        va_start(vl, fmt);
        vfprintf(stderr, fmt, vl);
        va_end(vl);
        #pragma warning (suppress : 4996) // no need for strerror_s
        fprintf(stderr, ": (%d) %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
#endif

    void exit_error(const char *fmt, ...)
    {
        va_list vl;
        va_start(vl, fmt);
        fprintf(stderr, "error: ");
        vfprintf(stderr, fmt, vl);
        exit(EXIT_FAILURE);
    }

    struct LineInfo {
        int32_t outer_index; //< index of the nearest preceeding line with strictly less indentation (or -1 if no such line)
        uint32_t indentation;
        uint32_t start_offset; //< offset of first non-indentation character from the beginning of the file
    };

    bool line_is_boring(LineInfo *line_info, char *text)
    {
        char *line_text = text + line_info->start_offset;
        return line_text[0] == '{';
    }

    struct Context {
        uint32_t index;
        char *text;
    };

    bool is_control_flow(Context *ctx)
    {
        if (strncmp(ctx->text, "if ", 3) == 0)
            return true;
        if (strncmp(ctx->text, "do ", 3) == 0)
            return true;
        if (strncmp(ctx->text, "for ", 4) == 0)
            return true;
        if (strncmp(ctx->text, "case ", 5) == 0)
            return true;
        if (strncmp(ctx->text, "while ", 6) == 0)
            return true;
        if (strncmp(ctx->text, "switch ", 7) == 0)
            return true;
        return false;
    }

    char *maybe_skip_substr(char *text)
    {
        if (strncmp(text, "namespace ", 10) == 0)
            text += 10;
        return text;
    }

    void print_context(Context *ctx)
    {
        bool is_control = is_control_flow(ctx);
        bool could_be_fn_name = !is_control;
        constexpr uint32_t max_ident_or_num_len = 6;
        constexpr uint32_t max_control_len = 20;
        char *text = ctx->text;
        char *next;
        while ((next = maybe_skip_substr(text)) > text)
            text = next;
        printf("..%u: ", 1 + ctx->index);
        char *ptr = text;
        uint32_t ident_or_num_len = 0;
        char before_space = 0;
        bool prev_was_space = false;
        uint32_t n_chars = 0;
        while (*ptr) {
            char ch = *ptr++;
            if (isspace(ch)) {
                ident_or_num_len = 0;
                prev_was_space = true;
                continue;
            }
            if (ch == '/' && *ptr == '/')
                break;
            if (isalnum(ch) || ch == '_') {
                if (prev_was_space && isalnum(before_space)) { // XXX isident
                    putc(' ', stdout);
                    n_chars++;
                }
                if (!could_be_fn_name && ident_or_num_len == max_ident_or_num_len) {
                    putc('$', stdout);
                    ch = '$';
                    n_chars++;
                }
                else if (could_be_fn_name || ident_or_num_len < max_ident_or_num_len) {
                    putc(ch, stdout);
                    n_chars++;
                }
                else
                    ch = '$';
                ident_or_num_len++;
            }
            else {
                putc(ch, stdout);
                n_chars++;
                if (ch == '(') {
                    if (could_be_fn_name)
                        break;
                    could_be_fn_name = false;
                }
                ident_or_num_len = 0;
            }
            prev_was_space = false;
            before_space = ch;
            if (is_control && n_chars >= max_control_len)
                break;
        }
    }
}

// parser state // XXX @Cleanup Maybe put this into a struct and pass that to parser functions

static uint32_t n_lines; //< number of lines in the input file
static uint32_t line; //< current line number, starting at 1
static uint32_t column; //< current column (up to the first non-whitespace character), starting at 0
static uint32_t tabsize; //< number of characters per TAB
static int32_t outer_index; //< index (line number - 1) of the surrounding context for the current line
static LineInfo *line_info_array; //< array with one LineInfo struct for each line
static LineInfo *line_info; //< points to the entry for the current line in line_info_array
// INVARIANT: line_info_array[outer_index].indentation shall always be initialized if outer_index >= 0.
static uint32_t prev_indentation;
static bool may_become_context;
static int32_t prev_valid_index; //< index (line number - 1) of the latest line we could potentially use as a context line

static void process_indentation_of_current_line(bool may_close_context)
{
    if (may_close_context && (column < prev_indentation)) {
        while (outer_index >= 0 && column <= line_info_array[outer_index].indentation) {
            assert(outer_index < (int32_t)n_lines);
            assert(prev_indentation >= line_info_array[outer_index].indentation);
            assert(line_info_array[outer_index].outer_index < outer_index);
            outer_index = line_info_array[outer_index].outer_index;
            prev_indentation = (outer_index >= 0) ? line_info_array[outer_index].indentation : 0;
            prev_valid_index = (line - 1);
        }
    }
    else if (may_become_context && (line > 1 && column > prev_indentation)) {
        assert(line >= 2);
        outer_index = prev_valid_index;
    }
    line_info->outer_index = outer_index;
    if (may_become_context || may_close_context) {
        prev_indentation = column;
        prev_valid_index = (line - 1);
    }
}

#define USAGE  "Usage: %s <SOURCEFILENAME> <LINE>\n\nLINE...line number for which to print whereami information, 0 means print all"

int main(int argc, char **argv)
{
    const char *progname = argv[0] ? argv[0] : "whereami";

    for (int i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (!arg)
            exit_error("null argument passed on the command line\n");
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "/?") == 0 || strcmp(arg, "/help") == 0) {
            printf(USAGE, progname);
            return 0;
        }
    }

    if (argc != 3)
        exit_error("expected two arguments on the command line (see usage)\n"
                   USAGE, progname);

    char *filename = argv[1];
    char *end = nullptr;
    uint32_t query_line = strtoul(argv[2], &end, 10);
    if (end && *end != 0)
        exit_error("expected a line number as the second command-line argument but got: %s\n",
                   argv[2]);

#ifdef WIN32
    HANDLE file = ::CreateFile(
            filename, // lpFileName
            GENERIC_READ, // dwDesiredAccess
            FILE_SHARE_READ, // dwShareMode
            NULL, // lpSecurityAttributes
            OPEN_EXISTING, // dwCreationDisposition
            FILE_FLAG_SEQUENTIAL_SCAN, // dwFlagsAndAttributes
            NULL); // hTemplateFile

    if (file == INVALID_HANDLE_VALUE)
        exit_windows_system_error("could not open file '%s'", filename);

    LARGE_INTEGER file_size_large_integer;
    BOOL result = ::GetFileSizeEx(file, &file_size_large_integer);
    if (!result)
        exit_windows_system_error("could not get file size");
    uint64_t file_size = file_size_large_integer.QuadPart;
#else
    #pragma warning (suppress : 4996) // gimme fopen
    FILE *file = fopen(filename, "rb");
    if (!file)
        exit_clib_error("could not open file '%s'", filename);
    int result = fseek(file, 0, SEEK_END);
    if (result != 0)
        exit_clib_error("could not seek the end of file '%s'", filename);
    int64_t file_size = (int64_t)ftell(file);
    if (file_size < 0)
        exit_clib_error("could not get the size of file '%s'", filename);
#endif

    if (file_size > UINT32_MAX)
        exit_error("File size %" PRIu64 " > %u bytes is not supported.\n",
                   file_size, UINT32_MAX);

    char *text = (char *)malloc(file_size + 2); // +1 for possible extra newline, +1 for terminating NUL
    if (!text)
        exit_error("Out-of-memory allocating buffer for file text (file_size = %" PRIu64 ")\n", file_size);

#ifdef WIN32
    DWORD n_bytes_read;
    result = ::ReadFile(file, text, (DWORD)file_size, &n_bytes_read, NULL);
    if (!result)
        exit_windows_system_error("Could not read file '%s'", filename);
#else
    result = fseek(file, 0, SEEK_SET);
    if (result != 0)
        exit_clib_error("could not seek the beginning of file '%s'", filename);
    uint32_t n_bytes_read = (uint32_t)fread(text, 1, file_size, file);
#endif

    if (n_bytes_read != file_size)
        exit_error("Reading file '%s' gave %u bytes instead of the expected %" PRIu64 ".\n",
                   filename, n_bytes_read, file_size);

    text[n_bytes_read] = 0;

#if WIN32
    result = ::CloseHandle(file);
    if (!result)
        exit_windows_system_error("Could not close file handle");
#else
    result = fclose(file);
    if (result == EOF)
        exit_clib_error("could not close file '%s'", filename);
#endif
    file = NULL;

    // Note: We only consider '\n' characters when counting newlines, so an '\r' without
    //       a following '\n' is not considered an end-of-line. see :CountingLines
    n_lines = 0;
    bool file_contains_a_nul_byte;
    {
        char *ptr;
        for (ptr = text; *ptr; ++ptr)
            if (*ptr == '\n')
                n_lines++;
        file_contains_a_nul_byte = (ptr < text + n_bytes_read);
    }

    // XXX DEBUG
    if (0) {
        fprintf(stderr, "file_size = %" PRIu64 ", n_bytes_read = %u, n_lines = %u, file_contains_a_nul_byte = %u, last byte of file: 0x%02x\n",
                file_size, n_bytes_read, n_lines, file_contains_a_nul_byte, file_size ? text[file_size - 1] : 0);
    }

    // Note: If the file contains a NUL byte, parsing will not reach the end of the file.
    if (!file_contains_a_nul_byte && file_size && text[file_size - 1] != '\n') {
        n_lines++; // extra line at the end, not terminated by a newline
        // add an extra newline so we do not have to treat this special case below
        text[n_bytes_read] = '\n';
        text[n_bytes_read + 1] = 0;
    }

    if (n_lines > INT32_MAX)
        exit_error("file has more lines (%u) than supported (%d)\n", n_lines, INT32_MAX);

    line_info_array = (LineInfo*) malloc(n_lines * sizeof(LineInfo));
    if (!line_info_array)
        exit_error("Out-of-memory allocating line info buffer.\n");

    {
        char *ptr = text;
        line = 1;
        column = 0;
        tabsize = 8;
        outer_index = -1;
        line_info = line_info_array;
        prev_indentation = 0;
        may_become_context = true;
        prev_valid_index = -1;
        while (*ptr) {
            assert(ptr <= text + file_size);
            char ch = *ptr++;
            switch (ch) {
                case '\n':
                    // whitespace-only line
whitespace_only_line:
                    line++;
                    assert(line_info < line_info_array + n_lines);
                    line_info->indentation = prev_indentation;
                    line_info->outer_index = outer_index;
                    line_info->start_offset = (uint32_t)(ptr - text) - 1;
                    line_info++;
                    may_become_context = true;
                    ptr[-1] = 0; // replace '\n' with terminating NUL
                    column = 0;
                    break;
                case '\t':
                    column++;
                    column = tabsize * ((column + tabsize - 1) / tabsize);
                    break;
                case ' ':
                    column++;
                    break;
                case '\r':
                    // replace by NUL, otherwise ignore
                    ptr[-1] = 0;
                    break;
                case '/':
                    if (ptr[0] == '*') {
                        // A C comment starts here. If it ends on the same line, we just
                        // skip it and consider the rest of the line normally.
                        // If it continues over a line break, skip the comment and the
                        // rest of the line after the closing '*/'.
                        ptr++;
                        line_info->indentation = column;
                        bool comment_contains_newline = false;
                        while (*ptr && (ptr[0] != '*' || ptr[1] != '/')) {
                            // XXX @Clarify Do we want to increase `column` in this loop?
                            if (ptr[0] == '\n') {
                                may_become_context = false;
                                process_indentation_of_current_line(!comment_contains_newline /* may_close_context */);
                                line_info->start_offset = (uint32_t)(ptr - text);
                                line_info->indentation = column;
                                line++;
                                line_info++;
                                ptr[0] = 0; // replace '\n' with line-terminating NUL
                                comment_contains_newline = true;
                            }
                            else if (ptr[0] == '\r') {
                                ptr[0] = 0; // replace '\r' with line-terminating NUL
                            }
                            ptr++;
                        }
                        if (ptr[0] == '*') {
                            // We found the terminating '*/'.
                            ptr += 2;
                            if (comment_contains_newline) {
                                line_info->start_offset = (uint32_t)(ptr - text);
                                line_info->indentation = prev_indentation;
                                goto skip_rest_of_line;
                            }
                            // skip whitespace without increasing column
                            while (*ptr == ' ' || *ptr == '\t' || *ptr == '\r') {
                                if (*ptr == '\r')
                                    *ptr = 0;
                                ptr++;
                            }
                            if (*ptr == '\n') {
                                ptr++;
                                goto whitespace_only_line;
                            }
                        }
                        if (ptr[0] == 0)
                            break; // terminate loop
                    }
                    goto first_nonwhitespace_character;
                case '#':
                    may_become_context = false;
                    // FALLTHROUGH
                default:
first_nonwhitespace_character:
                    if (ch < 0x20) {
                        fprintf(stderr, "%s:%u: warning: unexpected non-printable character 0x%02x encountered\n",
                                filename, line, (uint8_t)ch);
                    }

                    // we are at the first non-indentation character of a line
                    assert(line_info < line_info_array + n_lines);
                    line_info->indentation = column;
                    char *line_text = ptr - 1;
                    line_info->start_offset = (uint32_t)(line_text - text);

                    assert(outer_index < 0 || prev_indentation >= line_info_array[outer_index].indentation);

                    // XXX @Incomplete It would be good to report closing '}' braces still within the context that they close.

                    // If the line is only a C++ comment, do not consider it as a context line.
                    // Note: C++ comments after non-comment text will be dropped by the context printing code.
                    if (ch == '/' && ptr[0] == '/')
                        may_become_context = false;

                    // XXX @Incomplete handle C comments starting in the middle of the line

                    // check whether the line is of the form "identifier:" (with optional whitespace)
                    // if so, we do not turn it into a context line
                    // Note: This is to avoid using goto labels or "public:", "private:", etc. as context lines.
                    if (may_become_context) {
                        bool only_one_identifier = true;
                        bool seen_space = false;
                        bool seen_colon = false;
                        char *lookahead = ptr;
                        while (*lookahead && *lookahead != '\n' && (*lookahead != '\r' || lookahead[1] != '\n')) {
                            char ch = *lookahead;
                            if (seen_space || seen_colon) {
                                if ((!seen_colon || ch != ':') && !isspace(ch)) {
                                    only_one_identifier = false;
                                    break;
                                }
                            }
                            if (ch == ':')
                                seen_colon = true;
                            else if (isspace(ch))
                                seen_space = true;
                            else if (!isalnum(ch) && ch != '_') {
                                only_one_identifier = false;
                                break;
                            }
                            lookahead++;
                        }

                        if (seen_colon && only_one_identifier)
                            may_become_context = false;
                    }

                    // Don't consider 'case' labels as context lines.
                    // XXX @Clarify Case labels could make useful context lines but
                    //     if we consider them, we should consider goto labels, too,
                    //     for consistency. The problem is that goto labels are often
                    //     not meaningfully indented. Should we add some heuristics to
                    //     recognize goto labels that are at the same indentation level
                    //     on which we expect to see 'case' labels?
                    //     Another problem is that 'case's are often at the same level
                    //     as the surrounding 'switch' but we would really like to
                    //     have the switch as an outer context for the 'case's.
                    //     It seems we want some smart heuristics for goto/case labels.
                    //     For the time being, just ignore them.
                    // Note: 'default:' is handled by the goto label check above.
                    if (may_become_context && memcmp(ptr - 1, "case", 4) == 0 && isspace(ptr[3])) {
                        may_become_context = false;
                    }

skip_rest_of_line:
                    process_indentation_of_current_line(may_become_context /* may_close_context */);

                    // skip to end of line and replace line-terminating characters with NUL (if any)
                    // Note: A single '\r' without a following '\n' is not treated as an end-of-line.
                    //       (This is consistent with us not counting '\r' characters when determining n_lines.)
                    //       see :CountingLines
                    while (*ptr && *ptr != '\n' && (*ptr != '\r' || ptr[1] != '\n'))
                        ptr++;
                    if (*ptr == '\r')
                        *ptr++ = 0;
                    if (*ptr == '\n')
                        *ptr++ = 0;

                    assert(ptr <= text + file_size);

                    line++;
                    line_info++;
                    may_become_context = true;
                    column = 0;
                    break;
            }
        }

        assert((line_info - line_info_array) == n_lines);
    }

    uint32_t begin_index;
    uint32_t end_index;
    if (query_line) {
        begin_index = query_line - 1;
        end_index = query_line;
    }
    else {
        begin_index = 0;
        end_index = n_lines;
    }

    for (uint32_t index = begin_index; index < end_index; ++index) {
        LineInfo *line_info = line_info_array + index;
        if (!query_line)
            printf("%5u: %5u<- %2u: ", 1 + index, 1 + line_info->outer_index, line_info->indentation);

        uint32_t n_contexts = 0;
        Context *context_array = nullptr;
        Context *context_ptr = nullptr;
        // First pass: count contexts
        // Second pass: allocate and fill in contexts
        for (uint32_t i_pass = 0; i_pass < 2; ++i_pass) {
            if (i_pass == 1) {
                context_array = (Context *)malloc(n_contexts * sizeof(Context));
                if (!context_array)
                    exit_error("Out-of-memory allocating context array.\n");
            }

            context_ptr = context_array + n_contexts;
            for (LineInfo *outer = line_info; outer->outer_index >= 0; ) {
                outer = line_info_array + outer->outer_index;
                uint32_t indent = outer->indentation;
                while (outer > line_info_array && line_is_boring(outer, text)) {
                    outer--;
                    while (outer > line_info_array && outer->indentation > indent)
                        outer--;
                }
                if (i_pass == 0) {
                    n_contexts++;
                }
                else {
                    assert(context_ptr > context_array);
                    context_ptr--;
                    context_ptr->index = (uint32_t)(outer - line_info_array);
                    context_ptr->text = text + outer->start_offset;
                }
            }
        }
        assert(context_ptr >= context_array);
        uint32_t start_i = (uint32_t)(context_ptr - context_array);

        bool skipped_previous = false;
        for (uint32_t i = start_i; i < n_contexts; ++i) {
            Context *ctx = context_array + i;
            // XXX should only skip control flow?
            if (index - ctx->index < 20) {
                skipped_previous = true;
                continue;
            }
            print_context(ctx);
            skipped_previous = false;
        }
        if (skipped_previous)
            printf("...");
        if (!query_line)
            printf("\n");
        free(context_array);
        context_array = nullptr;
    }

    free(line_info_array);
    line_info_array = nullptr;
    free(text);
    text = nullptr;

    return 0;
}
