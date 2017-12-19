module Expr: {
  type t;
  let unit_literal: unit => t;
  let variable: string => t;
  let call: t => t;
};

module Type: {
  type t;
  let named: string => t;
};

module Type_definition: {
  type t;
  /*let alias: (string, Type.t) => t;*/
};

module Function: {type t; let make: (string, Expr.t) => t;};

type t;

let make: (array(Function.t), array(Type_definition.t)) => t;

let print: t => unit;

let run: t => unit;
