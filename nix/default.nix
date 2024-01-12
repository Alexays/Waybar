{ lib
, pkgs
, waybar
, version
}:
let
  catch2_3 = {
    src = pkgs.fetchFromGitHub
      {
        owner = "catchorg";
        repo = "Catch2";
        rev = "v3.5.1";
        hash = "sha256-OyYNUfnu6h1+MfCF8O+awQ4Usad0qrdCtdZhYgOY+Vw=";
      };
  };
in
(waybar.overrideAttrs (oldAttrs: rec {
  inherit version;

  src = lib.cleanSourceWith {
    filter = name: type: type != "regular" || !lib.hasSuffix ".nix" name;
    src = lib.cleanSource ../.;
  };
})
).override {
  catch2_3 = pkgs.catch2_3.overrideAttrs (oldAttrs: {
    version = "3.5.1";
    src = catch2_3.src;
  });
}
