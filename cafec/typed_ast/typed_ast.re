open Pred;

open Spanned.Prelude;

module Expr = {
  type builder = 
    | Unit_literal
    | Bool_literal(bool)
    | Integer_literal(int)
    | If_else(t, t, t)
    | Call(t, array(t))
    | Global_function(int)
    | Parameter(int)
  and t = spanned(builder);
};

module Type = {
  module Ctxt: {
    type context;
    let make_context: array(Untyped_ast.Type_declaration.t) => context;
  } = {
    type context = unit;
    let make_context = (_) => ();
  };
  include Ctxt;

  type builder =
    | Unit
    | Bool
    | Int
  and t = spanned(builder);

  let unit_ = (Unit, Spanned.made_up);
  let bool_ = (Bool, Spanned.made_up);
  let int_ = (Int, Spanned.made_up);

  /* NOTE(ubsan): this should be error handling */
  let make: (Untyped_ast.Type.t, context) => t = (unt_ty, _ctxt) => {
    module T = Untyped_ast.Type;
    switch unt_ty {
    | (T.Named(name), _) =>
      if (name == "unit") {
        unit_
      } else if (name == "bool") {
        bool_
      } else if (name == "int") {
        int_
      } else {
        assert false
      }
    }
  };
};

module Function = {
  type decl_builder = {
    params: array(Type.t),
    ret_ty: Type.t
  } and decl = spanned(decl_builder);
  type builder = {
    ty: decl,
    expr: Expr.t
  } and t = spanned(builder);

  type context = array((string, decl));

  let create_context
    : (Untyped_ast.t, Type.context) => result(context, spanned(Error.t))
    = (unt_ast, ty_ctxt) =>
  {
    let module U = Untyped_ast;
    let module F = U.Function;
    let err = ref(None);
    let ret = Array.init(
      Array.length(unt_ast.U.funcs),
      (i) => {
        let ({F.name, F.params, F.ret_ty, _}, sp) = unt_ast.U.funcs[i];
        for (j in 0 to i - 1) {
          let (decl, old_sp) = unt_ast.U.funcs[j];
          if (name == decl.F.name) {
            switch err^ {
              | None =>
                err := Some(
                  (Error.Multiple_function_definitions(name, old_sp), sp))
              | Some(_) => ()
            }
          }
        };
        let ret_ty = switch ret_ty {
        | Some(ty) => Type.make(ty, ty_ctxt)
        | None => Type.unit_
        };
        let params = Array.init(
          Array.length(params),
          (i) => {
            let (_name, param) = params[i];
            Type.make(param, ty_ctxt)
          });
        (name, ({params, ret_ty}, sp))
      });
    switch err^ {
    | Some((e, sp)) => Err((e, sp))
    | None => Ok(ret)
    }
  };
  let make = (_unt_func, _ctxt) => {
    assert false;
  };
};

type t = {
  funcs: array(Function.t)
};

let make = (_unt_ast) => {
  assert false;
};
