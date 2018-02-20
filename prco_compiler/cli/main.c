/*
MIT License

Copyright (c) 2018 Ben Lancaster

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifdef _MSC_VER
#include "getopt.h"
#else
#include <getopt.h>
#endif


#include <libprco/dbug.h>
#include <libprco/parser.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    // Compiler variables
    char *              src_name        = NULL;
    int                 src_size;
    char *              out_name        = "out.s";
    int                 parse_result    = 0;
    FILE *              file_output     = NULL;
    struct text_parser  *parser         = NULL;
    int                 module_dump     = 0;

    // Parse command line
    int opt;
    while((opt = getopt(argc, argv, "i:dD:")) != -1) {
        switch(opt) {
            case 'i': src_name = optarg; break;
            case 'D': {
                char *ptr;
                set_dbug_level(strtoul(optarg, &ptr, 16));
            } break;
            case 'd': module_dump = 1; break;
            default:
                dprintf(D_ERR, "Unknown arguments.\r\n");
                exit(1);
        }
    }

    if(!src_name) {
        dprintf(D_ERR, "-i parameter missing.\r\n");
        return 2;
    }

    file_output = fopen(out_name, "w");
    
    // Create top level parser
    parser = parser_fopen(src_name, &src_size, NULL);
    if(!parser) {
        dprintf(D_ERR, "parser_fopen failed!\r\n");
        return 3;
    }
    dprintf(D_INFO, "File size: %d\r\n", src_size);

    parser_test(parser);
    return 0;

    // Run the parser
    parse_result = parser_run(parser);
    if(parse_result != 0) {
        dprintf(D_ERR, "parser_run error %d\r\n", parse_result);
        return parse_result;
    }


    return 0;
}