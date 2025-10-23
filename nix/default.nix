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
      # NOTE: Needs to match the cava.wrap
      rev = "23efcced43b5a395747b18a2e5f2171fc0925d18";
      hash = "sha256-CNspaoK5KuME0GfaNijpC24BfALngzNi04/VNwPqMvo=";
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
    cp -R --no-preserve=mode,ownership ${libcava.src} subprojects/cava
    patchShebangs .
    popd
  '';
})
