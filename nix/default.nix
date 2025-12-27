{
  lib,
  pkgs,
  waybar,
  version,
}:
let
  libcava = {
    src = pkgs.fetchFromGitHub {
      owner = "LukashonakV";
      repo = "cava";
      # NOTE: Needs to match the libcava.wrap
      rev = "v0.10.7-beta";
      hash = "sha256-IX1B375gTwVDRjpRfwKGuzTAZOV2pgDWzUd4bW2cTDU=";
    };
  };
in
waybar.overrideAttrs (oldAttrs: {
  inherit version;

  src = lib.cleanSourceWith {
    filter = name: type: type != "regular" || !lib.hasSuffix ".nix" name;
    src = lib.cleanSource ../.;
  };

  mesonFlags = lib.remove "-Dgtk-layer-shell=enabled" oldAttrs.mesonFlags;

  # downstream patch should not affect upstream
  patches = [ ];
  # nixpkgs checks version, no need when building locally
  nativeInstallCheckInputs = [ ];

  buildInputs =
    (builtins.filter (p: p.pname != "wireplumber" && p.pname != "gps") oldAttrs.buildInputs)
    ++ [
      pkgs.wireplumber
      pkgs.gpsd
    ];

  postUnpack = ''
    pushd "$sourceRoot"
    cp -R --no-preserve=mode,ownership ${libcava.src} subprojects/cava-0.10.7-beta
    patchShebangs .
    popd
  '';
})
