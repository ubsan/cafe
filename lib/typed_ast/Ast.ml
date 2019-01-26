module Stmt = Types.Ast_Stmt

module Binding = struct
  include Types.Ast_Binding

  let name (Binding r) = r.name

  let is_mut (Binding r) = r.is_mut

  let ty (Binding r) = r.ty
end

module Expr = struct
  include Types.Ast_Expr
  module Builtin = Types.Ast_Expr_Builtin

  module Local = struct
    include Types.Ast_Expr_Local

    let binding (Local r) = r.binding
  end

  let full_type (Expr {ty; _}) = ty

  let full_type_sp (Expr {ty; _}, _) = ty

  let base_type e = Type.value_type (full_type e)

  let base_type_sp e = Type.value_type (full_type_sp e)

  module Block = struct
    include Types.Ast_Expr_Block

    let expr (Block r) = r.expr

    let stmts (Block r) = r.stmts

    let base_type blk =
      match expr blk with
      | Some expr -> base_type_sp expr
      | None -> Types.Type.Builtin Types.Type.Unit

    let base_type_sp (blk, _) = base_type blk

    let full_type blk =
      match expr blk with
      | Some expr -> full_type_sp expr
      | None -> Type.Any (Type.Builtin Type.Unit)

    let full_type_sp (blk, _) = full_type blk
  end

  let unit_value =
    Expr {variant = Unit_literal; ty = Type.Any (Type.Builtin Type.Unit)}
end