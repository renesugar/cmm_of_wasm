open Util.Names

type t = { name: Libwasm.Ast.name option; id : int ; ty: Libwasm.Types.func_type }
let count = ref (-1)
let create ~name ~ty =
  incr count;
  { name; id = !count ; ty}

let print ppf t =
  match t.name with
  | None -> Format.fprintf ppf "f%i" t.id
  | Some name -> Format.fprintf ppf "%s" (name_to_string name)

let is_named t name =
  match t.name with
  | None -> false
  | Some tname -> name_to_string tname = name

let name t =
  match t.name with
  | None -> None
  | Some name -> Some (name_to_string name)

let type_ f = f.ty

module M = struct
  type nonrec t = t
  let compare a b = compare a.id b.id
end
module Map = struct
  include Map.Make(M)
  let print f ppf s =
    let elts ppf s = iter (fun id v ->
        Format.fprintf ppf "@ (@[%a@ %a@])" print id f v) s in
    Format.fprintf ppf "@[<1>{@[%a@ @]}@]" elts s
end