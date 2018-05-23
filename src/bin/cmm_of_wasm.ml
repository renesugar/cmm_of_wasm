open Libwasm.Ast
open Ir.Stackless
open Cmm

(* Entry point of the compiler *)
let trace = print_endline


let load_wasm_script filename =
  trace ("Loading (" ^ filename ^ ")...");
  let ic = open_in filename in
  try
    let lexbuf = Lexing.from_channel ic in
    trace "Parsing...";
    let script = Libwasm.(Parse.parse filename lexbuf Parse.Script) in
    close_in ic;
    script
  with | e -> close_in ic; raise e

let rec collect_modules commands =
  (* UGH. *)
  List.fold_left (fun acc (x: Libwasm.Script.command) -> 
    match x.it with
      | Module (name_opt, def) ->
          begin
            match def.it with
              | Textual module_ -> (name_opt, module_) :: acc
              | _ -> acc
          end
      | _ -> acc) [] commands |> List.rev

let print_module ast_mod =
  let open Libwasm in
  Print.module_ stdout !(Flags.width) ast_mod

let compile_module filename (_name_opt, module_) =
  print_module module_;
  (* Validate the module *)
  Libwasm.Valid.check_module module_;
  (* Generate stackless IR representation *)
  let ir = Ir.Genstackless.ir_module module_ in
  trace "IR Module: ";
  trace (Ir.Print_stackless.string_of_module ir);
  let cmm_phrases = Cmmcompile.Gencmm.compile_module ir in
  trace "CMM Phrases: ";
  List.iter (Printcmm.phrase Format.std_formatter) cmm_phrases;
  trace "Time to output ASM!";
  Build_utils.build ~name:filename ~out_dir:"." cmm_phrases

let frontend filename =
  load_wasm_script filename
  |> collect_modules
  |> List.iteri (fun i -> compile_module (filename ^ (string_of_int i)) )

let () =
  Command_line.setup ();
  let filename = Command_line.filename () in
  frontend filename

