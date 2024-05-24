{ lib
, pkgs
, waybar
, version
}:
let
  libcava = rec {
    version = "0.10.2";
    src = pkgs.fetchFromGitHub {
      owner = "LukashonakV";
      repo = "cava";
      rev = version;
      hash = "sha256-jU7RQV2txruu/nUUl0TzjK4nai7G38J1rcTjO7UXumY=";
    };
  };
in
(waybar.overrideAttrs (
  oldAttrs: {
    inherit version;

    src = lib.cleanSourceWith {
      filter = name: type: type != "regular" || !lib.hasSuffix ".nix" name;
      src = lib.cleanSource ../.;
    };

    mesonFlags = lib.remove "-Dgtk-layer-shell=enabled" oldAttrs.mesonFlags;

    buildInputs = (builtins.filter (p: p.pname != "wireplumber") oldAttrs.buildInputs) ++ [
        pkgs.wireplumber
    ];

    postUnpack = ''
      pushd "$sourceRoot"
      cp -R --no-preserve=mode,ownership ${libcava.src} subprojects/cava-${libcava.version}
      patchShebangs .
      popd
    '';
  }
))
