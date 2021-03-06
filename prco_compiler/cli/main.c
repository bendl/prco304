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
#include <libprco/adt/ast.h>
#include <libprco/module.h>
#include <include/libprco/opt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIBPRCO_COMPILER_VERSION 2.60

int
main(int argc, char **argv)
{
        // Compiler variables
        struct module           *module      = NULL;
        struct text_parser      *parser      = NULL;
        FILE                    *file_output = NULL;

        char *src_name = NULL;
        int src_size;
        int O = 0;
        char *out_name = "out.s";

        int parse_result = 0;
        int module_dump_output = 0;
        struct ast_func *function_list = NULL;

        // Parse command line
        int opt;
        while ((opt = getopt(argc, argv, "i:dD:O:")) != -1) {
                switch (opt) {
                case 'i': src_name = optarg;
                        break;
                case 'D': {
                        char *ptr;
                        set_dbug_level(strtoul(optarg, &ptr, 16));
                }
                        break;
                case 'd': module_dump_output = 1;
                        break;
                case 'O':
                        O = atoi(optarg);
                        dbprintf(D_INFO, "Setting Optimiser to O%d\r\n", O);
                        break;
                default:
                        dbprintf(D_ERR, "Unknown arguments.\r\n");
                        exit(1);
                }
        }

        if (!src_name) {
                dbprintf(D_ERR, "-i parameter missing.\r\n");
                parse_result = 2;
                goto bad_exit;
        }

        dbprintf(D_INFO, "LIBPRCO Compiler. Version %f\r\n\r\n",
                LIBPRCO_COMPILER_VERSION);


        // Create output file for writing
        file_output = fopen(out_name, "w");
        g_file_out  = file_output;

        // Create new IR module
        module = new_module();

        // Create top level parser
        parser = parser_fopen(src_name, &src_size, NULL);
        if (!parser) {
                dbprintf(D_ERR, "parser_fopen failed!\r\n");
                parse_result = 3;
                goto main_exit;
        }

        // Run the parser
        parse_result = parser_run(parser);
        if (parse_result != 0) {
                dbprintf(D_ERR, "parser_run error %d\r\n", parse_result);
                goto main_exit;
        }

        if(module_dump_output) {
                // If using optimisations, apply them
                // to each function
                if(O > 0) {
                        function_list = module->functions;
                        list_for_each(function_list) {
                                AST_SELF_CF(function_list->body);
                        }
                }

                module_dump(module);
        }

main_exit:
        // Free the module
        module_free(module);
        fclose(g_file_out);

bad_exit:
        return parse_result;
}