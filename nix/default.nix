{ lib
, waybar
, version
}:

waybar.overrideAttrs (prev: {
  inherit version;
  # version = "0.9.17";

  src = lib.cleanSourceWith {
    filter = name: type:
      let
        baseName = baseNameOf (toString name);
      in
        ! (
          lib.hasSuffix ".nix" baseName
        );
    src = lib.cleanSource ../.;
  };
})
