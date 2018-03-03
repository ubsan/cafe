module Error = Error
module Spanned = Cafec_spanned
module Untyped_ast = Cafec_parse.Ast
open Spanned.Prelude
open Error.Monad_spanned

let get_type _ctxt unt_ty =
  let module T = Untyped_ast.Type in
  let unt_ty, sp = unt_ty in
  let%bind (), _ = with_span sp in
  match unt_ty with T.Named name ->
    match name with
    | "unit" -> wrap Type.Unit
    | "bool" -> wrap Type.Bool
    | "int" -> wrap Type.Int
    | _ -> wrap_err (Error.Type_not_found unt_ty)


type builtin = Builtin_less_eq | Builtin_add | Builtin_sub

type decl = {params: (string * Type.t) list; ret_ty: Type.t}

(* TODO(ubsan): add spans *)
type expr =
  | Unit_literal
  | Bool_literal of bool
  | Integer_literal of int
  | If_else of (expr spanned * expr spanned * expr spanned)
  | Call of (expr spanned * expr spanned list)
  | Builtin of builtin
  | Global_function of func
  | Parameter of int

and func = {ty: decl spanned; expr: expr spanned}

type t =
  {types: (string * Type.t spanned) list; funcs: (string * func spanned) list}

let find_function_by_name {funcs; _} name =
  let rec helper = function
    | (name', func) :: _ when name = name' -> Some func
    | _ :: xs -> helper xs
    | [] -> None
  in
  helper funcs


let rec type_of_expr e =
  let e, sp = e in
  let%bind (), _ = with_span sp in
  match e with
  | Unit_literal -> wrap Type.Unit
  | Bool_literal _ -> wrap Type.Bool
  | Integer_literal _ -> wrap Type.Int
  | If_else (cond, e1, e2) -> (
      match%bind type_of_expr cond with
      | Type.Bool, _ ->
          let%bind t1, _ = type_of_expr e1 in
          let%bind t2, _ = type_of_expr e2 in
          if t1 = t2 then wrap t1
          else wrap_err (Error.If_branches_of_differing_type (t1, t2))
      | ty, _ -> wrap_err (Error.If_on_non_bool ty) )
  | Call (callee, args) -> (
      let%bind ty_callee, _ = type_of_expr callee in
      let%bind ty_args, _ =
        let rec helper = function
          | [] -> wrap []
          | x :: xs ->
              let%bind ty, _ = type_of_expr x in
              let%bind rest, _ = helper xs in
              wrap (ty :: rest)
        in
        helper args
      in
      match ty_callee with
      | Type.(Function {params; ret_ty}) ->
          if ty_args = params then wrap ret_ty
          else
            wrap_err
              (Error.Invalid_function_arguments
                 {expected= params; found= ty_args})
      | ty -> wrap_err (Error.Call_of_non_function ty) )
  | Builtin b -> (
    match b with
    | Builtin_add ->
        wrap Type.(Function {params= [Type.Int; Type.Int]; ret_ty= Type.Int})
    | Builtin_sub | Builtin_less_eq -> assert false )
  | Global_function f ->
      let params =
        let ty, _ = f.ty in
        List.map (fun (_, ty) -> ty) ty.params
      in
      let%bind ret_ty, _ = type_of_expr f.expr in
      wrap Type.(Function {params; ret_ty})
  | Parameter _ -> assert false


let find_parameter name lst =
  let rec helper name lst idx =
    match lst with
    | [] -> None
    | (name', ty) :: _ when name' = name -> Some (ty, idx)
    | _ :: xs -> helper name xs (idx + 1)
  in
  helper name lst 0


let rec type_expression decl ast unt_expr =
  let module E = Untyped_ast.Expr in
  match%bind Ok unt_expr with
  | E.Unit_literal, _ -> wrap Unit_literal
  | E.Bool_literal b, _ -> wrap (Bool_literal b)
  | E.Integer_literal i, _ -> wrap (Integer_literal i)
  | E.If_else (cond, thn, els), _ ->
      let%bind cond = type_expression decl ast cond in
      let%bind thn = type_expression decl ast thn in
      let%bind els = type_expression decl ast els in
      wrap (If_else (cond, thn, els))
  | E.Call (callee, args), _ ->
      let%bind callee = type_expression decl ast callee in
      let rec helper = function
        | [] -> wrap []
        | x :: xs ->
            let%bind x = type_expression decl ast x in
            let%bind xs, _ = helper xs in
            wrap (x :: xs)
      in
      let%bind args, _ = helper args in
      wrap (Call (callee, args))
  | E.Variable name, _ ->
      let {params; _} = decl in
      match find_parameter name params with
      | None -> (
        match find_function_by_name ast name with
        | None -> (
          match name with
          | "LESS_EQ" -> wrap (Builtin Builtin_less_eq)
          | "ADD" -> wrap (Builtin Builtin_add)
          | "SUB" -> wrap (Builtin Builtin_sub)
          | _ -> wrap_err (Error.Name_not_found name) )
        | Some (e, _) -> wrap (Global_function e) )
      | Some (_ty, idx) -> wrap (Parameter idx)


let add_function unt_func (ast: t) : (t, Error.t) spanned_result =
  let module F = Untyped_ast.Function in
  let {F.name; _}, sp = unt_func in
  let%bind func, _ =
    let%bind ty, ty_sp =
      let {F.params; F.ret_ty; _}, _ = unt_func in
      let rec get_params = function
        | [] -> wrap []
        | (name, ty) :: params ->
            let%bind ty, _ = get_type ast.types ty in
            let%bind params, _ = get_params params in
            wrap ((name, ty) :: params)
      in
      let%bind ret_ty, _ =
        match ret_ty with
        | None -> wrap Type.Unit
        | Some ty -> get_type ast.types ty
      in
      let%bind params, _ = get_params params in
      wrap {params; ret_ty}
    in
    let unt_func, _ = unt_func in
    match type_expression ty ast unt_func.F.expr with
    | Ok expr ->
        let%bind te, _ = type_of_expr expr in
        if te = ty.ret_ty then wrap {ty= (ty, ty_sp); expr}
        else
          wrap_err
            Error.(Return_type_mismatch {expected= ty.ret_ty; found= te})
    | Error e -> Error e
  in
  let ast_with_f = {ast with funcs= (name, (func, sp)) :: ast.funcs} in
  wrap ast_with_f


let make unt_ast =
  let module U = Untyped_ast in
  let empty_ast = wrap {funcs= []; types= []} in
  let rec helper ast = function
    | unt_func :: funcs ->
        let%bind ast, _ = ast in
        let new_ast = add_function unt_func ast in
        helper new_ast funcs
    | [] -> ast
  in
  (* 
   note(ubsan): this eventually won't be an issue
   it's currently O(n^2), but by the time n gets big enough,
   this should be rewritten
  *)
  let rec check_for_duplicates values =
    let rec helper v v_sp = function
      | (name, (_, sp)) :: _ when name = v ->
          Error
            ( Error.Defined_multiple_times {name; original_declaration= sp}
            , v_sp )
      | _ :: xs -> helper v v_sp xs
      | [] -> wrap ()
    in
    match values with
    | (name, (_, sp)) :: xs ->
        let%bind (), _ = helper name sp xs in
        check_for_duplicates xs
    | [] -> wrap ()
  in
  let%bind ret, _ = helper empty_ast unt_ast.U.funcs in
  let%bind (), _ = check_for_duplicates ret.funcs in
  let%bind (), _ = check_for_duplicates ret.types in
  wrap ret


type value =
  | Value_unit
  | Value_bool of bool
  | Value_integer of int
  | Value_function of func
  | Value_builtin of builtin

let run self =
  let rec eval args ctxt = function
    | Unit_literal -> Value_unit
    | Bool_literal b -> Value_bool b
    | Integer_literal n -> Value_integer n
    | If_else ((cond, _), (thn, _), (els, _)) -> (
      match eval args ctxt cond with
      | Value_bool true -> eval args ctxt thn
      | Value_bool false -> eval args ctxt els
      | _ -> assert false )
    | Parameter i -> List.nth_exn i args
    | Call ((e, _), args') -> (
      match eval args ctxt e with
      | Value_function func ->
          let expr, _ = func.expr in
          let args' = List.map (fun (e, _) -> eval args ctxt e) args' in
          eval args' ctxt expr
      | Value_builtin b ->
          let (lhs, _), (rhs, _) =
            match args' with [lhs; rhs] -> (lhs, rhs) | _ -> assert false
          in
          let lhs =
            match eval args ctxt lhs with
            | Value_integer v -> v
            | _ -> assert false
          in
          let rhs =
            match eval args ctxt rhs with
            | Value_integer v -> v
            | _ -> assert false
          in
          let ret =
            match b with
            | Builtin_add -> Value_integer (lhs + rhs)
            | Builtin_sub -> Value_integer (lhs - rhs)
            | Builtin_less_eq -> Value_bool (lhs <= rhs)
          in
          ret
      | _ -> assert false )
    | Builtin b -> Value_builtin b
    | Global_function i -> Value_function i
  in
  let rec find = function
    | (name, (func, _sp)) :: _ when name = "main" -> Some func
    | _ :: xs -> find xs
    | [] -> None
  in
  match find self.funcs with
  | None -> print_endline "main not defined"
  | Some main ->
      let main_expr, _ = main.expr in
      match eval [] self.funcs main_expr with
      | Value_integer n -> Printf.printf "main returned %d\n" n
      | Value_bool true -> print_endline "main returned true"
      | Value_bool false -> print_endline "main returned false"
      | _ -> assert false
