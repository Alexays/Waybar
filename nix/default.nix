{ lib
, waybar
, version
}:

waybar.overrideAttrs (prev: {
  inherit version;

  src = lib.cleanSourceWith {
    filter = name: type: type != "regular" || !lib.hasSuffix ".nix" name;
    src = lib.cleanSource ../.;
  };
})
