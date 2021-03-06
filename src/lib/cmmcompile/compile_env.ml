type t = {
  var_env : (Ir.Var.t, Ident.t) Hashtbl.t;
  label_env : (Ir.Label.Id.t, int) Hashtbl.t;
  const_env : (Ir.Var.t, Cmm.expression) Hashtbl.t;
  mutable label_count : int;
  module_name : string;
  memory: Ir.Stackless.memory option;
  table : Ir.Stackless.table option;
  mutable fuel_ident: Ident.t
}

let create ~module_name ~memory ~table = {
  var_env = Hashtbl.create 200;
  label_env = Hashtbl.create 100;
  const_env = Hashtbl.create 200;
  label_count = 0;
  module_name;
  memory;
  table;
  fuel_ident = Ident.create "fuel"
}

let reset env =
  Hashtbl.reset env.var_env;
  Hashtbl.reset env.label_env;
  Hashtbl.reset env.const_env;
  env.label_count <- 0;
  env.fuel_ident <- Ident.create "fuel"

let module_name env = env.module_name

let bind_var v i env = Hashtbl.replace env.var_env v i

let lookup_var v env = Hashtbl.find env.var_env v

let bind_label lbl env =
  let lbl_id = env.label_count in
  env.label_count <- env.label_count + 1;
  Hashtbl.replace env.label_env (Ir.Label.id lbl) lbl_id;
  lbl_id

let lookup_label lbl env =
  Hashtbl.find env.label_env (Ir.Label.id lbl)

let func_symbol func env =
  Ir.Func.symbol ~module_name:env.module_name func

let global_symbol glob env =
  Ir.Global.symbol ~module_name:env.module_name glob

let table_symbol env =
  match env.table with
    | Some (ImportedTable { module_name; table_name; _ }) ->
        Printf.sprintf "%s_table_%s" module_name table_name
    | _ -> env.module_name ^ "_internaltable"

let memory_symbol env =
  match env.memory with
    | Some (ImportedMemory { module_name; memory_name; _ }) ->
        Printf.sprintf "%s_memory_%s" module_name memory_name
    | _ -> env.module_name ^ "_internalmemory"

let add_constant var const env = Hashtbl.replace env.const_env var const

let fuel_ident env = env.fuel_ident

(* Tries to resolve a constant. If it's in the constant environment,
 * then returns the cached constant. *)
let resolve_variable ir_var env =
  match Hashtbl.find_opt env.const_env ir_var with
    | Some x -> x
    | None -> Cvar (lookup_var ir_var env)

