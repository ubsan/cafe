module rec Value : sig
  type t =
    | Integer : int -> t
    | Tuple : t ref Array.t -> t
    | Function : Function_index.t -> t
    | Reference : Expr_result.Place.t -> t
    | Builtin : Cafec_Typed_ast.Expr.Builtin.t -> t
    | Constructor : int -> t
    | Variant : int * t ref -> t
    | Nilary_variant : int -> t
    | Record : t ref Array.t -> t
end =
  Value

and Function_index : sig
  type t = private int

  val of_int : int -> t
end = struct
  type t = int

  let of_int x = x
end

and Expr_result : sig
  module Place : sig
    type t = Place : {is_mut : bool; value : Value.t ref} -> t
  end

  type t =
    | Value : Value.t -> t
    | Place : Place.t -> t
end =
  Expr_result
