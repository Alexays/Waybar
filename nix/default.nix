{ lib
, pkgs
, waybar
, version
}:
let
  libcava = rec {
    version = "0.10.3";
    src = pkgs.fetchFromGitHub {
      owner = "LukashonakV";
      repo = "cava";
      rev = version;
      hash = "sha256-ZDFbI69ECsUTjbhlw2kHRufZbQMu+FQSMmncCJ5pagg=";
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

    # downstream patch should not affect upstream
    patches = [];
    # nixpkgs checks version, no need when building locally
    nativeInstallCheckInputs = [];

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
