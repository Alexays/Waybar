(
  import
  (
    builtins.fetchTarball {
      url = "https://github.com/edolstra/flake-compat/archive/99f1c2157fba4bfe6211a321fd0ee43199025dbf.tar.gz";
      sha256 = "0x2jn3vrawwv9xp15674wjz9pixwjyj3j771izayl962zziivbx2";
    }
  )
  {
    src = ./.;
  }
)
.defaultNix
