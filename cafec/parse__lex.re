open Pred;

open Spanned.Prelude;

open Spanned.Monad;

module Error = Parser_error;

exception Bug_lexer(string);

type t = {
  buffer: string,
  mutable index: int,
  mutable line: int,
  mutable column: int
};

let lexer = (s) => {buffer: s, index: 0, line: 1, column: 1};

/* the actual lexer functions */
let is_whitespace = (ch) => ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';

let is_alpha = (ch) => ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z';

let is_ident_start = (ch) => is_alpha(ch) || ch == '_';

let is_ident_continue = (ch) => is_ident_start(ch) || ch == '\'';

let is_operator_start = (ch) =>
  switch ch {
  | '!'
  | '#'
  | '$'
  | '%'
  | '&'
  | '*'
  | '+'
  | '-'
  | '.'
  | '/'
  | ':'
  | '<'
  | '='
  | '>'
  | '?'
  | '@'
  | '\\'
  | '^'
  | '|'
  | '~' => true
  | _ => false
  };

let is_operator_continue = (ch) => is_operator_start(ch);

let is_number_continue = (ch, base) =>
  switch base {
  | 2 => ch == '0' || ch == '1'
  | 8 => ch >= '0' && ch <= '7'
  | 10 => ch >= '0' && ch <= '9'
  | 16 => ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'F'
  | _ => raise(Bug_lexer("Invalid base: " ++ string_of_int(base)))
  };

let is_number_start = (ch) => ch >= '0' && ch <= '9';

let rec next_token: t => option(spanned(Token.t, Error.t)) =
  (lex) => {
    let current_span = () => {
      start_line: lex.line,
      start_column: lex.column,
      end_line: lex.line,
      end_column: lex.column + 1
    };
    let peek_ch = () =>
      if (String.length(lex.buffer) <= lex.index) {
        None;
      } else {
        Some((lex.buffer.[lex.index], current_span()));
      };
    let next_ch = () =>
      switch (peek_ch()) {
      | Some((ch, sp)) =>
        lex.index = lex.index + 1;
        if (ch == '\n') {
          lex.line = lex.line + 1;
          lex.column = 1;
        } else {
          lex.column = lex.column + 1;
        };
        Some((ch, sp));
      | None => None
      };
    let eat_ch = () => next_ch() |> ignore;
    let rec eat_whitespace = () =>
      switch (peek_ch()) {
      | Some((ch, _)) when is_whitespace(ch) =>
        eat_ch();
        eat_whitespace();
      | Some(_)
      | None => ()
      };
    let lex_ident = (fst, sp) => {
      open String_buffer;
      let buff = with_capacity(16);
      let rec helper = (idx, sp) =>
        switch (peek_ch()) {
        | Some((ch, sp')) when is_ident_continue(ch) =>
          eat_ch();
          push(buff, ch);
          helper(idx + 1, Spanned.union(sp, sp'));
        | Some(_)
        | None =>
          let kw = (k) => SOk(Token.Keyword(k), sp);
          switch (to_string(buff)) {
          | "true" => kw(Keyword_true)
          | "false" => kw(Keyword_false)
          | "if" => kw(Keyword_if)
          | "else" => kw(Keyword_else)
          | "func" => kw(Keyword_func)
          | "let" as res => SErr(Error.Reserved_token(res), sp)
          | "type" as res => SErr(Error.Reserved_token(res), sp)
          | "struct" as res => SErr(Error.Reserved_token(res), sp)
          | "variant" as res => SErr(Error.Reserved_token(res), sp)
          | id => SOk(Token.Identifier(id), sp)
          };
        };
      push(buff, fst);
      helper(1, sp);
    };
    let lex_operator = (fst, sp) => {
      open String_buffer;
      let buff = with_capacity(8);
      let rec helper = (idx, sp) =>
        switch (peek_ch()) {
        | Some((ch, sp')) when is_operator_continue(ch) =>
          eat_ch();
          push(buff, ch);
          helper(idx + 1, Spanned.union(sp, sp'));
        | Some(_)
        | None =>
          switch (to_string(buff)) {
          | "|" as res => SErr(Error.Reserved_token(res), sp)
          | "." as res => SErr(Error.Reserved_token(res), sp)
          | op => SOk(Token.Operator(op), sp)
          }
        };
      push(buff, fst);
      helper(1, sp);
    };
    let lex_number = (fst, sp) => {
      open String_buffer;
      let buff = with_capacity(22);
      let (base, idx, sp) =
        if (fst == '0') {
          switch (peek_ch()) {
          | Some(('x', sp')) =>
            eat_ch();
            (16, 0, Spanned.union(sp, sp'));
          | Some(('o', sp')) =>
            eat_ch();
            (8, 0, Spanned.union(sp, sp'));
          | Some(('b', sp')) =>
            eat_ch();
            (2, 0, Spanned.union(sp, sp'));
          | Some(_)
          | None =>
            push(buff, '0');
            (10, 1, sp);
          };
        } else {
          push(buff, fst);
          (10, 1, sp);
        };
      let rec helper = (idx, sp, space_allowed) =>
        switch (peek_ch()) {
        | Some((ch, sp')) when is_number_continue(ch, base) =>
          eat_ch();
          push(buff, ch);
          helper(idx + 1, Spanned.union(sp, sp'), true);
        | Some((' ', sp')) =>
          eat_ch();
          push(buff, ' ');
          helper(idx, Spanned.union(sp, sp'), false);
        | Some((ch, sp'))
            when space_allowed && (is_number_continue(ch, 10) || is_ident_continue(ch)) =>
          eat_ch();
          push(buff, ch);
          SErr(Error.Malformed_number_literal(to_string(buff)), Spanned.union(sp, sp'));
        | Some(_)
        | None =>
          let char_to_int = (ch) =>
            if (ch >= '0' && ch <= '9') {
              Char.code(ch) - Char.code('0');
            } else if (ch >= 'a' && ch <= 'f') {
              Char.code(ch) - Char.code('a') + 10;
            } else if (ch >= 'A' && ch <= 'F') {
              Char.code(ch) - Char.code('A') + 10;
            } else {
              assert false;
            };
          /* TODO(ubsan): fix overflow */
          let rec to_int = (idx, acc) =>
            if (idx < length(buff)) {
              let ch = get(buff, idx);
              if (ch == ' ') {
                to_int(idx + 1, acc);
              } else {
                to_int(idx + 1, acc * base + char_to_int(ch));
              };
            } else {
              acc;
            };
          SOk(Token.Int_literal(to_int(0, 0)), sp);
        };
      helper(idx, sp, true);
    };
    let rec block_comment: span => spanned(unit, Error.t) =
      (sp) => {
        let rec eat_the_things = () =>
          switch (next_ch()) {
          | Some(('*', _)) =>
            switch (next_ch()) {
            | Some(('/', _)) => pure()
            | _ => eat_the_things()
            }
          | Some(('/', sp')) =>
            switch (next_ch()) {
            | Some(('*', _)) => block_comment(sp')
            | _ => eat_the_things()
            }
          | Some(_) => eat_the_things()
          | None => SErr(Error.Unclosed_comment, Spanned.union(sp, current_span()))
          };
        eat_the_things();
      };
    let line_comment = () => {
      let rec eat_the_things = () =>
        switch (next_ch()) {
        | Some(('\n', _))
        | None => ()
        | Some(_) => eat_the_things()
        };
      eat_the_things();
    };
    eat_whitespace();
    switch (next_ch()) {
    | Some(('/', sp)) =>
      switch (peek_ch()) {
      | Some(('*', _)) =>
        eat_ch();
        switch (block_comment(sp)) {
        | SOk((), _) => next_token(lex)
        | SErr(e, sp) => Some(SErr(e, sp))
        };
      | Some(('/', _)) =>
        eat_ch();
        line_comment();
        next_token(lex);
      | _ => Some(SErr(Error.Unrecognized_character('/'), sp))
      }
    | Some(('(', sp)) => Some(SOk(Token.Open_paren, sp))
    | Some((')', sp)) => Some(SOk(Token.Close_paren, sp))
    | Some(('{', sp)) => Some(SOk(Token.Open_brace, sp))
    | Some(('}', sp)) => Some(SOk(Token.Close_brace, sp))
    | Some(('[', sp)) => Some(SErr(Error.Reserved_token("["), sp))
    | Some((']', sp)) => Some(SErr(Error.Reserved_token("]"), sp))
    | Some((';', sp)) => Some(SOk(Token.Semicolon, sp))
    | Some((',', sp)) => Some(SOk(Token.Comma, sp))
    | Some((ch, sp)) when is_ident_start(ch) => Some(lex_ident(ch, sp) >>= ((id) => pure(id)))
    | Some((ch, sp)) when is_operator_start(ch) =>
      Some(lex_operator(ch, sp) >>= ((op) => pure(op)))
    | Some((ch, sp)) when is_number_start(ch) => Some(lex_number(ch, sp) >>= ((n) => pure(n)))
    | Some((ch, sp)) => Some(SErr(Error.Unrecognized_character(ch), sp))
    | None => None
    };
  };

let iter = (lex) => Iter.from_next(() => next_token(lex));