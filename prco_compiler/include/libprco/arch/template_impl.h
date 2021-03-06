//
// Created by BDL on 03/03/2018.
//

#ifndef LIBPRCO_TOP_TEMPLATE_IMPL_H
#define LIBPRCO_TOP_TEMPLATE_IMPL_H

#include "target.h"

void cg_target_template_init(struct target_delegate *dt);

extern struct ast_func *cg_cur_function;

extern void cg_expr_template            (struct ast_item *e);
extern void cg_function_template        (struct ast_func *f);
extern void cg_bin_template             (struct ast_bin *b);
extern void cg_number_template          (struct ast_num *n);
extern void cg_local_decl_template      (struct ast_lvar *v);
extern void cg_call_template            (struct ast_call *c);
extern void cg_precode_template         (void);
extern void cg_postcode_template        (void);
extern void cg_if_template              (struct ast_if *v);

extern void cg_var_template             (struct ast_var *v);

extern void cg_assignment_template      (struct ast_assign *a);
extern void cg_var_ref_template         (struct ast_lvar *v);
extern void cg_local_decl_template      (struct ast_lvar *v);

extern void cg_if_template              (struct ast_if *i);
extern void cg_for_template             (struct ast_for *f);
extern void cg_while_template             (struct ast_while *v);

extern void cg_port_uart_template       (struct ast_expr *v);

extern void cg_cstring_ref              (struct ast_cstring *v);
extern void cg_deref_template           (struct ast_deref *v);

/*
extern void cg_call_template(struct ast_call *v);
extern void cg_dir_extern_template(ast_proto_t *p);
*/
extern int get_dt_size_template         (target_datatype dt);

extern void cg_sf_start                 (struct ast_func *f);


#endif //LIBPRCO_TOP_TEMPLATE_IMPL_H
